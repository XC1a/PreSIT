/*
 * Copyright (c) 2010-2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2013 Amin Farmahini-Farahani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mem/mem_ctrl.hh"

#include "base/trace.hh"
#include "debug/DRAM.hh"
#include "debug/Drain.hh"
#include "debug/MemCtrl.hh"
#include "debug/NVM.hh"
#include "debug/QOS.hh"
#include "mem/dram_interface.hh"
#include "mem/mem_interface.hh"
#include "mem/nvm_interface.hh"
#include "sim/system.hh"

#define VAULT_DEBUG false
bool trace_control = false;
uint64_t trace_cnt = 0;
uint64_t trace_global_cnt = 0;


namespace gem5
{

namespace memory
{

MemCtrl::MemCtrl(const MemCtrlParams &p) :
    qos::MemCtrl(p),
    port(name() + ".port", *this), isTimingMode(false),
    retryRdReq(false), retryWrReq(false),
    nextReqEvent([this] {processNextReqEvent(dram, respQueue,
                         respondEvent, nextReqEvent, retryWrReq);}, name()),
    respondEvent([this] {processRespondEvent(dram, respQueue,
                         respondEvent, retryRdReq); }, name()),
    //+++++++++++++++++ wxr
    EncEvent([this] {EncStateMachine();} , name()),
    hashHardwareEvent([this] {hashProcess();} , name()),
    //----------------- wxr
    dram(p.dram),
    readBufferSize(dram->readBufferSize),
    writeBufferSize(dram->writeBufferSize),
    writeHighThreshold(writeBufferSize * p.write_high_thresh_perc / 100.0),
    writeLowThreshold(writeBufferSize * p.write_low_thresh_perc / 100.0),
    minWritesPerSwitch(p.min_writes_per_switch),
    minReadsPerSwitch(p.min_reads_per_switch),
    writesThisTime(0), readsThisTime(0),
    memSchedPolicy(p.mem_sched_policy),
    frontendLatency(p.static_frontend_latency),
    backendLatency(p.static_backend_latency),
    commandWindow(p.command_window),
    prevArrival(0),
    stats(*this),
    encCache({{"nway",1}, {"size",64*1024}}),
    macCache({{"nway",1}, {"size",64*1024}}),
    vault_engine(VAULT_DEBUG,1024*_MEM),
    teePrediction(8,4,1024*_MEM),
    preRecord(),
    hashLock(),
    hashGroup(N_HASH),
    gapPre(PRE_GAP_NUM)
{
    DPRINTF(MemCtrl, "Setting up controller\n");

    readQueue.resize(p.qos_priorities);
    writeQueue.resize(p.qos_priorities);

    dram->setCtrl(this, commandWindow);

    // perform a basic check of the write thresholds
    if (p.write_low_thresh_perc >= p.write_high_thresh_perc)
        fatal("Write buffer low threshold %d must be smaller than the "
              "high threshold %d\n", p.write_low_thresh_perc,
              p.write_high_thresh_perc);
    if (p.disable_sanity_check) {
        port.disableSanityCheck();
    }
}

void
MemCtrl::init()
{
   if (!port.isConnected()) {
        fatal("MemCtrl %s is unconnected!\n", name());
    } else {
        port.sendRangeChange();
    }
}

void
MemCtrl::startup()
{
    // remember the memory system mode of operation
    isTimingMode = system()->isTimingMode();

    if (isTimingMode) {
        // shift the bus busy time sufficiently far ahead that we never
        // have to worry about negative values when computing the time for
        // the next request, this will add an insignificant bubble at the
        // start of simulation
        dram->nextBurstAt = curTick() + dram->commandOffset();
    }
}

Tick
MemCtrl::recvAtomic(PacketPtr pkt)
{
    if (!dram->getAddrRange().contains(pkt->getAddr())) {
        panic("Can't handle address range for packet %s\n", pkt->print());
    }

    return recvAtomicLogic(pkt, dram);
}


Tick
MemCtrl::recvAtomicLogic(PacketPtr pkt, MemInterface* mem_intr)
{
    DPRINTF(MemCtrl, "recvAtomic: %s 0x%x\n",
                     pkt->cmdString(), pkt->getAddr());

    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    // do the actual memory access and turn the packet into a response
    mem_intr->access(pkt);

    if (pkt->hasData()) {
        // this value is not supposed to be accurate, just enough to
        // keep things going, mimic a closed page
        // also this latency can't be 0
        return mem_intr->accessLatency();
    }

    return 0;
}

Tick
MemCtrl::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &backdoor)
{
    Tick latency = recvAtomic(pkt);
    dram->getBackdoor(backdoor);
    return latency;
}

bool
MemCtrl::readQueueFull(unsigned int neededEntries) const
{
    DPRINTF(MemCtrl,
            "Read queue limit %d, current size %d, entries needed %d\n",
            readBufferSize, totalReadQueueSize + respQueue.size(),
            neededEntries);

    auto rdsize_new = totalReadQueueSize + respQueue.size() + neededEntries;
    return rdsize_new > readBufferSize;
}

bool
MemCtrl::writeQueueFull(unsigned int neededEntries) const
{
    DPRINTF(MemCtrl,
            "Write queue limit %d, current size %d, entries needed %d\n",
            writeBufferSize, totalWriteQueueSize, neededEntries);

    auto wrsize_new = (totalWriteQueueSize + neededEntries);
    return  wrsize_new > writeBufferSize;
}

bool
MemCtrl::addToReadQueue(PacketPtr pkt,
                unsigned int pkt_count, MemInterface* mem_intr)
{
    // only add to the read queue here. whenever the request is
    // eventually done, set the readyTime, and call schedule()
    assert(!pkt->isWrite());

    assert(pkt_count != 0);

    // if the request size is larger than burst size, the pkt is split into
    // multiple packets
    // Note if the pkt starting address is not aligened to burst size, the
    // address of first packet is kept unaliged. Subsequent packets
    // are aligned to burst size boundaries. This is to ensure we accurately
    // check read packets against packets in write queue.
    const Addr base_addr = pkt->getAddr();
    Addr addr = base_addr;
    unsigned pktsServicedByWrQ = 0;
    BurstHelper* burst_helper = NULL;

    uint32_t burst_size = mem_intr->bytesPerBurst();

    for (int cnt = 0; cnt < pkt_count; ++cnt) {
        unsigned size = std::min((addr | (burst_size - 1)) + 1,
                        base_addr + pkt->getSize()) - addr;
        stats.readPktSize[ceilLog2(size)]++;
        stats.readBursts++;
        stats.requestorReadAccesses[pkt->requestorId()]++;

        // First check write buffer to see if the data is already at
        // the controller
        bool foundInWrQ = false;
        Addr burst_addr = burstAlign(addr, mem_intr);
        // if the burst address is not present then there is no need
        // looking any further
        if (isInWriteQueue.find(burst_addr) != isInWriteQueue.end()) {
            for (const auto& vec : writeQueue) {
                for (const auto& p : vec) {
                    // check if the read is subsumed in the write queue
                    // packet we are looking at
                    if (p->addr <= addr &&
                       ((addr + size) <= (p->addr + p->size))) {

                        foundInWrQ = true;
                        stats.servicedByWrQ++;
                        pktsServicedByWrQ++;
                        DPRINTF(MemCtrl,
                                "Read to addr %#x with size %d serviced by "
                                "write queue\n",
                                addr, size);
                        stats.bytesReadWrQ += burst_size;
                        break;
                    }
                }
            }
        }

        // If not found in the write q, make a memory packet and
        // push it onto the read queue
        if (!foundInWrQ) {

            // Make the burst helper for split packets
            if (pkt_count > 1 && burst_helper == NULL) {
                DPRINTF(MemCtrl, "Read to addr %#x translates to %d "
                        "memory requests\n", pkt->getAddr(), pkt_count);
                burst_helper = new BurstHelper(pkt_count);
            }

            MemPacket* mem_pkt;
            mem_pkt = mem_intr->decodePacket(pkt, addr, size, true,
                                                    mem_intr->pseudoChannel);

            // Increment read entries of the rank (dram)
            // Increment count to trigger issue of non-deterministic read (nvm)
            mem_intr->setupRank(mem_pkt->rank, true);
            // Default readyTime to Max; will be reset once read is issued
            mem_pkt->readyTime = MaxTick;
            mem_pkt->burstHelper = burst_helper;

            assert(!readQueueFull(1));
            stats.rdQLenPdf[totalReadQueueSize + respQueue.size()]++;

            DPRINTF(MemCtrl, "Adding to read queue\n");

            readQueue[mem_pkt->qosValue()].push_back(mem_pkt);

            // log packet
            logRequest(MemCtrl::READ, pkt->requestorId(),
                       pkt->qosValue(), mem_pkt->addr, 1);

            // Update stats
            stats.avgRdQLen = totalReadQueueSize + respQueue.size();
        }

        // Starting address of next memory pkt (aligned to burst boundary)
        addr = (addr | (burst_size - 1)) + 1;
    }

    // If all packets are serviced by write queue, we send the repsonse back
    if (pktsServicedByWrQ == pkt_count) {
        accessAndRespond(pkt, frontendLatency, mem_intr);
        return true;
    }

    // Update how many split packets are serviced by write queue
    if (burst_helper != NULL)
        burst_helper->burstsServiced = pktsServicedByWrQ;

    // not all/any packets serviced by the write queue
    return false;
}

void
MemCtrl::addToWriteQueue(PacketPtr pkt, unsigned int pkt_count,
                                MemInterface* mem_intr)
{
    // only add to the write queue here. whenever the request is
    // eventually done, set the readyTime, and call schedule()
    assert(pkt->isWrite());

    // if the request size is larger than burst size, the pkt is split into
    // multiple packets
    const Addr base_addr = pkt->getAddr();
    Addr addr = base_addr;
    uint32_t burst_size = mem_intr->bytesPerBurst();

    for (int cnt = 0; cnt < pkt_count; ++cnt) {
        unsigned size = std::min((addr | (burst_size - 1)) + 1,
                        base_addr + pkt->getSize()) - addr;
        stats.writePktSize[ceilLog2(size)]++;
        stats.writeBursts++;
        stats.requestorWriteAccesses[pkt->requestorId()]++;

        // see if we can merge with an existing item in the write
        // queue and keep track of whether we have merged or not
        bool merged = isInWriteQueue.find(burstAlign(addr, mem_intr)) !=
            isInWriteQueue.end();

        // if the item was not merged we need to create a new write
        // and enqueue it
        if (!merged) {
            MemPacket* mem_pkt;
            mem_pkt = mem_intr->decodePacket(pkt, addr, size, false,
                                                    mem_intr->pseudoChannel);
            // Default readyTime to Max if nvm interface;
            //will be reset once read is issued
            mem_pkt->readyTime = MaxTick;

            mem_intr->setupRank(mem_pkt->rank, false);

            assert(totalWriteQueueSize < writeBufferSize);
            stats.wrQLenPdf[totalWriteQueueSize]++;

            DPRINTF(MemCtrl, "Adding to write queue\n");

            writeQueue[mem_pkt->qosValue()].push_back(mem_pkt);
            isInWriteQueue.insert(burstAlign(addr, mem_intr));

            // log packet
            logRequest(MemCtrl::WRITE, pkt->requestorId(),
                       pkt->qosValue(), mem_pkt->addr, 1);

            assert(totalWriteQueueSize == isInWriteQueue.size());

            // Update stats
            stats.avgWrQLen = totalWriteQueueSize;

        } else {
            DPRINTF(MemCtrl,
                    "Merging write burst with existing queue entry\n");

            // keep track of the fact that this burst effectively
            // disappeared as it was merged with an existing one
            stats.mergedWrBursts++;
        }

        // Starting address of next memory pkt (aligned to burst_size boundary)
        addr = (addr | (burst_size - 1)) + 1;
    }

    // we do not wait for the writes to be send to the actual memory,
    // but instead take responsibility for the consistency here and
    // snoop the write queue for any upcoming reads
    // @todo, if a pkt size is larger than burst size, we might need a
    // different front end latency
    accessAndRespond(pkt, frontendLatency, mem_intr);
}

void
MemCtrl::printQs() const
{
#if TRACING_ON
    DPRINTF(MemCtrl, "===READ QUEUE===\n\n");
    for (const auto& queue : readQueue) {
        for (const auto& packet : queue) {
            DPRINTF(MemCtrl, "Read %#x\n", packet->addr);
        }
    }

    DPRINTF(MemCtrl, "\n===RESP QUEUE===\n\n");
    for (const auto& packet : respQueue) {
        DPRINTF(MemCtrl, "Response %#x\n", packet->addr);
    }

    DPRINTF(MemCtrl, "\n===WRITE QUEUE===\n\n");
    for (const auto& queue : writeQueue) {
        for (const auto& packet : queue) {
            DPRINTF(MemCtrl, "Write %#x\n", packet->addr);
        }
    }
#endif // TRACING_ON
}

//wxr+++++++++++++++++++++++

/*
*  Pre functions
*/



void 
MemCtrl::poolDelete()
{
    for (std::vector<PacketPtr>::iterator ptr = EncPktPool.begin(); ptr != EncPktPool.end(); )
    {
        if ( ((*ptr)->real_resp))
        {
            #if STATE_DEBUG 
            std::cout<<"poolDelete <<<<==== Delete the generated pkt pool ====>>>> "<<EncPktPool.size()<<" "<<std::hex<<(*ptr)->getAddr()<<std::dec<<std::endl;
            #endif

            delete (*ptr)->senderState;
            delete (*ptr);
            ptr = EncPktPool.erase(ptr);
        }
        else{
            ++ ptr;
        }
    }
}

void
MemCtrl::EncGenPacket(PacketPtr &pkt, RequestPtr &req, Addr addr, unsigned size, uint8_t* data, bool write)
{
    //as the experiment, writeReq-> id = 10. readReq->id=9
    req = std::make_shared<Request>(addr,size,0, write ? 0 : 5);
    pkt = new Packet(req, write ? MemCmd::WriteReq : MemCmd::ReadReq,1,1,1); //flag 1 = memory controller adder
    pkt->setAddr(addr);
    pkt->allocate();  //The function could copy all the created data from given pointer. Delete pkt means also releasing the buffer in this pkt.
    pkt->setData(data);
    EncPktPool.push_back(pkt);
    // std::cout<<"====<<<< packet datapayload "<<pkt->getPtr<char>()[0]<<std::endl;

    #if STATE_DEBUG 
    std::cout<<"EncGenPacket <<<<====  mem range begin ====>>>> "<<std::hex<<pkt->getAddrRange().start()<<" end: "<<pkt->getAddrRange().end()<<std::dec<<std::endl;
    #endif
    // std::cout<<"<<<<<<<< mem range: "<<std::hex<<dram->getAddrRange().to_string()<<std::endl;
}


bool
MemCtrl::EncAccessPacket(PacketPtr pkt)
{
    //++++++++++++++++++++++++++++++++++++ xrwang
    //To correctly handle the latency of a memory access,
    //insert a new event is neccessary way to increase
    //the system cycles.
    unsigned size = pkt->getSize();
    uint32_t burst_size = dram->bytesPerBurst();
    unsigned offset = pkt->getAddr() & (burst_size - 1);
    unsigned int pkt_count = divCeil(offset + size, burst_size);

    qosSchedule( { &readQueue, &writeQueue }, burst_size, pkt);

    // we need  to record the situation of read hit in write buffer
    bool rHit_Wb=false;

    // check local buffers and do not accept if full
    if (pkt->isWrite()) {
        assert(size != 0);
        assert(!writeQueueFull(pkt_count));
        addToWriteQueue(pkt, pkt_count, dram);
        if (!nextReqEvent.scheduled()) {
            schedule(nextReqEvent, curTick());
        }
        stats.writeReqs++;
        stats.bytesWrittenSys += size;
    } else {
        assert(pkt->isRead());
        assert(size != 0);
        assert(!readQueueFull(pkt_count));
        auto ss = addToReadQueue(pkt, pkt_count, dram);
        rHit_Wb = ss;
        if(!ss){
            if (!nextReqEvent.scheduled()) {
                schedule(nextReqEvent, curTick());
            }
        }
        stats.readReqs++;
        stats.bytesReadSys += size;
    }
    return rHit_Wb;
}

//************************************************** comment by xrwang
//Important to notice these two functions, as the response from the memory
//the total writequeueSize may not be decreased due to memory busy.
int
MemCtrl::getWQueueVacancy()
{
    auto vacancy = writeBufferSize - totalWriteQueueSize;
    return  vacancy <= 0 ? -1 : vacancy;
}

//Here reserves the resp queue
int
MemCtrl::getRQueueVacancy()
{
    auto vacancy = readBufferSize - totalReadQueueSize - respQueue.size();
    return  vacancy <= 0 ? -1 : vacancy;
}



/*
*  CounterInit() return TRUE when initialize done
*  If the init_p don't reach the the Counter entry size, return FALSE
*
*/
bool 
MemCtrl::CounterInit()
{
    // 1. get the empty entry in writing queen
    int queen_empty = getWQueueVacancy();
    if(queen_empty == -1)
    {
        return false;
    }
    // 2. Generate the packet to fulfill all the empty 
    int byte_remain = vault_engine.getCounterSize() - init_p;
    int size_byte = queen_empty * dram->bytesPerBurst();
    size_byte = size_byte < byte_remain ? size_byte : byte_remain;
    // size_byte = 64;
    uint8_t* payload;
    payload = new uint8_t[size_byte];
    // memset(payload,0xff,size_byte);
    memset(payload,0x0,size_byte);

    RequestPtr req0;
    EncGenPacket(pkt_encBuild,req0,ENCLAVE_STARTADDR+init_p,size_byte,payload,1);
    EncAccessPacket(pkt_encBuild);
    delete payload;
    init_p += size_byte;

    // 3. If done ,return true
    #if STATE_DEBUG 
    std::cout<<"CounterInit <<<<==== INIT SIZE ====>>>> "<<getWQueueVacancy()<<" init_p: "<<init_p<<" thisbyte: "<<size_byte<<" "<<std::endl;
    #endif
    if (init_p >= vault_engine.getCounterSize())
    {
        #if STATE_DEBUG 
        std::cout<<"CounterInit <<<<=== Counter Size ====>>>> "<<vault_engine.getCounterSize()<<" "<<writeBufferSize<<std::endl;
        #endif
        return true;
    }
    
    return false;
}


void
MemCtrl::TestOne()
{
    #if STATE_DEBUG 
    std::cout<<"<<<<==== ONLY TEST ONE ====>>>>"<<std::endl;
    #endif
    uint8_t tmp[64]={0};
    tmp[0] = 'a';
    PacketPtr p_test0;
    RequestPtr req0;
    EncGenPacket(p_test0,req0,ENCLAVE_STARTADDR,0x40,tmp,1);
    EncAccessPacket(p_test0);
    #if STATE_DEBUG 
    std::cout<<p_test0->print()<<" p_test0 data "<<p_test0->getPtr<uint8_t>()[0]<<std::endl;
    #endif
    // exit(0);


    PacketPtr p_test1;
    RequestPtr req1;
    tmp[0]=0;
    EncGenPacket(p_test1,req1,ENCLAVE_STARTADDR,0x40,tmp,0);
    EncAccessPacket(p_test1);
    #if STATE_DEBUG 
    std::cout<<"char value: "<<tmp[0]<<std::endl;
    std::cout<<p_test1->print()<<" p_test1 data "<<p_test1->getPtr<uint8_t>()[0]<<std::endl;
    std::cout<<">>>>==== ONLY TEST ONE ====<<<<"<<std::endl;
    #endif
    // exit(0);
}


uint64_t acc = 0;
uint64_t memAccessCtr = 0;

//////////////////////////////////////
//
//This funcions has the main statemachine.
//
//////////////////////////////////////
void
MemCtrl::EncStateMachine()
{
    #if STATE_DEBUG 
    std::cout<<"MAIN STATEMACHINE <<<<==== ====>>>> cyclenow: "<<curCycle()<<" EncState "<<stateEnc<<std::endl;
    #endif
    
    //***************************
    //***************************
    //***************************
    stateAgain:
    //***************************
    //***************************
    if (stateEnc == StatesEnc::SPLITAG)
    {
        stateEnc = StatesEnc::RDCNT;
        EncPreC = curCycle();
        RDCNTwRpkt_cnt = 0;
        RDCNTwRpkt.clear();
        RDCNTrDpkt_cnt = 0;
        RDCNTrDpkt.clear();
        RDCNTneedRD = false;
        RDCNTneedWB = false;

        encCache.RdMiss_ptr.clear();
        encCache.RdMissAddr.clear();
        macCache.RdMiss_ptr.clear();
        macCache.RdMissAddr.clear();
        

        RDCNTrecvCNT = 0;

        splitOriPkt_cnt ++;
        #if STATE_DEBUG 
        std::cout<<"----MAIN STATEMACHINE <<<<==== StateEnc SPLITAG to RDCNT ====>>> "<<std::endl;
        #endif

    }
    
    if (stateEnc == StatesEnc::RDCNT)
    {
        //******************************************comment by wxr
        //1. Here we first read the counters from the EncCache.
        //During reading the EncCache, the hit and miss info is
        //recorded in a stucture for future refill the counters.
        //Take care of adding the access time of EncCache.
        //2. We first evict the cache blks in EncCache according
        //the hit/miss info.
        //3. As the miss happens, the miss info can be read from
        //the structure to future refill the cache (genertate the
        //user packets then push into queue).
        //***************************************
        //NOTICE!!
        //1. the split pkt will be handled in the last of the epoch
        //of this counter read/write and operation

        //++++++++++++++++++++  2023.11.20
        // in memory reading, we just need to read until a cache hit (tree L1 hit enough)
        //--------------------



        if (!hasResponseRead)
        {
            if (!WriteIf)
                return;
        }
       
        
        assert(splitOriPkt.size() != 0);
        cntRwTrace.clear();
        // #if STATE_DEBUG std::cout<<"----MAIN STATEMACHINE <<<<==== pktAddr ====>>> "<<std::hex<<pkt->isWrite()<<" "<<pkt->getAddr()<<" packet ID: "<<pkt->requestorId()<<" req taskId: "<<pkt->req->taskId()<<std::dec<<" w/r queue vacancy "<<getWQueueVacancy()<<" "<<getRQueueVacancy()<<" CurCycle "<<curCycle()<<std::endl;
        //we read the counter 1 by 1

        if (!RDCNTneedRD && !RDCNTneedWB)
        {

            trace_global_cnt++;
            if ((trace_global_cnt/1000000 > 10) && (trace_global_cnt/1000000 <20))
            {
                if ((trace_global_cnt% 1000000)==0)
                {
                    trace_control = true;
                }
            }
            if (trace_control && TRACE_DEBUG)
            {
                if (WriteIf)
                {
                    std::cout<<"Write    ";
                }
                else
                {
                    std::cout<<"Read     ";
                }
                
                // std::cout<<"DATA: ";
                for (size_t i = 0; i < 8; i++)
                {
                    // std::cout<<std::hex<<prePkt->getPtr<uint64_t>()[i]<<std::dec;
                }
                std::cout<<" memAddr: "<<std::hex<<splitOriPkt[splitOriPkt_cnt]<<std::dec<<"   Counter Trace: ";
                trace_cnt++;
            }
            
            //Following is reading the EncCache.
            vault_engine.getCounterTrace(splitOriPkt[splitOriPkt_cnt] & ENCALVE_MASK);

            if (trace_cnt == 1000)
            {
                trace_control = false;
                std::cout<<" =========================== "<<std::endl;
                trace_cnt = 0;
            }
            

            uint8_t holdCnt[64];
            bool readUntilHit = false;
            bool encryptionCounterHit = false;
            bool treeL1Hit = false;
            bool writeReadCntMiss=false;
            //***************************************commnet by xrwang
            //Top 2 levels are kept on chip
            for (size_t i = 0; i < vault_engine.MT_LEVEL-2; i++)
            {
                int counter_off = 0;
                for (size_t j = 0; j < vault_engine.MT_LEVEL-3-i; j++)
                {
                    counter_off += vault_engine.mt_level_entry_num[vault_engine.MT_LEVEL-3-j]*64;
                }
                
                uint64_t counterAddr = (vault_engine.Counter_modify_list[i*2]*64) + ENCLAVE_STARTADDR+counter_off;
                (vault_engine.counterOP+i)->off=vault_engine.Counter_modify_list[i*2+1];
                int ary_change= i==0 ? 64 : (i==1 ? 32 : 16);
                (vault_engine.counterOP+i)->arity=ary_change;

                //we directly jump this loop
                if (!WriteIf && readUntilHit)
                {
                    continue;
                }
                else if (WriteIf && !EAGER_MODE && readUntilHit)
                {
                    continue;
                }
                

                bool evict = encCache.EncRead(counterAddr,holdCnt);
                cntRwTrace.push_back(std::vector<bool> {encCache.EncS.nowRDhit,evict});

                if (i == 0)
                {
                    if (!WriteIf)
                    {
                        stats.EncTreeL0ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncTreeL0MissTime ++;
                        }
                    }
                    else
                    {
                        stats.EncWTreeL0ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncWTreeL0MissTime ++;
                        }
                    }
                }
                else if(i==1)
                {
                    if (!WriteIf)
                    {
                        stats.EncTreeL1ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncTreeL1MissTime ++;
                        }
                    }
                    else
                    {
                        stats.EncWTreeL1ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncWTreeL1MissTime ++;
                        }
                    }
                }
                else if(i==2)
                {
                    if (!WriteIf)
                    {
                        stats.EncTreeL2ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncTreeL2MissTime ++;
                        }
                    }
                    else
                    {
                        stats.EncWTreeL2ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncWTreeL2MissTime ++;
                        }
                    }
                }
                else if(i==3)
                {
                    if (!WriteIf)
                    {
                        stats.EncTreeL3ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncTreeL3MissTime ++;
                        }
                    }
                    else
                    {
                        stats.EncWTreeL3ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncWTreeL3MissTime ++;
                        }
                    }
                }
                else if(i==4)
                {
                    if (!WriteIf)
                    {
                        stats.EncTreeL4ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncTreeL4MissTime ++;
                        }
                    }
                    else
                    {
                        stats.EncWTreeL4ReadTime ++;
                        if (!encCache.EncS.nowRDhit)
                        {
                            stats.EncWTreeL4MissTime ++;
                        }
                    }
                }
                

                if (encCache.EncS.nowRDhit)
                {
                
                    ////////////////////////////////////
                    // for record for check and update the
                    ////////////////////////////////////
                    Lread =i;

                    if (i>0)
                    {
                        if (WriteIf)
                        {
                            readUntilHit = true;
                        }
                        #if EAGER_MODE
                        #else
                        writeHashNum = i;
                        #endif
                    }
                    if (i==0)
                    {
                        if (!WriteIf)
                        {
                            readUntilHit = true;
                            noNeedTreeCheck = true;
                            stats.EncTreeNoCheck ++;
                        }
                        encryptionCounterHit = true;
                        if (WriteIf)
                        {
                            reduceNum = 1;
                        }
                    }
                    if (i==1)
                    {
                        treeL1Hit = true;
                    }
                    
                    memcpy((vault_engine.counterOP+i)->blk,holdCnt,64);
                    (vault_engine.counterOP+i)->addr=counterAddr;
                    #if STATE_DEBUG 
                    std::cout<<"----MAIN STATEMACHINE <<<<==== HIT_VAULE ====>>> ";
                    for (size_t j = 0; j < 64; j++)
                    {
                        std::cout<<std::hex<<int(holdCnt[j])<<std::dec;
                    }
                    std::cout<<std::endl;
                    #endif
                }
                else
                {
                    if (WriteIf)
                    {
                        writeReadCntMiss = true;
                    }
                    
                    if (evict)
                    {
                        RequestPtr req0;
                        EncGenPacket(pkt_encBuild,req0,encCache.Evicted_ptr.addr,64,encCache.Evicted_ptr.blk,1);
                        RDCNTwRpkt.push_back(pkt_encBuild);
                    }
                    RequestPtr req0;
                    encCache.RdMiss_ptr.push_back(encCache.RdMiss);
                    encCache.RdMissAddr.push_back(counterAddr);
                    EncGenPacket(pkt_encBuild,req0,counterAddr,64,encCache.RdMiss->blk,0);
                    RDCNTrDpkt.push_back(pkt_encBuild);
                }
            }
            
            if (WriteIf && writeReadCntMiss)
            {
                stats.EncWTreeNoCheck ++;
            }
            

            RDCNTneedRD = RDCNTrDpkt.size() != 0;
            RDCNTneedWB = RDCNTwRpkt.size() != 0;
            #if STATE_DEBUG 
            std::cout<<"----MAIN STATEMACHINE <<<<==== RDCNT EVICT/READ ====>>> "<<RDCNTwRpkt.size()<<" "<<RDCNTrDpkt.size()<<std::endl;
            #endif
        }

        if (!RDCNTneedRD && !RDCNTneedWB)
        {
            EncS.EncRDCNTC += curCycle() - EncPreC;
            memAccessTap.C_RDCNT = curCycle() - EncPreC;
            EncPreC = curCycle();
            stateEnc = StatesEnc::CNTOP;
        }

        if (RDCNTwRpkt_cnt < RDCNTwRpkt.size())
        {
            if (getWQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            else
            {
                int vac=getWQueueVacancy();
                for (size_t i = 0; i < vac; i++)
                {
                    EncAccessPacket(RDCNTwRpkt[RDCNTwRpkt_cnt]);
                    RDCNTwRpkt_cnt ++;
                    RDCNTrecvCNT ++;
                    if (RDCNTwRpkt_cnt == RDCNTwRpkt.size())
                    {
                        break;
                    }
                }
                if (RDCNTwRpkt_cnt < RDCNTwRpkt.size())
                {
                    if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                    return;
                }
            }
        }

        if (RDCNTrDpkt_cnt < RDCNTrDpkt.size())
        {
            if (getRQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            else
            {
                int vac = getRQueueVacancy();
                for (size_t i = 0; i <vac; i++)
                {
                    bool hitWbuffer = EncAccessPacket(RDCNTrDpkt[RDCNTrDpkt_cnt]);
                    if (hitWbuffer)
                    {
                        #if STATE_DEBUG 
                        std::cout<<"RDCNT <<<<==== read hit write buffer ====>>> "<<std::endl;
                        #endif
                        RDCNTrecvCNT ++;
                        int index = encCache.EncRefill(RDCNTrDpkt[RDCNTrDpkt_cnt]->getAddr(),RDCNTrDpkt[RDCNTrDpkt_cnt]->getPtr<uint8_t>());
                        memcpy((vault_engine.counterOP+index)->blk,RDCNTrDpkt[RDCNTrDpkt_cnt]->getPtr<uint8_t>(),64);
                        (vault_engine.counterOP+index)->addr=RDCNTrDpkt[RDCNTrDpkt_cnt]->getAddr();
                    }
                    RDCNTrDpkt_cnt ++;
                    if (RDCNTrDpkt_cnt == RDCNTrDpkt.size())
                    {
                        break;
                    }
                }
                if (RDCNTrDpkt_cnt < RDCNTrDpkt.size())
                {
                    if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                    return;
                }
            }
        }

        // #if STATE_DEBUG std::cout<<"----MAIN STATEMACHINE <<<<==== FLAG ====>>> "<<std::endl;
        assert(RDCNTrecvCNT <= (RDCNTrDpkt.size() + RDCNTwRpkt.size()));
        if (RDCNTrecvCNT < (RDCNTrDpkt.size() + RDCNTwRpkt.size()))
        {
            // if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
            //we just need to wait the response
            #if STATE_DEBUG 
            std::cout<<"----MAIN STATEMACHINE <<<<==== wait for reading response for CTR====>>> Rq vacancy: "<<getRQueueVacancy()<<std::endl;
            #endif
            return;
        }
        else{
            if (WriteIf)
            {
                stateEnc = StatesEnc::CNTOP;
                #if STATE_DEBUG 
                std::cout<<"----MAIN STATEMACHINE <<<<==== state RDCNT to CNTOP ====>>>> "<<std::endl;
                #endif
            }
            else
            {
                stateEnc = StatesEnc::CNTSANITY;
                // stateEnc = StatesEnc::CNTOP;
                #if STATE_DEBUG 
                std::cout<<"----MAIN STATEMACHINE <<<<==== state RDCNT to CNTSANITY ====>>>> "<<std::endl;
                #endif
            }
            EncS.EncRDCNTC += curCycle() - EncPreC;
            memAccessTap.C_RDCNT = curCycle() - EncPreC;
            EncPreC = curCycle();
            for (size_t i = 0; i < vault_engine.MT_LEVEL-2; i++)
            {
                #if STATE_DEBUG 
                std::cout<<"----MAIN STATEMACHINE <<<<==== ====>>>> "<<std::hex<<(vault_engine.counterOP+i)->addr<<std::dec<<" "<<int((vault_engine.counterOP+i)->blk[0])<<std::endl;
                #endif
            }
            
        }
        
        //processing flow is WriteBack => Read => Refill(in responding function)

    }
    
    if (stateEnc == StatesEnc::CNTOP)
    {
        //*********************************************comment by xrwang
        //In this state, the counter adding and reset are analyzed.
        //Some key information could be saved in vector structure.
        //1. The propragate of the adding tree is from root to the leaf.
        //-----------------------------wxr 2023.10.8
        /*
        #if TURNON_PRE
        TableInx tGroup;
        bool findGroup = teePrediction.findGroupRange(splitOriPkt[splitOriPkt_cnt],tGroup);
        if (findGroup)
        {
            teePrediction.upadataCtrChain(tGroup,vault_engine);
            #if PREDICTION_DEBUG
            std::cout<<"++xx++ CNTOP UPDATE CTRCHAIN. "<<" "<<curCycle()<<std::endl;
            #endif
        }


        #endif
        */

        ///////////////////////////////////////////////// xrwang
        /////////////////// R1 firstly check read
        ////////////////////////////////////////////////
        //we check the tree frist
        bool needTreeCheck = ((reduceNum == 1) && (writeHashNum == 1)) || (writeHashNum >1);
        if (needTreeCheck)
        {
            checkHAMC(true);
        }

        vault_engine.counterPropragate(writeHashNum);

        if (vault_engine.resetRecord[0])
        {
            stats.EncL0Reset ++;
        }
        
        
        #if VAULT_LAST_64
        #elif
        EncS.EncDecryptTime ++;               //decrypt of the lowest leaf node
        #endif

        for (auto i : vault_engine.resetRecord)
        {
            if (i)
            {
                EncS.EncReset++;
            }
        }
        // EncS.EncHasUpdata += vault_engine.MT_LEVEL -1;
        #if STATE_DEBUG 
        std::cout<<"----MAIN STATEMACHINE <<<<==== RDCNT cyele ====>>>> "<<EncS.EncRDCNTC<<std::endl;
        #endif
        
        EncPreC = curCycle();
        stateEnc = StatesEnc::WBLIMB;
        WBLIMBwRpkt.clear();
        WBLIMBwRpkt_cnt=0;
        WBLIMBneedWB = false;
        
        #if TURNON_PRE_WRITE
        stateEnc = StatesEnc::PREDICTION_PREFETCH_DECIDE;
        predictingRetry = false;
        if(hashLock.isLock())
        {
            if (hashLock.LockTime(curCycle()) <hashC )
            {
                EncS.EncHashHardwareWaitTime += (hashC - hashLock.LockTime(curCycle()));
            }
            hashLock.relase();
        }
        hashLock.lock(curCycle());
        #else
        if(hashLock.isLock())
        {
            if (hashLock.LockTime(curCycle()) <hashC )
            {
                EncS.EncHashHardwareWaitTime += (hashC - hashLock.LockTime(curCycle()));
            }
            hashLock.relase();
        }
        if (hashC >= encryptC)
        {
            // EncS.EncHasUpdata ++;
            if ((writeHashNum*2-reduceNum) > HASH_FOR_TREE)
            {
                reschedule(EncEvent, curTick()+hashC*CYCLETICK*2,true);
                stats.EncWTree2HASHTIME ++;
                stats.EncWriteHashTime += hashC*2;
            }
            else
            {
                reschedule(EncEvent, curTick()+hashC*CYCLETICK,true);
                stats.EncWriteHashTime += hashC;
            }
            return;
        }
        else
        {
            // EncS.EncEncryptTime ++;
            reschedule(EncEvent, curTick()+encryptC*CYCLETICK,true);
            return;
        }
        #endif
        // exit(0);
    }

    if (stateEnc == StatesEnc::CNTSANITY)
    {
        //*************************************comment by xrwang
        //In this state, we do the counter check. Considered the
        //the hash logic just consumes the cyclse, so we directly
        //add the cycles.
        // EncS.EncHasUpdata += vault_engine.MT_LEVEL - 1; //check the tree
        // EncS.EncHasUpdata ++;
        // EncS.EncDecryptTime ++; //decrypt the last level

        //////////////////////
        //
        //we put the hash cal after the data reading
        //
        //////////////////////
        //-----------------------------wxr 2023.10.8
        /*
        #if TURNON_PRE
        TableInx tGroup;
        bool findGroup = teePrediction.findGroupRange(splitOriPkt[splitOriPkt_cnt],tGroup);
        if (findGroup)
        {
            teePrediction.upadataCtrChain(tGroup,vault_engine);

            #if PREDICTION_DEBUG
            std::cout<<"++xx++ CNTSANITY UPDATE CTRCHAIN. "<<" "<<curCycle()<<std::endl;
            #endif
        }

        #endif
        */
       
        //Note we also need to check the MAC
        stateEnc = StatesEnc::RRDMAC;
        if (hashLock.isLock())
        {
            if (hashLock.LockTime(curCycle()) <hashC )
            {
                EncS.EncHashHardwareWaitTime += (hashC - hashLock.LockTime(curCycle()));
            }
            hashLock.relase();
        }
        hashLock.lock(curCycle());
        
    }

    if (stateEnc == StatesEnc::WBLIMB)
    {
        //************************************************comment by xrwang
        //1. Write the single limb of modified tree.
        //2. Note the Encalve Cache needs to be operated. Assume there is no
        //confict of the limb address, all ounters are in Enclave cache. To
        //deal with the conlict, the original writing flow is necessary.
        
        //////////////////
        /////R1 xrwang
        /////////////////
        updateTreeMAC();

        if (!WBLIMBneedWB)
        {
            #if EAGER_MODE
            for (size_t i = 0; i < vault_engine.MT_LEVEL-2; i++)
            #else
            for (size_t i = 0; i < writeHashNum; i++)
            #endif
            {
                bool ifwb = encCache.EncWrite((vault_engine.counterOP+i)->addr,(vault_engine.counterOP+i)->blk);
                if (ifwb)
                {
                    #if STATE_DEBUG 
                    std::cout<<"WBLIMB <<<<==== EncCache evict ====>>>> "<<std::endl;
                    #endif
                    RequestPtr req0;
                    EncGenPacket(pkt_encBuild,req0,encCache.Evicted_ptr.addr,64,encCache.Evicted_ptr.blk,1);
                    WBLIMBwRpkt.push_back(pkt_encBuild);
                }
            }
            WBLIMBneedWB = WBLIMBwRpkt.size() != 0;
            EncS.EncLimbUpdata ++;
            // EncS.EncHasUpdata += vault_engine.MT_LEVEL-1; //new re hash of this limb (lastest level don't need to hash)
            
            //------------------------------- wxr 10.9
            //we calculate in DATATRAN
            // EncS.EncHasUpdata ++;
            
            #if VAULT_LAST_64
            #elif
            EncS.EncEncryptTime ++; //encryt the last counter level
            #endif
        }
        
        bool hasReset = false;
        for (size_t i = 0; i < vault_engine.MT_LEVEL; i++)
        {
            hasReset |= vault_engine.resetRecord[i];
        }

        if (!WBLIMBneedWB)
        {
            stateEnc = hasReset ? StatesEnc::DORESETTREE : StatesEnc::WUPMAC;
        }
        
        
        if (WBLIMBwRpkt_cnt < WBLIMBwRpkt.size())
        {
            if (getWQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            else
            {
                int vac = getWQueueVacancy();
                for (size_t i = 0; i < vac; i++)
                {
                    EncAccessPacket(WBLIMBwRpkt[WBLIMBwRpkt_cnt]);
                    WBLIMBwRpkt_cnt ++;
                    if (WBLIMBwRpkt_cnt == WBLIMBwRpkt.size())
                    {
                        stateEnc = hasReset ? StatesEnc::DORESETTREE : StatesEnc::WUPMAC;
                        break;
                    }
                }
                if (WBLIMBwRpkt_cnt < WBLIMBwRpkt.size())
                {
                    if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                    return;
                }
            }
        }

        if (stateEnc == StatesEnc::DORESETTREE)
        {
            #if STATE_DEBUG 
            std::cout<<"WBLIMB <<<<==== EncCache from WBLIMB to DORESETTREE ====>>>> "<<std::endl;
            #endif
            DORESETTREEwRpkt.clear();
            DORESETTREErDpkt.clear();
            ResetCycleRecord.clear();
            encCache.RdMiss_ptr.clear();
            encCache.RdMissAddr.clear();
            macCache.RdMiss_ptr.clear();
            macCache.RdMissAddr.clear();
            DORESETTREEwRpkt_cnt=0;
            DORESETTREEdata_cnt=0;
            DORESETTREEdata_recv=0;
            stateRSTEnc = RstState::Pre;

            for (size_t i = 0; i < 256; i++)
            {
                ResetCycleRecord.push_back(curCycle());
            }
        }
        
        
    }
    

    if (stateEnc == StatesEnc::DORESETTREE)
    {
        //****************************** comment bt xrwang
        //In this state, the tree resetting flow will be proprageted.
        //Due to only hash re-compute, we can just generate the correct
        //packets and send them into queue.
        //Note in level0, the MACs and the hash needs to be updated.
        //***********************************************************
        //The logic of EncCache and memory access are changed, we first
        //put the data written or read into the vector, then we access
        //the cache to choose which needs to be send to memory so that
        //the logic of accessing memory and EncCache is decoupled.
        if (stateRSTEnc == RstState::Pre)
        {
            EncS.EncWBLIMB = curCycle() - EncPreC;
            EncPreC = curCycle();

            if (vault_engine.resetRecord[vault_engine.MT_LEVEL-1])
            {
                #if STATE_DEBUG 
                std::cout<<"DORESETTREE <<<<==== root reset ====>>>> "<<std::endl;
                #endif
                EncS.EncHasUpdata += 16;

                EncS.EncReset_Mem_HashTime += 16;
            }   
            
            for (size_t i = 0; i < vault_engine.MT_LEVEL-1; i++)
            {
                if (vault_engine.resetRecord[i])
                {
                    #if STATE_DEBUG 
                    std::cout<<"DORESETTREE <<<<==== Level reset ====>>>> "<<i<<std::endl;
                    #endif
                    if (i==0)
                    {
                        //64 r/w for re-encryption. 64 w for MACs update.

                        //re-encrypt the data
                        /*
                        EncS.EncEncryptTime += 64;
                        EncS.EncDecryptTime += 63; //itself doesn't need encryption
                        */
                        //re-mac the data

                        /////////////////////////// xrwang note!!
                        //
                        //Reset means serval time re-hash, so what consider parallel???
                        //
                        ///////////////////////////

                        // EncS.EncHasUpdata += 64;

                        //////////
                        //
                        //EncS to record reset
                        //
                        //////////
                        /*
                        EncS.EncReset_Mem_HashTime += 63;
                        EncS.EncReset_Mem_DecryptTime += 63;
                        EncS.EncReset_Mem_EncryptTime += 63;
                        */
                        EncS.EncReset_Mem_RdTime += 63;
                        EncS.EncReset_Mem_WrTime += 63;
                        EncS.EncReset_Mem_HashTime += 1;
                        EncS.EncHasUpdata += 1;

                        // align to 4KiB
                        //************************************************MAC - begin addr
                        Addr DATAbeginAddr = splitOriPkt[splitOriPkt_cnt] - (splitOriPkt[splitOriPkt_cnt] & 0xfff);
                        Addr MACbeginAddr = (((DATAbeginAddr - LINUX_MEM_STR)>>3)&ENCALVE_MASK) + ENCLAVE_STARTADDR + vault_engine.COUNTER_SIZE;
                        MACbeginAddr = MACbeginAddr - (MACbeginAddr & 0x3f);
                        Addr newAddrGen;
                        //8 packets can hold all MACs
                        uint8_t chunk[64]={0};
                        for (size_t j= 0; j < 64/8; j++)
                        {
                            newAddrGen = MACbeginAddr + j*64;
                            RequestPtr req0;
                            EncGenPacket(pkt_encBuild,req0,newAddrGen,64,chunk,1);
                            DORESETTREEwRpkt.push_back(pkt_encBuild);
                        }
                        for (size_t j = 0; j < 64; j++)
                        {
                            newAddrGen = DATAbeginAddr + j*64;
                            if (newAddrGen == splitOriPkt[splitOriPkt_cnt])
                            {
                                continue;
                            }
                            RequestPtr req0;
                            EncGenPacket(pkt_encBuild,req0,newAddrGen,64,chunk,0);
                            DORESETTREErDpkt.push_back(pkt_encBuild);
                            // write packets will be generated when read the correct data
                        }
                    }
                    else if (i==1)
                    {
                        /*
                        EncS.EncEncryptTime += 31;
                        EncS.EncDecryptTime += 31;

                        EncS.EncHasUpdata += 31;
                        */

                        //////////
                        //
                        //EncS to record reset
                        //
                        //////////
                        /*
                        EncS.EncReset_Mem_HashTime += 31;
                        EncS.EncReset_Mem_DecryptTime += 31;
                        EncS.EncReset_Mem_EncryptTime += 31;
                        */
                        EncS.EncReset_Mem_RdTime += 31;
                        EncS.EncReset_Mem_WrTime += 31;
                        EncS.EncHasUpdata += 1;

                        // align to 4KiB
                        Addr CNTbeginAddr = vault_engine.counterOP->addr - (vault_engine.counterOP->addr & 0x7ff);
                        Addr newAddrGen;
                        //8 packets can hold all MACs
                        uint8_t chunk[64]={0};
                        for (size_t j = 0; j < 32; j++)
                        {
                            newAddrGen = CNTbeginAddr + j*64;
                            if (newAddrGen == vault_engine.counterOP->addr )
                            {
                                continue;
                            }
                            RequestPtr req0;
                            EncGenPacket(pkt_encBuild,req0,newAddrGen,64,chunk,0);
                            DORESETTREErDpkt.push_back(pkt_encBuild);
                            // write packets will be generated when read the correct data
                        }
                    }
                    else
                    {
                        // EncS.EncHasUpdata += 15;

                        //////////
                        //
                        //EncS to record reset
                        //
                        //////////
                        /*
                        EncS.EncReset_Mem_HashTime += 15;
                        EncS.EncReset_Mem_DecryptTime += 15;
                        EncS.EncReset_Mem_EncryptTime += 15;
                        */
                        EncS.EncReset_Mem_RdTime += 15;
                        EncS.EncReset_Mem_WrTime += 15;
                        EncS.EncHasUpdata += 1;

                        // align to 1KiB
                        Addr CNTbeginAddr = (vault_engine.counterOP+i-1)->addr - ((vault_engine.counterOP+i-1)->addr & 0x3ff);
                        Addr newAddrGen;
                        //8 packets can hold all MACs
                        uint8_t chunk[64]={0};
                        for (size_t j = 0; j < 16; j++)
                        {
                            newAddrGen = CNTbeginAddr + j*64;
                            if (newAddrGen == (vault_engine.counterOP+i-1)->addr)
                            {
                                continue;
                            }
                            RequestPtr req0;
                            EncGenPacket(pkt_encBuild,req0,newAddrGen,64,chunk,0);
                            DORESETTREErDpkt.push_back(pkt_encBuild);
                            // write packets will be generated when read the correct data
                        }
                    }
                }
            }
            if (DORESETTREEwRpkt.size() != 0)
            {
                stateRSTEnc = RstState::MACDATA;
            }
            else{
                stateRSTEnc = RstState::TREErdEncCache;
            }
        }


        //Remember to read a vaule until sucessful then write.
        //But the MACs updating can be directly written into memory.
        if (stateRSTEnc == RstState::MACDATA)
        {
            if (getWQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            while (DORESETTREEwRpkt_cnt < DORESETTREEwRpkt.size())
            {
                #if STATE_DEBUG 
                std::cout<<"DORESETTREE <<<<==== MACDATA ====>>>> "<<curCycle()<<std::endl;
                #endif
                uint8_t chunk[64] = {0};
                bool ifwb = macCache.EncWrite(DORESETTREEwRpkt[DORESETTREEwRpkt_cnt]->getAddr(),chunk);
                //set the clear flag
                DORESETTREEwRpkt[DORESETTREEwRpkt_cnt]->real_resp=true;
                DORESETTREEwRpkt_cnt ++;
                if (ifwb)
                {
                    #if STATE_DEBUG 
                    std::cout<<"DORESETTREE <<<<==== EncCache evict ====>>>> "<<curCycle()<<std::endl;
                    #endif
                    RequestPtr req0;
                    EncGenPacket(pkt_encBuild,req0,macCache.Evicted_ptr.addr,64,macCache.Evicted_ptr.blk,1);
                    EncAccessPacket(pkt_encBuild);
                    if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                    return;
                }
            }
            if (DORESETTREEwRpkt_cnt == DORESETTREEwRpkt.size())
            {
                stateRSTEnc = RstState::DATA_R;
            }
        }

        if (stateRSTEnc == RstState::DATA_R)
        {
            #if STATE_DEBUG 
            std::cout<<"DORESETTREE <<<<==== DATA_R ====>>>> "<<getRQueueVacancy()<<" "<<DORESETTREEdata_cnt<<std::endl;
            #endif
            if (getRQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            int vac = getRQueueVacancy();
            while (DORESETTREEdata_cnt < 63)
            {
                for (size_t i = 0; i < vac; i++)
                {
                    bool hitWbuffer = EncAccessPacket(DORESETTREErDpkt[DORESETTREEdata_cnt]);
                    if (hitWbuffer)
                    {
                        #if STATE_DEBUG 
                        std::cout<<"DATA_R <<<<==== read hit write buffer ====>>> "<<std::endl;
                        #endif
                        DORESETTREErDpkt[DORESETTREEdata_cnt]->real_resp = false;
                        DORESETTREEdata_recv ++;

                        EncS.EncReset_Mem_HashTime ++;
                        EncS.EncReset_Mem_DecryptTime ++;
                        EncS.EncReset_Mem_EncryptTime ++;

                        EncS.EncHasUpdata ++;
                        EncS.EncEncryptTime ++;
                        EncS.EncDecryptTime ++;
                    }
                    //always push the cycles, due to in responsing function, always find the match
                    ResetCycleRecord[DORESETTREEdata_cnt] = curCycle();

                    DORESETTREEdata_cnt ++;
                    if (DORESETTREEdata_cnt == 63)
                    {
                        break;
                    }
                    
                }
                if (DORESETTREEdata_cnt == 63)
                {
                    stateRSTEnc = RstState::DATA_W_R;
                    break;
                }
                else{
                    if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                    return;
                }
            }
        }

        if (stateRSTEnc == RstState::DATA_W_R)
        {
            #if STATE_DEBUG 
            std::cout<<"DORESETTREE <<<<==== data wait read ====>>>> "<<curCycle()<<std::endl;
            #endif
            if (DORESETTREEdata_recv == 63)
            {
                stateRSTEnc = RstState::DATA_W;
                DORESETTREEdata_cnt = 0;
            }
            else{
                // if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
        }

        if (stateRSTEnc == RstState::DATA_W)
        {
            if (getWQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            
            while (DORESETTREEdata_cnt < 63)
            {
                int vac = getWQueueVacancy();
                for (size_t i = 0; i < vac; i++)
                {
                    RequestPtr req0;
                    EncGenPacket(pkt_encBuild,req0,DORESETTREErDpkt[0]->getAddr(),64,DORESETTREErDpkt[0]->getPtr<uint8_t>(),1);
                    EncAccessPacket(pkt_encBuild);
                    DORESETTREErDpkt[0]->real_resp=true;
                    DORESETTREErDpkt.erase(DORESETTREErDpkt.begin());
                    DORESETTREEdata_cnt ++;
                    if (DORESETTREEdata_cnt == 63)
                    {
                        break;
                    }
                    
                }
                if (DORESETTREEdata_cnt == 63)
                {
                    // stateRSTEnc = RstState::TREErdEncCache;
                    if (DORESETTREErDpkt.size() == 0)
                    {
                        stateRSTEnc = RstState::Pre;
                        stateEnc = StatesEnc::DATATRAN;
                    }
                    else{
                        stateRSTEnc = RstState::TREErdEncCache;
                    }
                    
                    break;
                }
                else{
                    if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                    return;
                }
            }
        }
        
        
        
        

        //++++++++++++++++++++++++++++++++++
        //++++++++++++++++++++++++++++++++++
        rdCacheAgain:
        //----------------------------------
        //----------------------------------
        if (stateRSTEnc == RstState::TREErdEncCache)
        {
            //First read the EncCache, then write back dirty then write this blk(certainly in EncCache)
            while (DORESETTREErDpkt.size() != 0)
            {
                #if STATE_DEBUG 
                std::cout<<"DORESETTREE <<<<==== TREErdEncCache ====>>>> "<<std::hex<<DORESETTREErDpkt[0]->getAddr()<<" "<<std::dec<<curCycle()<<std::endl;
                #endif
                uint8_t holdCnt[64];
                bool evict = encCache.EncRead(DORESETTREErDpkt[0]->getAddr(),holdCnt);
                if (encCache.EncS.nowRDhit)
                {
                    encCache.EncWrite(DORESETTREErDpkt[0]->getAddr(),holdCnt);
                    DORESETTREErDpkt[0]->real_resp = true;
                    DORESETTREErDpkt.erase(DORESETTREErDpkt.begin());

                    EncS.EncReset_Mem_HashTime ++;
                    EncS.EncHasUpdata ++;
                }
                else
                {
                    encCache.RdMiss_ptr.push_back(encCache.RdMiss);
                    encCache.RdMissAddr.push_back(DORESETTREErDpkt[0]->getAddr());
                    if (evict)
                    {
                        stateRSTEnc = RstState::TREEWB;
                    }
                    else
                    {
                        stateRSTEnc = RstState::TREErdMem;
                    }
                    break;
                }
            }
            if (DORESETTREErDpkt.size() == 0)
            {
                stateRSTEnc = RstState::Pre;
                stateEnc = StatesEnc::DATATRAN;
            }
        }

        if (stateRSTEnc == RstState::TREEWB)
        {
            #if STATE_DEBUG 
            std::cout<<"DORESETTREE <<<<==== TREEWB ====>>>> "<<curCycle()<<std::endl;
            #endif
            if (getWQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }

            RequestPtr req0;
            EncGenPacket(pkt_encBuild,req0,encCache.Evicted_ptr.addr,64,encCache.Evicted_ptr.blk,1);
            EncAccessPacket(pkt_encBuild);
            stateRSTEnc = RstState::TREErdMem;
            if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
            return;
        }

        if (stateRSTEnc == RstState::TREErdMem)
        {
            #if STATE_DEBUG 
            std::cout<<"DORESETTREE <<<<==== TREErdMem before ====>>>> "<<getRQueueVacancy()<<" "<<totalReadQueueSize<<" "<<curCycle()<<std::endl;
            #endif
            if (getRQueueVacancy() == -1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            RequestPtr req0;
            // uint8_t chunk[64]={0};
            //we directly send the before packet
            // EncGenPacket(pkt_encBuild,req0,DORESETTREErDpkt[0]->getAddr(),64,chunk,0);
            bool hitWbuffer = EncAccessPacket(DORESETTREErDpkt[0]);
            ResetCycleRecord[0] = curCycle();
            if (hitWbuffer)
            {
                #if STATE_DEBUG 
                std::cout<<"TREErdMem <<<<==== read hit write buffer ====>>> "<<std::endl;
                #endif
                encCache.EncRefill(DORESETTREErDpkt[0]->getAddr(),DORESETTREErDpkt[0]->getPtr<uint8_t>());
                encCache.EncWrite(DORESETTREErDpkt[0]->getAddr(),DORESETTREErDpkt[0]->getPtr<uint8_t>());
                DORESETTREErDpkt.erase(DORESETTREErDpkt.begin());

                encCache.RdMiss_ptr.clear();
                encCache.RdMissAddr.clear();
                stateRSTEnc = RstState::DATA_HBUF;

                EncS.EncReset_Mem_HashTime ++;
                EncS.EncHasUpdata ++;
            }
            else
            {
                stateRSTEnc = RstState::WaitResp;
            }
            #if STATE_DEBUG 
            std::cout<<"DORESETTREE <<<<==== TREErdMem after ====>>>> "<<getRQueueVacancy()<<" "<<totalReadQueueSize<<" "<<curCycle()<<std::endl;
            #endif
        }

        if (stateRSTEnc == RstState::DATA_HBUF)
        {
            #if STATE_DEBUG 
            std::cout<<"DORESETTREE <<<<==== DATA_HBUF ====>>>> "<<curCycle()<<std::endl;
            #endif
            if (DORESETTREErDpkt.size() == 0)
            {
                if (splitOriPkt_cnt == (splitOriPkt.size()-1))
                {
                    stateEnc = StatesEnc::DATATRAN;
                }
                else
                {
                    stateEnc = StatesEnc::SPLITAG;
                    goto stateAgain;
                }
            }
            else
            {
                stateRSTEnc = RstState::TREErdEncCache;
                goto rdCacheAgain;
            }
        }
        
        if (stateRSTEnc == RstState::WaitResp)
        {
            #if STATE_DEBUG 
            std::cout<<"DORESETTREE <<<<==== WaitResp ====>>>> "<<curCycle()<<std::endl;
            #endif
            // if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
            return;
        }
        


    }


    if (stateEnc == StatesEnc::WUPMAC)
    {
        Addr DATAbeginAddr = splitOriPkt[splitOriPkt_cnt];
        Addr MACbeginAddr = (((DATAbeginAddr - LINUX_MEM_STR)>>3)&ENCALVE_MASK) + ENCLAVE_STARTADDR + vault_engine.COUNTER_SIZE;
        int MAC_Loc = ((DATAbeginAddr-LINUX_MEM_STR) >> 6) & 0x7; // get the mac location
        MACbeginAddr -= (MACbeginAddr&0x3f);
        if (getWQueueVacancy() == -1)
        {
            if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
            return;
        }
        #if STATE_DEBUG
        std::cout<<"WUPMAC <<<<==== update mac ====>>>> "<<curCycle()<<std::endl;
        #endif
        
        //----------------------------- wxr 10.9
        //we calculate this in DATATRAN
        // EncS.EncEncryptTime ++;
        
        ///////////////////////////xrwong note!!!
        //
        //We parallel the authentation MAC with tree
        //
        ///////////////////////////
        // EncS.EncHasUpdata ++;
        //we just write 0 to this encCache block due to we set all the hash as 0
        // uint8_t chunk[64]={0};
        // bool evict = macCache.EncWrite(MACbeginAddr,chunk);
        //////////////////////R1
        ///////////////////////////////////////// NOTE !!!!!!!!!!!!!!!!!!!!!!
        ///////////  In real system, we can use byteneable, like some RISCV Rocket-Chip. So we use function read to avoid functional failure. (has no influence on performance)
        ///////////////////////////////////////// R1

        RequestPtr req_func = std::make_shared<Request>(MACbeginAddr,64,0, 0 ? 0 : 5);
        PacketPtr pkt_func = new Packet(req_func,MemCmd::ReadReq,1,1,1); //flag 1 = memory controller adder
        pkt_func->setAddr(MACbeginAddr);
        pkt_func->allocate();
        recvFunctional(pkt_func);

        bool evict = updateDataMAC(MACbeginAddr,MAC_Loc,pkt_func->getPtr<uint8_t>());
        delete (pkt_func)->senderState;
        delete (pkt_func);
        ///////////////////////////////////////// R1
        if (evict)
        {
            RequestPtr req0;
            EncGenPacket(pkt_encBuild,req0,macCache.Evicted_ptr.addr,64,macCache.Evicted_ptr.blk,1);
            EncAccessPacket(pkt_encBuild);
        }

        if (splitOriPkt_cnt == (splitOriPkt.size()-1))
        {
            stateEnc = StatesEnc::WDONE;
        }
        else
        {
            stateEnc = StatesEnc::SPLITAG;
            goto stateAgain;
        }
        
    }

    if (stateEnc == StatesEnc::RRDMAC)
    {
        Addr DATAbeginAddr = splitOriPkt[splitOriPkt_cnt];
        Addr MACbeginAddr = (((DATAbeginAddr - LINUX_MEM_STR) >>3)&ENCALVE_MASK) + ENCLAVE_STARTADDR + vault_engine.COUNTER_SIZE;
        MAC_ADDR = ((DATAbeginAddr-LINUX_MEM_STR) >> 6) & 0x7;
        MACbeginAddr -= (MACbeginAddr&0x3f);
        macAddrread=MACbeginAddr;


        // EncS.EncDecryptTime ++;
        ///////////////////////////xrwong note!!!
        //
        //We parallel the authentation MAC with tree
        //
        ///////////////////////////
        // EncS.EncHasUpdata ++;
        //we just write 0 to this encCache block due to we set all the hash as 0
        uint8_t chunk[64]={0};
        bool evict = macCache.EncRead(MACbeginAddr,chunk);
        if (macCache.EncS.nowRDhit)
        {
            //////////////////////
            //to record the MAC to verify
            /////////////////////
            memcpy(RMAC,chunk,64);
            macCheck = false;

            if (splitOriPkt_cnt == (splitOriPkt.size()-1))
            {
                // stateEnc = StatesEnc::DATATRAN;
                stateEnc = StatesEnc::RDONE;
            }
            else
            {
                stateEnc = StatesEnc::SPLITAG;
                goto stateAgain;
            }
        }
        else
        {

            macCheck = true;

            macCache.RdMiss_ptr.push_back(macCache.RdMiss);
            macCache.RdMissAddr.push_back(MACbeginAddr);
            if (evict)
            {
                RequestPtr req0;
                EncGenPacket(pkt_encBuild,req0,macCache.Evicted_ptr.addr,64,macCache.Evicted_ptr.blk,1);
                stateEnc = StatesEnc::RRDMAC_WB;
                #if STATE_DEBUG 
                std::cout<<"RRDMAC <<<<==== RRDMAC to RRDMAC_WB ====>>>> "<<curCycle()<<std::endl;
                #endif
            }
            else
            {
                stateEnc = StatesEnc::RRDMAC_REFILL;
                #if STATE_DEBUG 
                std::cout<<"RRDMAC <<<<==== RRDMAC to RRDMAC_REFILL ====>>>> "<<curCycle()<<std::endl;
                #endif
            }
        }
    }

    if (stateEnc == StatesEnc::RRDMAC_WB)
    {
        if (getWQueueVacancy() == -1)
        {
            if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
            return;
        }
        EncAccessPacket(pkt_encBuild);
        stateEnc = StatesEnc::RRDMAC_REFILL;
        #if STATE_DEBUG 
        std::cout<<"RRDMAC_WB <<<<==== RRDMAC_WB to RRDMAC_REFILL ====>>>> "<<curCycle()<<std::endl;
        #endif
    }

    if (stateEnc == StatesEnc::RRDMAC_REFILL)
    {
        if (getRQueueVacancy() == -1)
        {
            if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
            return;
        }
        Addr DATAbeginAddr = splitOriPkt[splitOriPkt_cnt];
        Addr MACbeginAddr = (((DATAbeginAddr - LINUX_MEM_STR)>>3)&ENCALVE_MASK) + ENCLAVE_STARTADDR + vault_engine.COUNTER_SIZE;
        MACbeginAddr -= (MACbeginAddr&0x3f);
        uint8_t chunk[64] = {0};
        RequestPtr req0;
        EncGenPacket(pkt_encBuild,req0,MACbeginAddr,64,chunk,0);
        bool hitbuffer = EncAccessPacket(pkt_encBuild);
        if (hitbuffer)
        {
            stateEnc = StatesEnc::RDONE;
            /////////////////
            //hit buffer and record the MAC
            ////////////////
            memcpy(RMAC,pkt_encBuild->getPtr<uint8_t>(),64);

            #if STATE_DEBUG 
            std::cout<<"RRDMAC_WB <<<<==== RRDMAC_REFILL to DATATRAN ====>>>> "<<curCycle()<<std::endl;
            #endif
        }
        else
        {
            stateEnc = StatesEnc::RRDMAC_RESP;
            #if STATE_DEBUG 
            std::cout<<"RRDMAC_WB <<<<==== RRDMAC_REFILL to RRDMAC_RESP ====>>>> "<<curCycle()<<std::endl;
            #endif
        }
    }
    
    if (stateEnc == StatesEnc::RRDMAC_RESP)
    {
        #if STATE_DEBUG 
        std::cout<<"RRDMAC_RESP <<<<==== MAC WAIT ====>>>> "<<curCycle()<<std::endl;
        #endif
        // if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
        return;
    }

    //we will overlap the time of "ReadMac" and "HASH" 
    if (stateEnc == StatesEnc::RDONE)
    {
        ////////////////////////////////////
        // always to check the MAC
        ///////////////////////////////////
        bool errHmac = checkHAMC();
        #if DEBUG_HASH
        if(!errHmac)
            std::cout<<"Hash result wrong"<<std::endl;
        #endif



        #if TURNON_PRE
        int source_id=0;
        int hashId=-1;
        int find = teePrediction.findMatchInterface(splitOriPkt[splitOriPkt_cnt],vault_engine,readDataRecord,source_id,hashId);
        bool hasCorrectPrediction = find ==1 && noNeedTreeCheck;
        bool hasToWaitCal = find == 2 && noNeedTreeCheck;
        if ((find == 1|| find==2) && !noNeedTreeCheck)
        {
            EncS.EncPredictionSuccessTree ++;
        }
        
        
        /////
        // we update the group real addr
        /////
        TableInx coverGroupInx;
        bool hasThisGroupNow = teePrediction.findGroupRange(splitOriPkt[splitOriPkt_cnt],coverGroupInx);
        if (hasThisGroupNow)
        {
            teePrediction.getGroup(coverGroupInx).updateReal(splitOriPkt[splitOriPkt_cnt], hasCorrectPrediction||hasToWaitCal);
            // if (EncS.EncReadReq > 800000)
                // std::cout<<"  addr : "<<std::hex<<pkt->getAddr()<<" bias: "<<teePrediction.getGroup(coverGroupInx)<<std::dec<<std::endl;
        }
        

        if (hasCorrectPrediction)
        {
            EncS.EncPredictionSuccess ++;
            if (source_id==1)
            {
                EncS.EncPredictionWriteSuccess ++;
            }
            #if PREDICTION_DEBUG
                std::cout<<"++++xxxx++++ Successful Prediction! DataAddr: "<<std::hex<<splitOriPkt[splitOriPkt_cnt]<<std::dec<<" now successful times: "<<EncS.EncPredictionSuccess<<" / "<<EncS.EncReadReq<<" "<<std::dec<<curCycle()<<std::endl;
            #endif
            stateEnc = StatesEnc::DATATRAN;
        }
        else if(hasToWaitCal)
        {
            if (hashId == -1)
            {
                //we delete this entry
                assert(hashId != -1);
                hashId = 0;
                stateEnc = StatesEnc::PREDICTION_PREFETCH_DECIDE;
            }
            else{
                stats.EncPreWaitCaled ++;
                EncS.EncPreWaitCaled ++;
                // assert(hashGroup.isBusy(hashId));
                //may be this entry has no begin to cal
                uint64_t nextScheduleTime;
                if (!hashGroup.isBusy(hashId))
                {
                    nextScheduleTime = hashGroup.getIdReadyTime(hashId,curCycle());
                }
                else
                {
                    nextScheduleTime = hashC;
                    hashId = 0;
                }
                stateEnc = StatesEnc::DATATRAN;

                assert(!EncEvent.scheduled());
                schedule(EncEvent, curTick() + nextScheduleTime*CYCLETICK);
                return;
            }
        }
        else
        {
            if (hashLock.LockTime(curCycle()) <hashC )
            {
                overlapHashRDMAC = hashC - hashLock.LockTime(curCycle());
            }
            else
            {
                overlapHashRDMAC = 0;
            }
            if (overlapHashRDMAC < gapPre.getAvgPrefetch())
            {
                // std::cout<<"---------------- "<<EncS.EncPrefetchTime<<" "<<EncS.MacCacheRd_hit<<" "<<overlapHashRDMAC<<" "<<gapPre.getAvgGap()<<" "<<gapPre.getAvgPrefetch()<<std::endl;
                #if PDA
                if (isChecking)
                {
                    EncS.EncPrefetchWaitPort ++;
                    stateEnc = StatesEnc::DATATRAN;
                    hashLock.relase();
                    gapPre.updatePrefetch(EncS.EncPrefetchCycle/EncS.EncPrefetchTime);
                    reschedule(EncEvent,curTick()+overlapHashRDMAC*CYCLETICK,true);
                    return;
                }
                else
                {
                    if ((overlapHashRDMAC + gapPre.getAvgGap()) < gapPre.getAvgPrefetch())
                    {
                        stateEnc = StatesEnc::DATATRAN;
                        hashLock.relase();
                        gapPre.updatePrefetch(EncS.EncPrefetchCycle/EncS.EncPrefetchTime);
                        reschedule(EncEvent,curTick()+overlapHashRDMAC*CYCLETICK,true);
                        return;
                    }
                    else
                    {
                        stateEnc = StatesEnc::PREDICTION_PREFETCH_DECIDE;
                    }
                }
                #else
                stateEnc = StatesEnc::PREDICTION_PREFETCH_DECIDE;
                #endif
            }
            else{
                // std::cout<<EncS.EncPrefetchTime<<" "<<EncS.MacCacheRd_hit<<" "<<overlapHashRDMAC<<" "<<gapPre.getAvgGap()<<" "<<gapPre.getAvgPrefetch()<<std::endl;
                stateEnc = StatesEnc::PREDICTION_PREFETCH_DECIDE;
            }
            #if PREDICTION_DEBUG
            std::cout<<"++++xxxx++++ Failure in Prediction. Predict for Next Round! find res: "<<find<<curCycle()<<std::endl;
            #endif
        }
        #else
        stateEnc = StatesEnc::DATATRAN;
        #endif

        #if TURNON_PRE
        #else
        bool hasReturn = false;
        #endif
        assert(hashLock.isLock());
        if (hashLock.isLock())
        {
            if (hashLock.LockTime(curCycle()) <hashC )
            {
                overlapHashRDMAC = hashC - hashLock.LockTime(curCycle());
                #if TURNON_PRE
                if (!hasCorrectPrediction)
                {
                    EncS.EncHashOverlapRDMAC += overlapHashRDMAC;
                }
                #else
                EncS.EncHashOverlapRDMAC += overlapHashRDMAC;
                hasReturn=true;
                #endif
            }
            else
            {
                overlapHashRDMAC = 0;
            }
            hashLock.relase();
            #if TURNON_PRE
            PrefetchCycle = curCycle();
            #else
            if (hasReturn)
            {
                reschedule(EncEvent,curTick()+overlapHashRDMAC*CYCLETICK,true);
                return;
            }
            #endif
        }
    }

    if (stateEnc == StatesEnc:: WDONE)
    {
        stateEnc = StatesEnc::DATATRAN;
    }
    
    

    
    #if PREDICTION_DEBUG
    bool inPredicting = (stateEnc == StatesEnc::PREDICTION_CAL) || (stateEnc == StatesEnc::PREDICTION_DONE) || 
                        (stateEnc == StatesEnc::PREDICTION_PREFETCH_DECIDE) || (stateEnc == StatesEnc::PREDICTION_PREFETCH_DO) ||
                        (stateEnc == StatesEnc::PREDICTION_PREFETCH_WAIT);

    if (inPredicting)
    {
        std::cout<<"++xx++ EncEvent in Prediction "<<curCycle()<<std::endl;
    }
    #endif

    if (stateEnc==StatesEnc::PREDICTION_PREFETCH_DECIDE)
    {
        //we first needs to decide what needs to be prefetched
        //3 parts  CipherText + Counter0 + CounterChain
        #if PREDICTION_DEBUG
        teePrediction.print();
        #endif
        Addr nowDataAddr;
        Addr nowCtrAddr=0;
        if (!WriteIf)
        {
            nowDataAddr = splitOriPkt[splitOriPkt_cnt];
            nowCtrAddr = vault_engine.counterOP[0].addr;
            preDataAddr = nowDataAddr;
            preCtr0Addr = nowCtrAddr;
            // teePrediction.prefetchAddrs(preDataAddr, preCtr0Addr);
        }
        else
        {
            //------------------------ wxr 10.9
            //we predict for read when write
            // nowDataAddr = readHistrory.AddrRecord.back();
            nowDataAddr = ReadDataTransAddrRecord;
            preDataAddr = nowDataAddr;
            preCtr0Addr = nowCtrAddr;
            bool writeHasFindGroup = teePrediction.findGroupRange(preDataAddr,groupPreTInx,true);
            if(!writeHasFindGroup)
            {
                stateEnc = StatesEnc::PREDICTION_DONE;
                EncS.EncWritePredictionNoneed ++;
            }
            else{
                bool needPrediction = teePrediction.PredictionAddr(preDataAddr,groupPreTInx);
                if (!needPrediction)
                {
                    stateEnc = StatesEnc::PREDICTION_DONE;
                    EncS.EncWritePredictionNoneed ++;
                }
            }
        }
        // assert(stateEnc!=StatesEnc::PREDICTION_DONE);

        prefetchCounter = 0;
        prefetchRecvCounter = 0;
        Ctr0_Cached = false;
        //judge if the prediction miss the CtrChain cover
        if (stateEnc != StatesEnc::PREDICTION_DONE)
        {
            //////////////
            //
            //Aes PAD prediction
            //
            //////////////

            //note due to aes prediction be overlaped with origin data HASH
            //so we don't need additional aes hardware for PAD prediction
            //and the aes result must be ready in this check

            //////////////
            //Aes PAD done
            //////////////
            bool hasFindGroup = teePrediction.findGroupRange(preDataAddr,groupPreTInx,true);
            #if PREDICTION_DEBUG
            std::cout<<"++xx++ DECIDE. nowAddr: "<<std::hex<<nowDataAddr<<" preAddr: "<<preDataAddr<<std::dec<<std::endl;
            #endif
            if (!hasFindGroup)
            {
                assert(!WriteIf);
                // we directly first allocate a empty group in this situation
                #if PREDICTION_DEBUG
                std::cout<<"++xx++ DECIDE. No group Covered. Allocate new group "<<curCycle()<<std::endl;
                #endif
                preDataAddr = preDataAddr&teePrediction.Group_cover_mask;
                teePrediction.alloc_Entry(preDataAddr,curCycle(),0);

                hashGroup.updateNoneedHash(teePrediction);
                hashGroup.searchPTable2attach(teePrediction,curCycle());
                uint64_t hashGroupReady = hashGroup.getReadyTime(curCycle());
                if (!hashHardwareEvent.scheduled() && !hashGroup.isEmpty())
                {
                    schedule(hashHardwareEvent,curTick() + hashGroupReady*CYCLETICK);
                }

                groupPreTInx = TableInx{teePrediction.GroupNum-1,teePrediction.GroupEntryNum-1};

                //note we also need to modify the counterAddr matching preDataAddr
                teePrediction.tree_engine.getCounterTrace(preDataAddr&ENCALVE_MASK);
                int counter_off = 0;
                for (size_t j = 0; j < vault_engine.MT_LEVEL-3; j++)
                {
                    counter_off += teePrediction.tree_engine.mt_level_entry_num[vault_engine.MT_LEVEL-3-j]*64;
                }
                
                uint64_t counterAddr = (teePrediction.tree_engine.Counter_modify_list[0]*64) + ENCLAVE_STARTADDR+counter_off;
                preCtr0Addr = counterAddr;

                Ctr0_Cached = true; //this ctr0 must be cached
                //--------------------------------wxr 10.8
                //we directly update whole chain
                teePrediction.upadateCtr0(groupPreTInx,vault_engine.counterOP[0].blk);
                teePrediction.upadataCtrChain(groupPreTInx,vault_engine);

                //we updaete the next address of beginning
                teePrediction.updateAesEntry(groupPreTInx,preDataAddr + 0x40);
            }
            else
            {
                #if PREDICTION_DEBUG
                std::cout<<"++xx++ DECIDE. Group Covered. Allocate new Entry "<<curCycle()<<std::endl;
                #endif
                int source_id = WriteIf ? 1 : 0;
                if (!WriteIf)
                {
                    //we need to correct the address fo reading
                    ///////////////////////////////////////////////////
                    //to prediction the bias
                    ///////////////////////////////////////////////////
                    teePrediction.predictionBias(groupPreTInx);
                    #if APA
                    teePrediction.PredictionAddr(preDataAddr,groupPreTInx,teePrediction.getGroup(groupPreTInx).nowBias);
                    #else
                    srand((unsigned)time(NULL));
                    teePrediction.PredictionAddr(preDataAddr,groupPreTInx,(rand()%64) * 0x40);
                    #endif
                    // teePrediction.PredictionAddr(preDataAddr,groupPreTInx,0xc0);
                    // teePrediction.PredictionAddr(preDataAddr,groupPreTInx);
                }
                
                groupPreTInx = teePrediction.alloc_Entry(preDataAddr,curCycle(),source_id);
                ////////////////
                //we predict for aes
                ////////////////
                #if APA
                teePrediction.AddAesEntry(groupPreTInx,nowDataAddr,teePrediction.getGroup(groupPreTInx).secondBias);
                #else
                srand((unsigned)time(NULL));
                teePrediction.AddAesEntry(groupPreTInx,nowDataAddr,(rand()%64)*0x40);
                #endif
                hashGroup.updateNoneedHash(teePrediction);
                hashGroup.searchPTable2attach(teePrediction,curCycle());
                uint64_t hashGroupReady = hashGroup.getReadyTime(curCycle());
                if (!hashHardwareEvent.scheduled() && !hashGroup.isEmpty())
                {
                    schedule(hashHardwareEvent,curTick() + hashGroupReady*CYCLETICK);
                }
                
                //first we read the EncCache 
                // uint8_t data[64];
                //Here we need to note that the ciphertext cannot be cached 
                //-------------------------wxr 2023.10.8
                /*
                Ctr0_Cached = encCache.EncRead(preCtr0Addr,data);
                if (Ctr0_Cached)
                {
                    teePrediction.upadateCtr0(groupPreTInx,data);
                }
                */
                //directly update the Ctr0
                Ctr0_Cached = true; //this ctr0 must be cached
                if (!WriteIf)
                {
                    teePrediction.upadateCtr0(groupPreTInx,vault_engine.counterOP[0].blk);
                }
                else
                {
                    auto group_itr = teePrediction.group.begin();
                    for (size_t i = 0; i < groupPreTInx.groupInx; i++)
                    {
                        group_itr ++;
                    }
                    if ((*group_itr).blkCtr0Valid)
                    {
                        teePrediction.upadateCtr0(groupPreTInx,(*group_itr).blkCtr0);
                    }
                    else
                    {
                        assert((*group_itr).chunkCtr0Valid);
                        teePrediction.upadateCtr0(groupPreTInx,(*group_itr).chunkCtr0);
                    }
                }

            }
            stateEnc=StatesEnc::PREDICTION_PREFETCH_DO;
            #if PREDICTION_DEBUG
            std::cout<<"++xx++ DECIDE. now CtrAddr "<<std::hex<<vault_engine.counterOP[0].addr<<" prediction CtrAddr"<<preCtr0Addr<<std::dec<<std::endl;
            #endif
        }
        else
        {
            assert(WriteIf);
            stateEnc = StatesEnc::WBLIMB;
            if(hashLock.isLock())
            {
                if (hashLock.LockTime(curCycle()) <hashC )
                {
                    EncS.EncHashHardwareWaitTime += (hashC - hashLock.LockTime(curCycle()));
                }
                hashLock.relase();
            }
            if (hashC > encryptC)
            {
                reschedule(EncEvent, curTick()+hashC*CYCLETICK,true);
                return;
            }
            else
            {
                reschedule(EncEvent, curTick()+encryptC*CYCLETICK,true);
                return;
            }
        }
    }

    if (stateEnc==StatesEnc::PREDICTION_PREFETCH_DO)
    {
        //------------------------- wxr 10.10
        //we first see if hashLock. if it.s locked, we add waiting time
        ////////////////////////
        ///////////////////////
        if(hashLock.isLock())
        {
            assert(WriteIf);
            // if (hashLock.LockTime(curCycle()) <hashC )
            // {
            //     EncS.EncHashHardwareWaitTime += (hashC - hashLock.LockTime(curCycle()));
            // }
            // hashLock.relase();
            // hashLock.lock(curCycle());
        }
        ///////////////////////
        ///////////////////////
        
        //we begin to fetch ciphertext and Ctr0
        if (Ctr0_Cached)
        {
            #if PREDICTION_DEBUG
            std::cout<<"++xx++ DO. Ctr0 Cached. Allocate new one "<<curCycle()<<std::endl;
            #endif

            if (getRQueueVacancy() < 1)
            {
                if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
                return;
            }
            uint8_t chunk[64];
            RequestPtr req0;
            EncGenPacket(pkt_encBuild,req0,preDataAddr,64,chunk,0);
            bool hitbuffer = EncAccessPacket(pkt_encBuild);
            if (hitbuffer)
            {
                teePrediction.upadateCipherText(groupPreTInx,pkt_encBuild->getPtr<uint8_t>());
                stateEnc = StatesEnc::PREDICTION_CAL;
            }
            else
            {
                prefetchCounter++;
                stateEnc = StatesEnc::PREDICTION_PREFETCH_WAIT;
            }
        }
        // else
        // {
        //     if (getRQueueVacancy() < 2)
        //     {
        //         if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
        //         return;
        //     }
        //     int hitBufferNum=0;

        //     uint8_t chunk[64];
        //     RequestPtr req0;
        //     EncGenPacket(pkt_encBuild,req0,preDataAddr,64,chunk,0);
        //     bool hitbuffer = EncAccessPacket(pkt_encBuild);
        //     if (hitbuffer)
        //     {
        //         teePrediction.upadateCipherText(groupPreTInx,pkt_encBuild->getPtr<uint8_t>());
        //         hitBufferNum ++;
        //     }
        //     else
        //     {
        //         prefetchCounter++;
        //     }
            
        //     uint8_t chunk1[64];
        //     RequestPtr req1;
        //     EncGenPacket(pkt_encBuild,req1,preCtr0Addr,64,chunk1,0);
        //     hitbuffer = EncAccessPacket(pkt_encBuild);
        //     if (hitbuffer)
        //     {
        //         teePrediction.upadateCtr0(groupPreTInx,pkt_encBuild->getPtr<uint8_t>());
        //         hitBufferNum ++;
        //     }
        //     else
        //     {
        //         prefetchCounter++;
        //     }

        //     if (hitBufferNum == 2)
        //     {
        //         stateEnc = StatesEnc::PREDICTION_CAL;
        //     }
        //     else
        //     {
        //         stateEnc = StatesEnc::PREDICTION_PREFETCH_WAIT;
        //     }
        // }

        #if PREDICTION_DEBUG
        std::cout<<"++xx++ need fetch data num: "<<prefetchCounter<<" "<<curCycle()<<std::endl;
        #endif
    }

    if (stateEnc == StatesEnc::PREDICTION_PREFETCH_WAIT)
    {
        return;
    }
    
    if (stateEnc == StatesEnc::PREDICTION_CAL)
    {
        #if PREDICTION_DEBUG
        std::cout<<"++xx++ CAL."<<curCycle()<<std::endl;
        #endif

        // teePrediction.CalAllReadyContent(groupPreTInx, ENCALVE_MASK);
        stateEnc = StatesEnc::PREDICTION_DONE;
        
        // hashGroup.searchPTable2attach(teePrediction,curCycle());
        // uint64_t hashGroupReady = hashGroup.getReadyTime(curCycle());
        // // assert(!hashGroup.isEmpty());
        // if (!hashHardwareEvent.scheduled())
        // {
        //     schedule(hashHardwareEvent,curTick() + hashGroupReady*CYCLETICK);
        // }

        if (WriteIf)
        {
            assert(hashLock.isLock());
            PrefetchCycle = hashLock.LockTime(curCycle());
            stats.EncPrefetchCycle += PrefetchCycle;
            EncS.EncPrefetchCycle += PrefetchCycle;
            gapPre.updatePrefetch(PrefetchCycle);
            stats.EncPrefetchTime ++;
            EncS.EncPrefetchTime ++;
            hashLock.relase();
            std::cout<<"  w prefetch cycle: "<<PrefetchCycle<<" "<<std::hex<<splitOriPkt[0]<<" "<<preDataAddr<<std::dec<<std::endl;
            if (PrefetchCycle < hashC)
            {
                EncS.EncPrefetchHashOverlap += (hashC-PrefetchCycle);
                assert(!EncEvent.scheduled());
                stateEnc = StatesEnc::WBLIMB;
                schedule(EncEvent, curTick() + (hashC-PrefetchCycle)*CYCLETICK );
                return;
            }
            // hashLock.lock(curCycle());
        }
        else
        {
            assert(!hashLock.isLock());
            PrefetchCycle = curCycle() -PrefetchCycle;
            stats.EncPrefetchCycle += PrefetchCycle;
            EncS.EncPrefetchCycle += PrefetchCycle;
            gapPre.updatePrefetch(PrefetchCycle);
            stats.EncPrefetchTime ++;
            EncS.EncPrefetchTime ++;
            if (PrefetchCycle < overlapHashRDMAC)
            {
                EncS.EncPrefetchHashOverlap += (overlapHashRDMAC - PrefetchCycle);
                assert(!EncEvent.scheduled());
                schedule(EncEvent, curTick() + (overlapHashRDMAC-PrefetchCycle)*CYCLETICK );
                return;
            }
            // hashLock.lock(curCycle());
        }
        //we lock the hash hardware for prediction cal
    }

    if (stateEnc==StatesEnc::PREDICTION_DONE)
    {
        #if PREDICTION_DEBUG
        std::cout<<"++xx++ DONE. PREDOCTION DONE. need Retry? "<<predictingRetry<<" "<<curCycle()<<std::endl;
        #endif
        teePrediction.CalAllReadyContent(groupPreTInx, ENCALVE_MASK);
        hashGroup.searchPTable2attach(teePrediction,curCycle());
        uint64_t hashGroupReady = hashGroup.getReadyTime(curCycle());
        // assert(!hashGroup.isEmpty());
        if (!hashHardwareEvent.scheduled())
        {
            schedule(hashHardwareEvent,curTick() + hashGroupReady*CYCLETICK);
        }

        // StartPreCalCycle = curCycle();
        stateEnc = StatesEnc::DATATRAN;
        
        if (WriteIf)
        {
            stateEnc = StatesEnc::WBLIMB;
            if (!EncEvent.scheduled()) schedule(EncEvent, curTick());
        }
        

    }

    if (stateEnc == StatesEnc::DATATRAN)
    {

        #if TURNON_PRE
            //--------------------- wxr 10.8
            TableInx tGroup;
            bool findGroup = teePrediction.findGroupRange(splitOriPkt[splitOriPkt_cnt],tGroup);
            if (findGroup)
            {
                teePrediction.upadataCtrChain(tGroup,vault_engine);
                #if PREDICTION_DEBUG
                std::cout<<"++xx++ CNTOP UPDATE CTRCHAIN. "<<" "<<curCycle()<<std::endl;
                #endif
                if (WriteIf)
                {
                    for (auto itr = teePrediction.getGroup(tGroup).TableEntry.begin(); itr != teePrediction.getGroup(tGroup).TableEntry.end(); itr++)
                    {
                        (*itr).CipherTextCal = false;
                    }

                    for (auto itr = teePrediction.getGroup(tGroup).AesEntry.begin(); itr != teePrediction.getGroup(tGroup).AesEntry.end(); itr++)
                    {
                        (*itr).caled = false;
                    }
                }
                
            }
        #endif
        
        #if TURNON_PRE_GAP
        // bool decideGapGo = gapPre.cmp(PRE_GAP_THREHOLD);
        // bool decideGapGo = gapPre.getAvgGap()>gapPre.getAvgPrefetch();
        bool decideGapGo = gapPre.cmp();
        if (decideGapGo && !isChecking)
        {
            stateEnc = StatesEnc::PREDICTION_GAP_PREFETCH;
            PrefetchCycle = curCycle();
        }
        else
        {
            stateEnc = StatesEnc::READY;
            preCycle = curCycle();
            if (isChecking)
            {
                isChecking = false;
                #if STATE_DEBUG 
                std::cout<<"----MAIN STATEMACHINE <<<<==== ====>>>> ready to send RETRY"<<std::endl;
                #endif
                port.sendRetryReq();
            }
        }

        #else
        stateEnc = StatesEnc::READY;
        preCycle = curCycle();
        if(!WriteIf)
            stats.EncReadAllLatency += curCycle() - Lat_cycle;
        if (isChecking)
        {
            isChecking = false;
            #if STATE_DEBUG 
            std::cout<<"----MAIN STATEMACHINE <<<<==== ====>>>> ready to send RETRY"<<std::endl;
            #endif
            port.sendRetryReq();
        }
        #endif

    }

    if (stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH)
    {
        Addr nowDataAddr;
        Addr nowCtrAddr=0;
        if (!WriteIf)
        {
            nowDataAddr = splitOriPkt[splitOriPkt_cnt];
            nowCtrAddr = vault_engine.counterOP[0].addr;
            preDataAddr = nowDataAddr;
        }
        else
        {
            nowDataAddr = ReadDataTransAddrRecord;
            preDataAddr = nowDataAddr;
            assert(teePrediction.findGroupRange(preDataAddr,groupPreTInx,true));
        }
        teePrediction.PredictionAddr(preDataAddr,groupPreTInx);

        assert(teePrediction.findGroupRange(preDataAddr,groupPreTInx,true));
        teePrediction.alloc_Entry(preDataAddr,curCycle(),0);
        hashGroup.updateNoneedHash(teePrediction);

        groupPreTInx = TableInx{teePrediction.GroupNum-1,teePrediction.GroupEntryNum-1};
        teePrediction.upadateCtr0(groupPreTInx,vault_engine.counterOP[0].blk);
        teePrediction.upadataCtrChain(groupPreTInx,vault_engine);
        stateEnc = StatesEnc::PREDICTION_GAP_PREFETCH_DO;

    }

    if (stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH_DO)
    {
        if (getRQueueVacancy() < 1)
        {
            if (!nextReqEvent.scheduled()) schedule(nextReqEvent, curTick());
            return;
        }
        uint8_t chunk[64];
        RequestPtr req0;
        EncGenPacket(pkt_encBuild,req0,preDataAddr,64,chunk,0);
        bool hitbuffer = EncAccessPacket(pkt_encBuild);
        if (hitbuffer)
        {
            teePrediction.upadateCipherText(groupPreTInx,pkt_encBuild->getPtr<uint8_t>());
            stateEnc = StatesEnc::PREDICTION_GAP_CAL;
        }
        else
        {
            stateEnc = StatesEnc::PREDICTION_GAP_PREFETCH_WAIT;
        }
    }

    if (stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH_WAIT)
    {
        return;
    }

    if (stateEnc == StatesEnc::PREDICTION_GAP_CAL)
    {
        PrefetchCycle = curCycle() - PrefetchCycle;
        stats.EncPrefetchCycle += PrefetchCycle;
        EncS.EncPrefetchCycle += PrefetchCycle;
        gapPre.updatePrefetch(PrefetchCycle);
        stats.EncPrefetchTime ++;
        EncS.EncPrefetchTime ++;
        EncS.EncPrefetchGapPreTime ++;

        hashGroup.searchPTable2attach(teePrediction,curCycle());
        uint64_t hashGroupReady = hashGroup.getReadyTime(curCycle());
        // assert(!hashGroup.isEmpty());
        if (!hashHardwareEvent.scheduled())
        {
            schedule(hashHardwareEvent,curTick() + hashGroupReady*CYCLETICK);
        }
        stateEnc = StatesEnc::PREDICTION_GAP_DONE;
    }

    if (stateEnc == StatesEnc::PREDICTION_GAP_DONE)
    {
        stateEnc = StatesEnc::READY;
        preCycle = curCycle();
        if (isChecking)
        {
            isChecking = false;
            #if STATE_DEBUG 
            std::cout<<"----MAIN STATEMACHINE <<<<==== ====>>>> ready to send RETRY"<<std::endl;
            #endif
            port.sendRetryReq();
        }
    }

}

//wxr-----------------------
bool
MemCtrl::recvTimingReq(PacketPtr pkt)
{
    //wxr++++++++++
    // std::cout<<"PACKET: "<<pkt->print()<<" "<<pkt->requestorId()<<" "<<std::endl;
    acc ++;
    if (acc%100000 == 0)
    {
        // printf("===: %ld\n",acc);
    }

    #if PREDICTION_TAG1
    if (stateEnc==StatesEnc::READY)
    {
        memAccessCtr++;
        if (memAccessCtr%100000 == 0)
        {
            std::cout<<"predication dump: "<<std::dec<<EncS.EncPredictionSuccess<<" / "<<EncS.EncReadReq<<" wait( "<<EncS.EncPreWaitCaled<<" ) totalPrefetch: "<<EncS.EncPrefetchTime<<" gap prediction Time: "<<EncS.EncPrefetchGapPreTime<<" Write prediction status "<<EncS.EncPredictionWriteSuccess<<" / "<<EncS.EncWriteReq<<" PrefetchCycle: "<<EncS.EncPrefetchCycle<<" HashGroup Average: "<<double(EncS.EncHashGroupBusyNum) / (EncS.EncHashGroupNum)<<" Mem Avg.Gap: "<<double(EncS.EncTotGap)/double(EncS.EncWriteReq+EncS.EncReadReq)<<" Hash Max NUM: "<<EncS.EncHashGroupBusyMax<<" aesCycle: "<<EncS.EncReadAesBias<<" find in AES entry: "<<EncS.EncPredictionAesSuccess<<" EncCacheHit: "<<float(EncS.EncCacheRd_hit)/EncS.EncCacheRd<<" EncMacheHit: "<<float(EncS.MacCacheRd_hit)/EncS.MacCacheRd<<" "<<std::dec<<curCycle()<<std::endl;
        }
    }
    #endif
    
    

    bool pktNoSignal = (pkt->getAddrRange().end() < ENCLAVE_STARTADDR) || (pkt->getAddrRange().start() > (ENCLAVE_STARTADDR+ENCLAVE_SIZE));
    poolDelete();

    stats.EncEndCycle = curCycle();
    stats.EncTotInst= o3Inst;
    stats.EncEndTimeOut = uint64_t(curCycle());
    cache2EncS();
    EncS2stats();

    
    if(stateEnc == StatesEnc::IDLE)
    {
        // stateEnc = StatesEnc::INIT;
        // stateEnc = StatesEnc::IDLE;
        // if (acc == 27000000)
        // {
            stateEnc = StatesEnc::INIT;
        // }
        std::cout<<"====<<<<ECNALVE>>>>====: StateEnc IDLE"<<std::endl;
    }

    if(stateEnc == StatesEnc::INIT)
    {
        /************************************** comment by xrwang
        In this state, successive packets written to mem is tested.
        Return false will jump to the next event scheduled before.
        To understand the workflow of gem5, take notice of the callback designs.
        Constructed at the beginning of the MemCtrl() constructor, the callback
        is assigned with processNextReqEvent() and processNextRespEvent(), which
        means as if we return flase in this recvTimingReq() function above two
        functions will be called to begin the next event.
        ***************************************/
        // std::cout<<"----MAIN STATEMACHINE <<<<==== StateEnc INIT ====>>> ";
        #if STATE_DEBUG 
        std::cout<<"PACKET: "<<pkt->print()<<" "<<getWQueueVacancy()<<" "<<std::dec<<curCycle()<<std::endl;
        #endif
        /*
        if (!pre_issue)
        {
            pre_issue = CounterInit();

        }
        
        if (pre_issue)
        {
            // std::cout<<"----MAIN STATEMACHINE <<<<==== The counter initial done ====>>>> "<<std::endl;
            stateEnc = StatesEnc::POLLING;
            pre_issue = false;
        }
        else
        {
        }


        retryWrReq = true;
        stats.numWrRetry++;
        
        return false;
        */
        stateEnc = StatesEnc::POLLING;
    }


    // bool inPredicting = (stateEnc == StatesEnc::PREDICTION_CAL) || (stateEnc == StatesEnc::PREDICTION_DONE) || 
    //                     (stateEnc == StatesEnc::PREDICTION_PREFETCH_DECIDE) || (stateEnc == StatesEnc::PREDICTION_PREFETCH_DO) ||
    //                     (stateEnc == StatesEnc::PREDICTION_PREFETCH_WAIT);

    // if (inPredicting)
    // {
    //     #if PREDICTION_DEBUG
    //     std::cout<<"++---++ recv new pkt durting PREDICTION"<<std::endl;
    //     #endif
    //     EncS.EncPredictionRecvNew ++;
    //     predictingRetry = true;
    //     if (equal2Global)
    //     {
    //         infoR.updatePkt(pkt);
    //     }
    //     if(!EncEvent.scheduled())
    //         schedule(EncEvent, curTick());
    //     return false;
    // }

    if (stateEnc != StatesEnc::POLLING && stateEnc != StatesEnc::READY)
    {
        isChecking = true;
        #if STATE_DEBUG
        std::cout<<"---- Receive new req when checking"<<std::endl;
        #endif
        return false;
    }
    

    // This is where we enter from the outside world
    DPRINTF(MemCtrl, "recvTimingReq: request %s addr %#x size %d\n",
            pkt->cmdString(), pkt->getAddr(), pkt->getSize());

    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see read and writes at memory controller\n");

    // Calc avg gap between requests
    if (prevArrival != 0) {
        stats.totGap += curTick() - prevArrival;
    }
    prevArrival = curTick();

    if (preCycle != 0)
    {
        if (stateEnc != StatesEnc::POLLING)
        {
            EncS.EncTotGap += (curCycle() - preCycle);
            gapPre.updateGap(curCycle() - preCycle);
        }
    }
    

    panic_if(!(dram->getAddrRange().contains(pkt->getAddr())),
             "Can't handle address range for packet %s\n", pkt->print());

    // Find out how many memory packets a pkt translates to
    // If the burst size is equal or larger than the pkt size, then a pkt
    // translates to only one memory packet. Otherwise, a pkt translates to
    // multiple memory packets
    unsigned size = pkt->getSize();
    uint32_t burst_size = dram->bytesPerBurst();

    unsigned offset = pkt->getAddr() & (burst_size - 1);
    unsigned int pkt_count = divCeil(offset + size, burst_size);

    // run the QoS scheduler and assign a QoS priority value to the packet
    qosSchedule( { &readQueue, &writeQueue }, burst_size, pkt);

    bool oriHitBuffer = false;

    // check local buffers and do not accept if full
    if (pkt->isWrite()) {
        assert(size != 0);
        /////////////////////////////
        ///record wrie
        ////////////////////////////
        memcpy(writeDataRecord,pkt->getPtr<uint8_t>(),64);


        if (writeQueueFull(pkt_count)) {
            DPRINTF(MemCtrl, "Write queue full, not accepting\n");
            // remember that we have to retry this port
            retryWrReq = true;
            stats.numWrRetry++;
            return false;
        } else {
            addToWriteQueue(pkt, pkt_count, dram);
            // If we are not already scheduled to get a request out of the
            // queue, do so now
            if (!nextReqEvent.scheduled()) {
                DPRINTF(MemCtrl, "Request scheduled immediately\n");
                schedule(nextReqEvent, curTick());
            }
            stats.writeReqs++;
            stats.bytesWrittenSys += size;
        }
    } else {
        assert(pkt->isRead());
        assert(size != 0);
        if (readQueueFull(pkt_count)) {
            DPRINTF(MemCtrl, "Read queue full, not accepting\n");
            // remember that we have to retry this port
            retryRdReq = true;
            stats.numRdRetry++;
            return false;
        } else {
            hasResponseRead = false;
            if (!addToReadQueue(pkt, pkt_count, dram)) {
                // If we are not already scheduled to get a request out of the
                // queue, do so now
                if (!nextReqEvent.scheduled()) {
                    DPRINTF(MemCtrl, "Request scheduled immediately\n");
                    schedule(nextReqEvent, curTick());
                }
            }
            else
            {
                memcpy(readDataRecord,pkt->getPtr<uint8_t>(),64);
                hasResponseRead = true;
            }
            stats.readReqs++;
            stats.bytesReadSys += size;
        }
    }


    bool newEpoch=false;
    bool encFlag = int(pkt->getPtr<uint8_t>()[0]) == ENC_VALUE;
    #if ATT1
    if(encFlag && pkt->getAddr() == ENCLAVE_STARTADDR && pkt->isWrite())
    {
        receiveAtt1 = true;
        std::cout<<"Ready to perform regular physical attack..."<<std::endl;
    }
    #elif ATT2
    if(encFlag && pkt->getAddr() == ENCLAVE_STARTADDR && pkt->isWrite())
    {
        receiveAtt2 = true;
        std::cout<<"Ready to perform replay attack..."<<std::endl;
    }
    #else
    #endif

    if(stateEnc == StatesEnc::POLLING)
    {
        //*****************************************comment by xrwang
        //In order to controll by users of the memory integrity protection,
        //a specail memory writing like a write to ENCALAVE STARTADDR is choosen
        //to flag the protection beginning. So as this setting, we instrument
        //the specail writing at the beginning of protected program.
        #if INPROG
            stateEnc = StatesEnc::READY;
            EncS.EncBeginCycle = curCycle();
            stats.EncBeginCycle = curCycle();
            stats.EncBeginTimeout= uint64_t(curCycle());
        #else
        if (pkt->getAddr() == ENCLAVE_STARTADDR && pkt->isWrite() && encFlag)
        {
            #if STATE_DEBUG 
            std::cout<<"recvTiming <<<<==== StateEnc POLLING to READY ====>>> "<<std::hex<<pkt->getAddr()<<std::dec<<" taskId: "<<pkt->req->taskId()<<std::endl;
            #endif
            GlobalThreadID = pkt->req->taskId();
            stateEnc = StatesEnc::READY;
            newEpoch=true;
            
            //we begin to initial the status
            EncS.EncBeginCycle = curCycle();
            stats.EncBeginCycle = curCycle();
        }
        #endif
    }

    int thisId = pkt->req->taskId();
    bool equal2Global = thisId == GlobalThreadID;
    equal2Global = true;
        
    if (stateEnc == StatesEnc::READY && pktNoSignal && ENC_PROTECT && !enc_endtest)
    {
        if (pkt->isWrite())
        {
            EncS.EncWriteReq ++;
            writeHashNum = 5;
            reduceNum = 0;
            WriteDataTransAddrRecord = pkt->getAddr();
        }
        if (pkt->isRead())
        {
            EncS.EncReadReq ++;

            //*************record the addr to get the Cycles
            ReadDataTransAddrRecord = pkt->getAddr();
            ReadDataTransCycle = curCycle();

            Lat_cycle = curCycle();
            noNeedTreeCheck = false;
        }

        uint8_t xx[64] = {0};
        if (memcmp(xx,pkt->getPtr<uint8_t>(),64) == 0)
        {
            equal0 ++;
            if (pkt->isRead())
            {
                // std::cout<<"R  --------+++++++++ "<<equal0<<" "<<EncS.EncReadReq<<" "<<std::hex<<pkt->getAddr()<<std::dec<<std::endl;
            }
            else
            {
                // std::cout<<"W  --------+++++++++ "<<equal0<<" "<<EncS.EncWriteReq<<" "<<std::hex<<pkt->getAddr()<<std::dec<<std::endl;
            }
        }

        if (memcmp(LastWrite,pkt->getPtr<uint8_t>(),64) == 0)
        {
            equalSame ++;
            if (pkt->isWrite())
            {
                // std::cout<<"W  +++++++++++++---------------"<<equalSame<<" "<<EncS.EncWriteReq<<" "<<std::hex<<pkt->getAddr()<<" "<<pkt->getSize()<<std::dec<<std::endl;
            }
        }


        if (pkt->isWrite())
        {
            memcpy(LastWrite,pkt->getPtr<uint8_t>(),64);
        }
        else{
            memcpy(LastRead,pkt->getPtr<uint8_t>(),64);
        }
        
        
        #if STATE_DEBUG 
        std::cout<<"DATATRAN <<<<==== ORI DATA trans ====>>>> "<<std::hex<<pkt->getAddr()<<std::dec<<" if thisThread: "<<equal2Global<<std::endl;
        #endif

        WriteIf = pkt->isWrite() ? true : false;
        EncPreC = curCycle();
        stateEnc = StatesEnc::RDCNT;
        int pktBlkSize = (pkt->getSize() + 63) / 64;

        splitOriPkt_cnt = 0;
        splitOriPkt.clear();
        RDCNTwRpkt_cnt = 0;
        RDCNTwRpkt.clear();
        RDCNTrDpkt_cnt = 0;
        RDCNTrDpkt.clear();
        RDCNTneedRD = false;
        RDCNTneedWB = false;

        encCache.RdMiss_ptr.clear();
        encCache.RdMissAddr.clear();
        macCache.RdMiss_ptr.clear();
        macCache.RdMissAddr.clear();
        

        RDCNTrecvCNT = 0;

        for (size_t i = 0; i < pktBlkSize; i++)
        {
            splitOriPkt.push_back(((pkt->getAddr() - (pkt->getAddr()&0x3f)))+ i*64);
        }

        #if STATE_DEBUG 
        std::cout<<"recvTiming <<<<==== StateEnc READY to RDCNT ====>>> addr: "<<std::hex<<pkt->getAddr()<<std::dec<<std::endl;
        #endif
        isChecking = false;
        schedule(EncEvent, curTick());


        //only for read hit the write buffer
        if (ENC_PROTECT && hasResponseRead && pkt->isRead())
        {
            bool hasWait = false;
            uint64_t hasWaitTime = 0;
            #if TURNON_PRE
            int source_id;
            int find = teePrediction.findMatchInterface(ReadDataTransAddrRecord);

            #if TURNON_PRE_AES
            bool findAes = teePrediction.findAesResult(ReadDataTransAddrRecord);
            #else
            bool findAes = false;
            #endif
            // if (findAes)
            // {
            //     EncS.EncPredictionAesSuccess ++;
            // }
            // we first add the time
            bool hasCorrectPrediction = find ==1 || find ==2 || findAes;
            if (hasCorrectPrediction)
            {
                if (find==1 || find == 2)
                {
                    EncS.EncPredictionNorAesSuccess++;
                }
                else if(findAes)
                {
                    EncS.EncPredictionAesSuccess ++;
                }
                EncS.EncPredictionSaveAes += (encryptC);
            }
            else
            {
                EncS.EncReadAesBias += (encryptC );
                hasWait = true;
                hasWaitTime = (encryptC);
            }
            #else
            EncS.EncReadAesBias += (encryptC);
            hasWait = true;
            #endif

            if (hasWait)
            {
                reschedule(EncEvent,curTick()+hasWaitTime * CYCLETICK, true);
            }
            else
            {
                if (!EncEvent.scheduled())
                schedule(EncEvent,curTick());
            }
        }



        // EncS.EncBeginCycle = curCycle();
    }
    #if ATT1
    else if (false)
    #elif ATT2
    else if (false)
    #else
    else if ((!newEpoch && stateEnc == StatesEnc::READY && ((pkt->getAddr() == ENCLAVE_STARTADDR && pkt->isWrite() && encFlag))) || enc_endtest)
    #endif
    {
        stateEnc = StatesEnc::POLLING;
        enc_endtest = false;
        #if STATE_DEBUG 
        std::cout<<"recvTiming <<<<==== StateEnc READY to POLLING ====>>> "<<std::endl;
        #endif
        //print out the status
        EncS.EncCacheAccessTime = encCache.EncS.tRD + encCache.EncS.tWR;
        EncS.EncCacheRd = encCache.EncS.tRD;
        EncS.EncCacheWr = encCache.EncS.tWR;
        EncS.EncCacheRd_hit= encCache.EncS.tRDhit;
        EncS.EncCacheWr_hit= encCache.EncS.tWRhit;

        EncS.MacCacheRd = macCache.EncS.tRD;
        EncS.MacCacheWr = macCache.EncS.tWR;
        EncS.MacCacheRd_hit= macCache.EncS.tRDhit;
        EncS.MacCacheWr_hit= macCache.EncS.tWRhit;

        EncS.EncEndCycle = curCycle();
        EncS.print();
        exit(0);
    }



    


    return true;
}


void
MemCtrl::hashProcess()
{
    //both the tee entry allocation and lookup are at same time
    #if HASHDEBUG
    std::cout<<"----HASH getinto hashProcess  curCycle "<<std::dec<<curCycle()<<std::endl;
    #endif
    EncS.EncHashGroupNum++;
    EncS.EncHashGroupBusyNum += hashGroup.getBusyNum();
    if (hashGroup.getBusyNum() > EncS.EncHashGroupBusyMax)
    {
        EncS.EncHashGroupBusyMax = hashGroup.getBusyNum();
    }
    
    hashGroup.release(teePrediction,curCycle());
    hashGroup.searchPTable2attach(teePrediction,curCycle());
    if (!hashGroup.isEmpty())
    {
        uint64_t readyTime = hashGroup.getReadyTime(curCycle());
        //to find if there remains entry need to be calculated
        //if not, wait for next tee entry allocaltion
        #if HASHDEBUG
        std::cout<<"----HASH getinto hashProcess  next ready time "<<std::dec<<readyTime<<std::endl;
        #endif
        if (!hashHardwareEvent.scheduled())
        {
            schedule(hashHardwareEvent, curTick()+ CYCLETICK*readyTime);
        }
    }

}


void
MemCtrl::processRespondEvent(MemInterface* mem_intr,
                        MemPacketQueue& queue,
                        EventFunctionWrapper& resp_event,
                        bool& retry_rd_req)
{

    DPRINTF(MemCtrl,
            "processRespondEvent(): Some req has reached its readyTime\n");

    MemPacket* mem_pkt = queue.front();

    // media specific checks and functions when read response is complete
    // DRAM only
    mem_intr->respondEvent(mem_pkt->rank);

    // std::cout<<"processRespondEvent <<<<==== curCycle ====>>>> "<<std::dec<<curCycle()<<" "<<std::endl;
    if (mem_pkt->burstHelper) {
        // it is a split packet
        mem_pkt->burstHelper->burstsServiced++;
        if (mem_pkt->burstHelper->burstsServiced ==
            mem_pkt->burstHelper->burstCount) {
            // we have now serviced all children packets of a system packet
            // so we can now respond to the requestor
            // @todo we probably want to have a different front end and back
            // end latency for split packets
            accessAndRespond(mem_pkt->pkt, frontendLatency + backendLatency,
                             mem_intr);
            delete mem_pkt->burstHelper;
            mem_pkt->burstHelper = NULL;
        }
    } else {
        // it is not a split packet
        accessAndRespond(mem_pkt->pkt, frontendLatency + backendLatency,
                         mem_intr);
    }

    //+++++++++++++++++++++++++++wxr
    if (stateEnc == StatesEnc::RDCNT && mem_pkt->pkt->EncFlag)
    {
        #if STATE_DEBUG 
        std::cout<<"processRespondEvent <<<<==== response pkt ====>>>> "<<mem_pkt->pkt->print()<<" "<<curTick()<<" "<<curCycle()<<std::endl;
        std::cout<<"processRespondEvent <<<<==== response pkt ====>>>> "<<int(mem_pkt->pkt->getPtr<uint8_t>()[0])<<std::endl;
        #endif
        

        RDCNTrecvCNT ++;
        int index = encCache.EncRefill(mem_pkt->pkt->getAddr(),mem_pkt->pkt->getPtr<uint8_t>());
        memcpy((vault_engine.counterOP+index)->blk,mem_pkt->pkt->getPtr<uint8_t>(),64);
        (vault_engine.counterOP+index)->addr=mem_pkt->pkt->getAddr();

        if (RDCNTrecvCNT == (RDCNTwRpkt.size() + RDCNTrDpkt.size()))
        {
            if (WriteIf)
            {
                stateEnc = StatesEnc::CNTOP;
                memAccessTap.C_RDCNT = curCycle() - EncPreC;
                EncPreC = curCycle();
                #if STATE_DEBUG 
                std::cout<<"processRespondEvent <<<<==== state RDCNT to CNTOP ====>>>> "<<std::endl;
                #endif
            }
            else
            {
                stateEnc = StatesEnc::CNTSANITY;
                memAccessTap.C_RDCNT = curCycle() - EncPreC;
                EncPreC = curCycle();
                // stateEnc = StatesEnc::CNTOP;
                #if STATE_DEBUG 
                std::cout<<"processRespondEvent <<<<==== state RDCNT to CNTSANITY ====>>>> "<<std::endl;
                #endif
            }
            EncS.EncRDCNTC += curCycle() - EncPreC;
            EncPreC = curCycle();
            for (size_t i = 0; i < vault_engine.MT_LEVEL-2; i++)
            {
                #if STATE_DEBUG 
                std::cout<<"processRespondEvent <<<<==== ====>>>> "<<std::hex<<(vault_engine.counterOP+i)->addr<<std::dec<<" "<<int((vault_engine.counterOP+i)->blk[0])<<std::endl;
                #endif
            }
            
        }
    }

    if (stateEnc == StatesEnc::DORESETTREE && mem_pkt->pkt->EncFlag)
    {
        if (stateRSTEnc == RstState::WaitResp)
        { 
            #if STATE_DEBUG 
            std::cout<<"processRespondEvent <<<<==== state WaitResp pkt ====>>>> "<<mem_pkt->pkt->print()<<std::endl;
            #endif
            encCache.EncRefill(mem_pkt->pkt->getAddr(),mem_pkt->pkt->getPtr<uint8_t>());
            encCache.EncWrite(mem_pkt->pkt->getAddr(),mem_pkt->pkt->getPtr<uint8_t>());
            DORESETTREErDpkt.erase(DORESETTREErDpkt.begin());

            encCache.RdMiss_ptr.clear();
            encCache.RdMissAddr.clear();

            if (DORESETTREErDpkt.size() == 0)
            {
                if (splitOriPkt_cnt == (splitOriPkt.size()-1))
                {
                    stateEnc = StatesEnc::DATATRAN;
                }
                else
                {
                    stateEnc = StatesEnc::SPLITAG;
                }
            }
            else
            {
                stateRSTEnc = RstState::TREErdEncCache;
            }

            Cycles ResetTranCycle = curCycle() - ResetCycleRecord[0]; 
            if (ResetTranCycle < hashC )
            {
                EncS.EncResetHashBias += (hashC - ResetTranCycle);
            }
            
        }
        if (stateRSTEnc == RstState::DATA_W_R || stateRSTEnc == RstState::DATA_R)
        { 
            #if STATE_DEBUG 
            std::cout<<"processRespondEvent <<<<==== recv data new encrypt ====>>>> "<<DORESETTREEdata_recv<<" "<<mem_pkt->pkt->print()<<std::endl;
            #endif
            mem_pkt->pkt->real_resp = false;
            for (size_t i = 0; i < 63; i++)
            {
                if (DORESETTREErDpkt[i]->getAddr() == mem_pkt->pkt->getAddr())
                {
                    memcpy(DORESETTREErDpkt[i]->getPtr<uint8_t>(),mem_pkt->pkt->getPtr<uint8_t>(),64);
                }
            }
            
            DORESETTREEdata_recv ++;
            if (DORESETTREEdata_recv == 63)
            {
                stateRSTEnc = RstState::DATA_W;
                DORESETTREEdata_cnt = 0;
            }

            int indexReset;
            if (findResetAddr(mem_pkt->pkt->getAddr(), indexReset))
            {
                Cycles ResetTranCycle = curCycle() - ResetCycleRecord[indexReset]; 
                if ( ResetTranCycle < encryptC)
                {
                    EncS.EncResetAesBias += (encryptC - ResetTranCycle);
                }
                if ( ResetTranCycle < encryptC)
                {
                    EncS.EncResetHashBias += (hashC - ResetTranCycle);
                }
            }
            else{
                std::cout<<"ERROR! no reset reposne"<<std::endl;
                exit(0);
            }
        }
    }
    
    if (stateEnc == StatesEnc::RRDMAC_RESP && mem_pkt->pkt->EncFlag)
    {
        #if STATE_DEBUG 
        std::cout<<"processRespondEvent <<<<==== recv RDMAC REFILL ====>>>> "<<mem_pkt->pkt->print()<<std::endl;
        #endif
        //////////////////
        //to record the MAC
        /////////////////
        memcpy(RMAC,mem_pkt->pkt->getPtr<uint8_t>(),64);



        macCache.EncRefill(mem_pkt->pkt->getAddr(),mem_pkt->pkt->getPtr<uint8_t>());
        if (splitOriPkt_cnt == (splitOriPkt.size()-1))
        {
            stateEnc = StatesEnc::RDONE;
        }
        else
        {
            stateEnc = StatesEnc::SPLITAG;
        }
    }
    
    if (stateEnc == StatesEnc::PREDICTION_PREFETCH_WAIT && mem_pkt->pkt->EncFlag)
    {
        #if PREDICTION_DEBUG 
        std::cout<<"++xx++ WAIT recv "<<mem_pkt->pkt->print()<<" "<<curCycle()<<std::endl;
        #endif
        if (mem_pkt->pkt->getAddr() == preDataAddr)
        {
            teePrediction.upadateCipherText(groupPreTInx,mem_pkt->pkt->getPtr<uint8_t>());
            prefetchRecvCounter ++;
        }

        if (mem_pkt->pkt->getAddr() == preCtr0Addr)
        {
            teePrediction.upadateCtr0(groupPreTInx,mem_pkt->pkt->getPtr<uint8_t>());
            prefetchRecvCounter ++;
        }

        if (prefetchCounter == prefetchRecvCounter)
        {
            stateEnc = StatesEnc::PREDICTION_CAL;
            if (!EncEvent.scheduled())
            {
                schedule(EncEvent,curTick());
            }
            
        }
    }

    if (stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH_WAIT && mem_pkt->pkt->EncFlag)
    {
        if (mem_pkt->pkt->getAddr() == preDataAddr)
        {
            teePrediction.upadateCipherText(groupPreTInx,mem_pkt->pkt->getPtr<uint8_t>());
            stateEnc = StatesEnc::PREDICTION_GAP_CAL;
            if (!EncEvent.scheduled())
            {
                schedule(EncEvent,curTick());
            }
        }
    }
    

    if (stateEnc!=StatesEnc::IDLE && stateEnc!=StatesEnc::INIT && stateEnc!=StatesEnc::POLLING && (mem_pkt->pkt->getAddr() == ReadDataTransAddrRecord))
    {
        hasResponseRead=true;
        memcpy(readDataRecord,mem_pkt->pkt->getPtr<uint8_t>(),64);
        ReadDataTransCycle = curCycle() - ReadDataTransCycle;
        if (ENC_PROTECT)
        {
            bool hasWait = false;
            uint64_t hasWaitTime = 0;
            if (ReadDataTransCycle >encryptC)
            {

            }
            else
            {
                #if TURNON_PRE
                int source_id;
                int find = teePrediction.findMatchInterface(ReadDataTransAddrRecord);
                #if TURNON_PRE_AES
                bool findAes = teePrediction.findAesResult(ReadDataTransAddrRecord);
                #else
                bool findAes = false;
                #endif
                if (findAes)
                {
                    EncS.EncPredictionAesSuccess ++;
                }
                
                // we first add the time
                bool hasCorrectPrediction = find ==1 || find==2 || findAes;
                if (hasCorrectPrediction)
                {
                    EncS.EncPredictionSaveAes += (encryptC - ReadDataTransCycle);
                }
                else
                {
                    EncS.EncReadAesBias += (encryptC - ReadDataTransCycle);
                    hasWait = true;
                    hasWaitTime = (encryptC - ReadDataTransCycle);
                }
                #else
                EncS.EncReadAesBias += (encryptC-ReadDataTransCycle);
                hasWaitTime = (encryptC - ReadDataTransCycle);
                hasWait = true;
                #endif
            }

            if (hasWait)
            {
                reschedule(EncEvent,curTick()+hasWaitTime * CYCLETICK, true);
            }
            else
            {
                if (!EncEvent.scheduled())
                schedule(EncEvent,curTick());
            }

        }
    }
    
    //--------------------------wxr

    queue.pop_front();

    if (!queue.empty()) {
        assert(queue.front()->readyTime >= curTick());
        assert(!resp_event.scheduled());
        schedule(resp_event, queue.front()->readyTime);
    } else {
        // if there is nothing left in any queue, signal a drain
        if (drainState() == DrainState::Draining &&
            !totalWriteQueueSize && !totalReadQueueSize &&
            allIntfDrained()) {

            DPRINTF(Drain, "Controller done draining\n");
            signalDrainDone();
        } else {
            // check the refresh state and kick the refresh event loop
            // into action again if banks already closed and just waiting
            // for read to complete
            // DRAM only
            mem_intr->checkRefreshState(mem_pkt->rank);
        }
    }

    delete mem_pkt;

    // We have made a location in the queue available at this point,
    // so if there is a read that was forced to wait, retry now
    if (retry_rd_req) {
        retry_rd_req = false;
        //if the encevent is issued, give the retry to EncEvent
        if (stateEnc!=StatesEnc::DATATRAN)
        {
            port.sendRetryReq();
        }
    }
    if (stateEnc != StatesEnc::IDLE && stateEnc != StatesEnc::INIT && stateEnc != StatesEnc::POLLING && stateEnc != StatesEnc::READY) {
        if (!EncEvent.scheduled())
        {
            schedule(EncEvent,curTick());
        }
    }
}

MemPacketQueue::iterator
MemCtrl::chooseNext(MemPacketQueue& queue, Tick extra_col_delay,
                                                MemInterface* mem_intr)
{
    // This method does the arbitration between requests.

    MemPacketQueue::iterator ret = queue.end();

    if (!queue.empty()) {
        if (queue.size() == 1) {
            // available rank corresponds to state refresh idle
            MemPacket* mem_pkt = *(queue.begin());
            if (mem_pkt->pseudoChannel != mem_intr->pseudoChannel) {
                return ret;
            }
            if (packetReady(mem_pkt, mem_intr)) {
                ret = queue.begin();
                DPRINTF(MemCtrl, "Single request, going to a free rank\n");
            } else {
                DPRINTF(MemCtrl, "Single request, going to a busy rank\n");
            }
        } else if (memSchedPolicy == enums::fcfs) {
            // check if there is a packet going to a free rank
            for (auto i = queue.begin(); i != queue.end(); ++i) {
                MemPacket* mem_pkt = *i;
                if (packetReady(mem_pkt, mem_intr)) {
                    ret = i;
                    break;
                }
            }
        } else if (memSchedPolicy == enums::frfcfs) {
            Tick col_allowed_at;
            std::tie(ret, col_allowed_at)
                    = chooseNextFRFCFS(queue, extra_col_delay, mem_intr);
        } else {
            panic("No scheduling policy chosen\n");
        }
    }
    return ret;
}

std::pair<MemPacketQueue::iterator, Tick>
MemCtrl::chooseNextFRFCFS(MemPacketQueue& queue, Tick extra_col_delay,
                                MemInterface* mem_intr)
{
    auto selected_pkt_it = queue.end();
    Tick col_allowed_at = MaxTick;

    // time we need to issue a column command to be seamless
    const Tick min_col_at = std::max(mem_intr->nextBurstAt + extra_col_delay,
                                    curTick());

    std::tie(selected_pkt_it, col_allowed_at) =
                 mem_intr->chooseNextFRFCFS(queue, min_col_at);

    if (selected_pkt_it == queue.end()) {
        DPRINTF(MemCtrl, "%s no available packets found\n", __func__);
    }

    return std::make_pair(selected_pkt_it, col_allowed_at);
}

void
MemCtrl::accessAndRespond(PacketPtr pkt, Tick static_latency,
                                                MemInterface* mem_intr)
{
    DPRINTF(MemCtrl, "Responding to Address %#x.. \n", pkt->getAddr());

    bool needsResponse = pkt->needsResponse();
    // do the actual memory access which also turns the packet into a
    // response
    //wxr++++++++++++++++++++++++
    // if(( (pkt->getAddrRange().start() >= ENCLAVE_STARTADDR) && (pkt->getAddrRange().end() <= (ENCLAVE_STARTADDR+ENCLAVE_SIZE)) ))
    // if(pkt->EncFlag != 0)
    // {}
    // else{
    panic_if(!mem_intr->getAddrRange().contains(pkt->getAddr()),
             "Can't handle address range for packet %s\n", pkt->print());
    // }
    //wxr------------------------
    mem_intr->access(pkt);

    // turn packet around to go back to requestor if response expected
    if (needsResponse) {
        // access already turned the packet into a response
        assert(pkt->isResponse());
        //++++++++++++++++++++++++++wxr
        if (pkt->EncFlag == 1)
        {
            pkt->real_resp = true;
            return;
        }
        
        //--------------------------wxr
        // response_time consumes the static latency and is charged also
        // with headerDelay that takes into account the delay provided by
        // the xbar and also the payloadDelay that takes into account the
        // number of data beats.
        Tick response_time = curTick() + static_latency + pkt->headerDelay +
                             pkt->payloadDelay;
        // Here we reset the timing of the packet before sending it out.
        pkt->headerDelay = pkt->payloadDelay = 0;

        // queue the packet in the response queue to be sent out after
        // the static latency has passed
        port.schedTimingResp(pkt, response_time);
    } else {
        // @todo the packet is going to be deleted, and the MemPacket
        // is still having a pointer to it
        pendingDelete.reset(pkt);
    }

    DPRINTF(MemCtrl, "Done\n");

    return;
}

void
MemCtrl::pruneBurstTick()
{
    auto it = burstTicks.begin();
    while (it != burstTicks.end()) {
        auto current_it = it++;
        if (curTick() > *current_it) {
            DPRINTF(MemCtrl, "Removing burstTick for %d\n", *current_it);
            burstTicks.erase(current_it);
        }
    }
}

Tick
MemCtrl::getBurstWindow(Tick cmd_tick)
{
    // get tick aligned to burst window
    Tick burst_offset = cmd_tick % commandWindow;
    return (cmd_tick - burst_offset);
}

Tick
MemCtrl::verifySingleCmd(Tick cmd_tick, Tick max_cmds_per_burst, bool row_cmd)
{
    // start with assumption that there is no contention on command bus
    Tick cmd_at = cmd_tick;

    // get tick aligned to burst window
    Tick burst_tick = getBurstWindow(cmd_tick);

    // verify that we have command bandwidth to issue the command
    // if not, iterate over next window(s) until slot found
    while (burstTicks.count(burst_tick) >= max_cmds_per_burst) {
        DPRINTF(MemCtrl, "Contention found on command bus at %d\n",
                burst_tick);
        burst_tick += commandWindow;
        cmd_at = burst_tick;
    }

    // add command into burst window and return corresponding Tick
    burstTicks.insert(burst_tick);
    return cmd_at;
}

Tick
MemCtrl::verifyMultiCmd(Tick cmd_tick, Tick max_cmds_per_burst,
                         Tick max_multi_cmd_split)
{
    // start with assumption that there is no contention on command bus
    Tick cmd_at = cmd_tick;

    // get tick aligned to burst window
    Tick burst_tick = getBurstWindow(cmd_tick);

    // Command timing requirements are from 2nd command
    // Start with assumption that 2nd command will issue at cmd_at and
    // find prior slot for 1st command to issue
    // Given a maximum latency of max_multi_cmd_split between the commands,
    // find the burst at the maximum latency prior to cmd_at
    Tick burst_offset = 0;
    Tick first_cmd_offset = cmd_tick % commandWindow;
    while (max_multi_cmd_split > (first_cmd_offset + burst_offset)) {
        burst_offset += commandWindow;
    }
    // get the earliest burst aligned address for first command
    // ensure that the time does not go negative
    Tick first_cmd_tick = burst_tick - std::min(burst_offset, burst_tick);

    // Can required commands issue?
    bool first_can_issue = false;
    bool second_can_issue = false;
    // verify that we have command bandwidth to issue the command(s)
    while (!first_can_issue || !second_can_issue) {
        bool same_burst = (burst_tick == first_cmd_tick);
        auto first_cmd_count = burstTicks.count(first_cmd_tick);
        auto second_cmd_count = same_burst ? first_cmd_count + 1 :
                                   burstTicks.count(burst_tick);

        first_can_issue = first_cmd_count < max_cmds_per_burst;
        second_can_issue = second_cmd_count < max_cmds_per_burst;

        if (!second_can_issue) {
            DPRINTF(MemCtrl, "Contention (cmd2) found on command bus at %d\n",
                    burst_tick);
            burst_tick += commandWindow;
            cmd_at = burst_tick;
        }

        // Verify max_multi_cmd_split isn't violated when command 2 is shifted
        // If commands initially were issued in same burst, they are
        // now in consecutive bursts and can still issue B2B
        bool gap_violated = !same_burst &&
             ((burst_tick - first_cmd_tick) > max_multi_cmd_split);

        if (!first_can_issue || (!second_can_issue && gap_violated)) {
            DPRINTF(MemCtrl, "Contention (cmd1) found on command bus at %d\n",
                    first_cmd_tick);
            first_cmd_tick += commandWindow;
        }
    }

    // Add command to burstTicks
    burstTicks.insert(burst_tick);
    burstTicks.insert(first_cmd_tick);

    return cmd_at;
}

bool
MemCtrl::inReadBusState(bool next_state) const
{
    // check the bus state
    if (next_state) {
        // use busStateNext to get the state that will be used
        // for the next burst
        return (busStateNext == MemCtrl::READ);
    } else {
        return (busState == MemCtrl::READ);
    }
}

bool
MemCtrl::inWriteBusState(bool next_state) const
{
    // check the bus state
    if (next_state) {
        // use busStateNext to get the state that will be used
        // for the next burst
        return (busStateNext == MemCtrl::WRITE);
    } else {
        return (busState == MemCtrl::WRITE);
    }
}

Tick
MemCtrl::doBurstAccess(MemPacket* mem_pkt, MemInterface* mem_intr)
{
    // first clean up the burstTick set, removing old entries
    // before adding new entries for next burst
    pruneBurstTick();

    // When was command issued?
    Tick cmd_at;

    // Issue the next burst and update bus state to reflect
    // when previous command was issued
    std::vector<MemPacketQueue>& queue = selQueue(mem_pkt->isRead());
    std::tie(cmd_at, mem_intr->nextBurstAt) =
            mem_intr->doBurstAccess(mem_pkt, mem_intr->nextBurstAt, queue);

    DPRINTF(MemCtrl, "Access to %#x, ready at %lld next burst at %lld.\n",
            mem_pkt->addr, mem_pkt->readyTime, mem_intr->nextBurstAt);

    // Update the minimum timing between the requests, this is a
    // conservative estimate of when we have to schedule the next
    // request to not introduce any unecessary bubbles. In most cases
    // we will wake up sooner than we have to.
    mem_intr->nextReqTime = mem_intr->nextBurstAt - mem_intr->commandOffset();

    // Update the common bus stats
    if (mem_pkt->isRead()) {
        ++readsThisTime;
        // Update latency stats
        stats.requestorReadTotalLat[mem_pkt->requestorId()] +=
            mem_pkt->readyTime - mem_pkt->entryTime;
        stats.requestorReadBytes[mem_pkt->requestorId()] += mem_pkt->size;
    } else {
        ++writesThisTime;
        stats.requestorWriteBytes[mem_pkt->requestorId()] += mem_pkt->size;
        stats.requestorWriteTotalLat[mem_pkt->requestorId()] +=
            mem_pkt->readyTime - mem_pkt->entryTime;
    }

    return cmd_at;
}

bool
MemCtrl::memBusy(MemInterface* mem_intr) {

    // check ranks for refresh/wakeup - uses busStateNext, so done after
    // turnaround decisions
    // Default to busy status and update based on interface specifics
    // Default state of unused interface is 'true'
    bool mem_busy = true;
    bool all_writes_nvm = mem_intr->numWritesQueued == totalWriteQueueSize;
    bool read_queue_empty = totalReadQueueSize == 0;
    mem_busy = mem_intr->isBusy(read_queue_empty, all_writes_nvm);
    if (mem_busy) {
        // if all ranks are refreshing wait for them to finish
        // and stall this state machine without taking any further
        // action, and do not schedule a new nextReqEvent
        return true;
    } else {
        return false;
    }
}

bool
MemCtrl::nvmWriteBlock(MemInterface* mem_intr) {

    bool all_writes_nvm = mem_intr->numWritesQueued == totalWriteQueueSize;
    return (mem_intr->writeRespQueueFull() && all_writes_nvm);
}

void
MemCtrl::nonDetermReads(MemInterface* mem_intr) {

    for (auto queue = readQueue.rbegin();
            queue != readQueue.rend(); ++queue) {
            // select non-deterministic NVM read to issue
            // assume that we have the command bandwidth to issue this along
            // with additional RD/WR burst with needed bank operations
            if (mem_intr->readsWaitingToIssue()) {
                // select non-deterministic NVM read to issue
                mem_intr->chooseRead(*queue);
            }
    }
}

void
MemCtrl::processNextReqEvent(MemInterface* mem_intr,
                        MemPacketQueue& resp_queue,
                        EventFunctionWrapper& resp_event,
                        EventFunctionWrapper& next_req_event,
                        bool& retry_wr_req) {
    // transition is handled by QoS algorithm if enabled
    // if (turnPolicy) {
    //     // select bus state - only done if QoS algorithms are in use
    //     busStateNext = selectNextBusState();
    // }




    if (EncRetry)
    {
        EncRetry = false;
        #if STATE_DEBUG 
        std::cout<<"processNextReqEvent <<<<==== RETRY ====>>>>"<<std::endl;
        #endif
        port.sendRetryReq();
    }
    

    if (stateEnc == StatesEnc::INIT)
    {
        // std::cout<<"processNextReqEvent <<<<==== Pool Size ====>>>> "<<EncPktPool.size()<<std::endl;
        //************************************comment by xrwang (2023-5-12)
        //Based on paritially learning, the busStateNext can be changed in many situations.
        //For WRITE, buffer emptying, getting threhold both can lead to state changes.
        //What the user needs to do is to keep blocking by setting retryRd/Wr until completing the transformation.
        busStateNext = MemCtrl::WRITE;
    //    if (!write_found || (EncPktPool.size() == 0))
    //    {
    //         tailHandle = true;
    //         std::cout<<"processNextReqEvent <<<<==== write found / memory busy ====>>>> "<<write_found<<" "<<memBusy(mem_intr)<<std::endl;
    //    }
    }
    if (stateEnc == StatesEnc::RDCNT)
    {
        if (RDCNTrecvCNT < RDCNTwRpkt.size())
        {
            busStateNext = MemCtrl::WRITE;
        }
        else if(RDCNTrecvCNT < (RDCNTrDpkt.size()+RDCNTwRpkt.size()))
        {
            busStateNext = MemCtrl::READ;
        }

        // if (busStateNext == MemCtrl::WRITE && write_found)
        // {
        //     RDCNTrecvCNT ++;
        // }
        
    }

    if (stateEnc == StatesEnc::WBLIMB)
    {
        busStateNext = MemCtrl::WRITE;
    }

    if (stateEnc == StatesEnc::DORESETTREE)
    {
        if (stateRSTEnc == RstState::TREEWB || stateRSTEnc == RstState::MACDATA || stateRSTEnc == RstState::DATA_W)
        {
            busStateNext = MemCtrl::WRITE;
        }
        else if (stateRSTEnc == RstState::TREErdMem || stateRSTEnc == RstState::TREErdEncCache || stateRSTEnc == RstState::WaitResp || stateRSTEnc == RstState::DATA_W_R || stateRSTEnc == RstState::DATA_R)
        {
            busStateNext = MemCtrl::READ;
        }
    }

    if (stateEnc == StatesEnc::PREDICTION_PREFETCH_WAIT || stateEnc == StatesEnc::PREDICTION_PREFETCH_DO || stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH || stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH_DO)
    {
        busStateNext = MemCtrl::READ;
    }

    
    bool tailHandle = false;
    bool EncWrite_found = false;
    bool EncRead_found = false;

    bool switched_Enc = (busState != busStateNext);
    MemPacketQueue::iterator to_write;

    for (auto queue = writeQueue.rbegin();
            queue != writeQueue.rend(); ++queue) {
        to_write = chooseNext((*queue),switched_Enc ? minReadToWriteDataGap() : 0, mem_intr);
        if (to_write != queue->end()) {
            EncWrite_found = true;
            break;
        }
    }

    MemPacketQueue::iterator to_read;
    for (auto queue = readQueue.rbegin();
                 queue != readQueue.rend(); ++queue) {

                to_read = chooseNext((*queue),switched_Enc ? minWriteToReadDataGap() : 0, mem_intr);
                if (to_read != queue->end()) {
                    // candidate read found
                    EncRead_found = true;
                    break;
                }
            }

    if (stateEnc == StatesEnc::INIT)
    {
       if (!EncWrite_found || (EncPktPool.size() == 0))
       {
            tailHandle = true;
       }
    }
    else if (stateEnc == StatesEnc::RDCNT)
    {
        if (busStateNext == MemCtrl::WRITE && !EncWrite_found )
        {
            tailHandle = true;
        }
        if (busStateNext == MemCtrl::READ && !EncRead_found)
        {
            tailHandle = true;
        }
    }
    else if (stateEnc == StatesEnc::WBLIMB)
    {
       if (!EncWrite_found)
       {
            tailHandle = true;
       }
    }
    else if (stateEnc == StatesEnc::DORESETTREE)
    {
        if (busStateNext == MemCtrl::WRITE && !EncWrite_found )
        {
            tailHandle = true;
        }
        if (busStateNext == MemCtrl::READ && !EncRead_found)
        {
            tailHandle = true;
        }
    }
    else if (stateEnc == StatesEnc::PREDICTION_PREFETCH_WAIT || stateEnc == StatesEnc::PREDICTION_PREFETCH_DO || stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH|| stateEnc == StatesEnc::PREDICTION_GAP_PREFETCH_DO)
    {
        if (busStateNext == MemCtrl::READ && !EncRead_found)
        {
            tailHandle = true;
        }
    }
    
    
    
    //conflic detection
    bool conflict_enc;
    if (stateEnc!=StatesEnc::IDLE && stateEnc!=StatesEnc::POLLING)
    {
        conflict_enc = (busStateNext == MemCtrl::WRITE && totalWriteQueueSize==0) || (busStateNext==MemCtrl::READ && totalReadQueueSize==0);
    }
    

    bool noTransFind = !EncRead_found && !EncWrite_found;

    //******************************************************comment by xrwang
    //Noticeable it is for the logic, empty queue in the EncPktPool means
    //all the generated packets for enclave have reponsed so that we can
    //send or transform the authority back to gem5 itself.
    //********************************************
    //Interesting gem5 is, the finding packts needed to send in queue by using chooseNext()
    //is not supposed to used to determine if the generated packets completed
    //due to occupytions of dram ranks also cause writes are not found.
    //So during the generated packets transfromed, there may also exist the write not found,
    //user needs to retry this port manually.
    if (tailHandle)
    {
        #if STATE_DEBUG 
        std::cout<<"processNextReqEvent <<<<==== tail handle ====>>>> "<<"retryRead retryWrite "<<retryRdReq<<" "<<retryWrReq<<" write q "<<totalWriteQueueSize<<" read q "<<totalReadQueueSize<<" memBusy "<<memBusy(mem_intr)<<std::endl;
        #endif
        if((stateEnc == StatesEnc::INIT)){
            if (pre_issue)
            {
                #if STATE_DEBUG 
                std::cout<<"processNextReqEvent <<<<==== StateEnc INIT to POLLING====>>> "<<std::endl;
                #endif
                stateEnc = StatesEnc::POLLING;
                pre_issue = false;
                if (retryRdReq)
                {
                    busStateNext = MemCtrl::READ;
                }
                if (retryWrReq)
                {
                    busStateNext = MemCtrl::WRITE;
                }
            }
            if (memBusy(mem_intr) || !EncWrite_found)
            {
                return;
            }
            if (retryRdReq || retryWrReq)
            {
                retryRdReq = false;
                retryWrReq = false;
                port.sendRetryReq();
            }
            return;
        }
        
        if (conflict_enc && busStateNext==MemCtrl::READ && totalWriteQueueSize!=0)
        {
            busStateNext = MemCtrl::WRITE;
            #if STATE_DEBUG 
            std::cout<<"    READ/WRITE contention. READ to WRITE"<<std::endl;
            #endif
        }
        else if(conflict_enc && busStateNext==MemCtrl::WRITE && totalReadQueueSize!=0)
        {
            busStateNext = MemCtrl::READ;
            #if STATE_DEBUG 
            std::cout<<"    READ/WRITE contention. WRITE to READ"<<std::endl;
            #endif
        }
        else if(conflict_enc && ((busStateNext==MemCtrl::WRITE && totalReadQueueSize==0) || (busStateNext==MemCtrl::READ && totalWriteQueueSize==0)))
        {
            #if STATE_DEBUG 
            std::cout<<"    READ/WRITE contention. WRITE AND READ NO READY"<<std::endl;
            #endif
        }
        else if(!EncRead_found && EncWrite_found)
        {
            busStateNext = MemCtrl::WRITE;
            #if STATE_DEBUG 
            std::cout<<"    READ not found. WRITE found. Turn to WRITE"<<std::endl;
            #endif
        }
        else if(EncRead_found && !EncWrite_found)
        {
            busStateNext = MemCtrl::READ;
            #if STATE_DEBUG 
            std::cout<<"    WRITE not found. READ found. Turn to READ"<<std::endl;
            #endif
        }
    }

    if (stateEnc != StatesEnc::IDLE && stateEnc != StatesEnc::INIT && stateEnc != StatesEnc::POLLING && stateEnc != StatesEnc::DATATRAN && stateEnc != StatesEnc::READY && stateEnc != StatesEnc::PREDICTION_DONE) {
        if (!EncEvent.scheduled())
        {
            //when memory is busy, we issue the request in next cycle( so we add the cycle-Ticks)
            //when no transfer data found, we also delay issue
            if (!memBusy(mem_intr) && !noTransFind)
            {
                schedule(EncEvent,curTick());
            }
            else
            {
                schedule(EncEvent,curTick()+CYCLETICK);
                return;
            }
        }
    }
    
    // if (stateEnc != StatesEnc::IDLE && stateEnc != StatesEnc::INIT && stateEnc != StatesEnc::POLLING&& stateEnc != StatesEnc::READY && stateEnc != StatesEnc::DATATRAN) {
    //     #if STATE_DEBUG std::cout<<"    Normal. "<<curCycle()<<" Next req time. "<<(mem_intr->nextReqTime)/500<<" write q "<<totalWriteQueueSize<<" read q "<<totalReadQueueSize<<std::endl;
    // }
    // std::cout<<"   ====  Exe Normal. "<<curCycle()<<" Next req time. "<<(mem_intr->nextReqTime)/500<<" write q "<<totalWriteQueueSize<<" read q "<<totalReadQueueSize<<std::endl;
    
    // detect bus state change
    bool switched_cmd_type = (busState != busStateNext);
    // record stats
    recordTurnaroundStats();

    DPRINTF(MemCtrl, "QoS Turnarounds selected state %s %s\n",
            (busState==MemCtrl::READ)?"READ":"WRITE",
            switched_cmd_type?"[turnaround triggered]":"");

    if (switched_cmd_type) {
        if (busState == MemCtrl::READ) {
            DPRINTF(MemCtrl,
                    "Switching to writes after %d reads with %d reads "
                    "waiting\n", readsThisTime, totalReadQueueSize);
            stats.rdPerTurnAround.sample(readsThisTime);
            readsThisTime = 0;
        } else {
            DPRINTF(MemCtrl,
                    "Switching to reads after %d writes with %d writes "
                    "waiting\n", writesThisTime, totalWriteQueueSize);
            stats.wrPerTurnAround.sample(writesThisTime);
            writesThisTime = 0;
        }
    }

    // updates current state
    busState = busStateNext;

    nonDetermReads(mem_intr);

    if (memBusy(mem_intr)) {
        return;
    }

    // when we get here it is either a read or a write
    if (busState == READ) {

        // track if we should switch or not
        bool switch_to_writes = false;

        if (totalReadQueueSize == 0) {
            // In the case there is no read request to go next,
            // trigger writes if we have passed the low threshold (or
            // if we are draining)
            if (!(totalWriteQueueSize == 0) &&
                (drainState() == DrainState::Draining ||
                 totalWriteQueueSize > writeLowThreshold)) {

                DPRINTF(MemCtrl,
                        "Switching to writes due to read queue empty\n");
                switch_to_writes = true;
            } else {
                // check if we are drained
                // not done draining until in PWR_IDLE state
                // ensuring all banks are closed and
                // have exited low power states
                if (drainState() == DrainState::Draining &&
                    respQEmpty() && allIntfDrained()) {

                    DPRINTF(Drain, "MemCtrl controller done draining\n");
                    signalDrainDone();
                }

                // nothing to do, not even any point in scheduling an
                // event for the next request
                return;
            }
        } else {

            bool read_found = false;
            MemPacketQueue::iterator to_read;
            uint8_t prio = numPriorities();

            for (auto queue = readQueue.rbegin();
                 queue != readQueue.rend(); ++queue) {

                prio--;

                DPRINTF(QOS,
                        "Checking READ queue [%d] priority [%d elements]\n",
                        prio, queue->size());

                // Figure out which read request goes next
                // If we are changing command type, incorporate the minimum
                // bus turnaround delay which will be rank to rank delay
                to_read = chooseNext((*queue), switched_cmd_type ?
                                     minWriteToReadDataGap() : 0, mem_intr);

                if (to_read != queue->end()) {
                    // candidate read found
                    read_found = true;
                    break;
                }
            }

            // if no read to an available rank is found then return
            // at this point. There could be writes to the available ranks
            // which are above the required threshold. However, to
            // avoid adding more complexity to the code, return and wait
            // for a refresh event to kick things into action again.
            if (!read_found) {
                DPRINTF(MemCtrl, "No Reads Found - exiting\n");
                return;
            }

            auto mem_pkt = *to_read;

            Tick cmd_at = doBurstAccess(mem_pkt, mem_intr);

            DPRINTF(MemCtrl,
            "Command for %#x, issued at %lld.\n", mem_pkt->addr, cmd_at);

            // sanity check
            assert(pktSizeCheck(mem_pkt, mem_intr));
            assert(mem_pkt->readyTime >= curTick());

            // log the response
            logResponse(MemCtrl::READ, (*to_read)->requestorId(),
                        mem_pkt->qosValue(), mem_pkt->getAddr(), 1,
                        mem_pkt->readyTime - mem_pkt->entryTime);


            // Insert into response queue. It will be sent back to the
            // requestor at its readyTime
            if (resp_queue.empty()) {
                assert(!resp_event.scheduled());
                schedule(resp_event, mem_pkt->readyTime);
            } else {
                assert(resp_queue.back()->readyTime <= mem_pkt->readyTime);
                assert(resp_event.scheduled());
            }

            resp_queue.push_back(mem_pkt);

            // we have so many writes that we have to transition
            // don't transition if the writeRespQueue is full and
            // there are no other writes that can issue
            // Also ensure that we've issued a minimum defined number
            // of reads before switching, or have emptied the readQ
            if ((totalWriteQueueSize > writeHighThreshold) &&
               (readsThisTime >= minReadsPerSwitch || totalReadQueueSize == 0)
               && !(nvmWriteBlock(mem_intr))) {
                switch_to_writes = true;
            }

            // remove the request from the queue
            // the iterator is no longer valid .
            readQueue[mem_pkt->qosValue()].erase(to_read);
        }

        // switching to writes, either because the read queue is empty
        // and the writes have passed the low threshold (or we are
        // draining), or because the writes hit the hight threshold
        if (switch_to_writes) {
            // transition to writing
            busStateNext = WRITE;
        }
    } else {

        bool write_found = false;
        MemPacketQueue::iterator to_write;
        uint8_t prio = numPriorities();

        for (auto queue = writeQueue.rbegin();
             queue != writeQueue.rend(); ++queue) {

            prio--;

            DPRINTF(QOS,
                    "Checking WRITE queue [%d] priority [%d elements]\n",
                    prio, queue->size());

            // If we are changing command type, incorporate the minimum
            // bus turnaround delay
            to_write = chooseNext((*queue),
                    switched_cmd_type ? minReadToWriteDataGap() : 0, mem_intr);

            if (to_write != queue->end()) {
                write_found = true;
                break;
            }
        }

        // if there are no writes to a rank that is available to service
        // requests (i.e. rank is in refresh idle state) are found then
        // return. There could be reads to the available ranks. However, to
        // avoid adding more complexity to the code, return at this point and
        // wait for a refresh event to kick things into action again.
        if (!write_found) {
            DPRINTF(MemCtrl, "No Writes Found - exiting\n");
            return;
        }

        auto mem_pkt = *to_write;

        // sanity check
        assert(pktSizeCheck(mem_pkt, mem_intr));

        Tick cmd_at = doBurstAccess(mem_pkt, mem_intr);
        DPRINTF(MemCtrl,
        "Command for %#x, issued at %lld.\n", mem_pkt->addr, cmd_at);

        isInWriteQueue.erase(burstAlign(mem_pkt->addr, mem_intr));

        // log the response
        logResponse(MemCtrl::WRITE, mem_pkt->requestorId(),
                    mem_pkt->qosValue(), mem_pkt->getAddr(), 1,
                    mem_pkt->readyTime - mem_pkt->entryTime);


        // remove the request from the queue - the iterator is no longer valid
        writeQueue[mem_pkt->qosValue()].erase(to_write);

        delete mem_pkt;

        // If we emptied the write queue, or got sufficiently below the
        // threshold (using the minWritesPerSwitch as the hysteresis) and
        // are not draining, or we have reads waiting and have done enough
        // writes, then switch to reads.
        // If we are interfacing to NVM and have filled the writeRespQueue,
        // with only NVM writes in Q, then switch to reads
        bool below_threshold =
            totalWriteQueueSize + minWritesPerSwitch < writeLowThreshold;

        if (totalWriteQueueSize == 0 ||
            (below_threshold && drainState() != DrainState::Draining) ||
            (totalReadQueueSize && writesThisTime >= minWritesPerSwitch) ||
            (totalReadQueueSize && (nvmWriteBlock(mem_intr)))) {

            // turn the bus back around for reads again
            busStateNext = MemCtrl::READ;

            // note that the we switch back to reads also in the idle
            // case, which eventually will check for any draining and
            // also pause any further scheduling if there is really
            // nothing to do
        }
    }
    // It is possible that a refresh to another rank kicks things back into
    // action before reaching this point.
    if (!next_req_event.scheduled())
        schedule(next_req_event, std::max(mem_intr->nextReqTime, curTick()));

    if (retry_wr_req && totalWriteQueueSize < writeBufferSize) {
        retry_wr_req = false;
        port.sendRetryReq();
    }
    //+++++++++++++++++++++++++++++++++++++++wxr
    // if (retryRdReq && totalReadQueueSize < readBufferSize && stateEnc != StatesEnc::IDLE && stateEnc != StatesEnc::INIT && stateEnc != StatesEnc::POLLING && stateEnc != StatesEnc::DATATRAN) {
    //     //************************** we turn back the state to avoid the conflict when our own states are different from origin pkt
    //     busStateNext = WriteIf ? MemCtrl::WRITE : MemCtrl::READ;
    //     //**************************
    //     retryRdReq = false;
    //     port.sendRetryReq();
    // }
    //---------------------------------------wxr
}

bool
MemCtrl::packetReady(MemPacket* pkt, MemInterface* mem_intr)
{
    return mem_intr->burstReady(pkt);
}

Tick
MemCtrl::minReadToWriteDataGap()
{
    return dram->minReadToWriteDataGap();
}

Tick
MemCtrl::minWriteToReadDataGap()
{
    return dram->minWriteToReadDataGap();
}

Addr
MemCtrl::burstAlign(Addr addr, MemInterface* mem_intr) const
{
    return (addr & ~(Addr(mem_intr->bytesPerBurst() - 1)));
}

bool
MemCtrl::pktSizeCheck(MemPacket* mem_pkt, MemInterface* mem_intr) const
{
    return (mem_pkt->size <= mem_intr->bytesPerBurst());
}

MemCtrl::CtrlStats::CtrlStats(MemCtrl &_ctrl)
    : statistics::Group(&_ctrl),
    ctrl(_ctrl),

    ADD_STAT(readReqs, statistics::units::Count::get(),
             "Number of read requests accepted"),
    ADD_STAT(writeReqs, statistics::units::Count::get(),
             "Number of write requests accepted"),

    ADD_STAT(readBursts, statistics::units::Count::get(),
             "Number of controller read bursts, including those serviced by "
             "the write queue"),
    ADD_STAT(writeBursts, statistics::units::Count::get(),
             "Number of controller write bursts, including those merged in "
             "the write queue"),
    ADD_STAT(servicedByWrQ, statistics::units::Count::get(),
             "Number of controller read bursts serviced by the write queue"),
    ADD_STAT(mergedWrBursts, statistics::units::Count::get(),
             "Number of controller write bursts merged with an existing one"),

    ADD_STAT(neitherReadNorWriteReqs, statistics::units::Count::get(),
             "Number of requests that are neither read nor write"),

    ADD_STAT(avgRdQLen, statistics::units::Rate<
                statistics::units::Count, statistics::units::Tick>::get(),
             "Average read queue length when enqueuing"),
    ADD_STAT(avgWrQLen, statistics::units::Rate<
                statistics::units::Count, statistics::units::Tick>::get(),
             "Average write queue length when enqueuing"),

    ADD_STAT(numRdRetry, statistics::units::Count::get(),
             "Number of times read queue was full causing retry"),
    ADD_STAT(numWrRetry, statistics::units::Count::get(),
             "Number of times write queue was full causing retry"),

    ADD_STAT(readPktSize, statistics::units::Count::get(),
             "Read request sizes (log2)"),
    ADD_STAT(writePktSize, statistics::units::Count::get(),
             "Write request sizes (log2)"),

    ADD_STAT(rdQLenPdf, statistics::units::Count::get(),
             "What read queue length does an incoming req see"),
    ADD_STAT(wrQLenPdf, statistics::units::Count::get(),
             "What write queue length does an incoming req see"),

    ADD_STAT(rdPerTurnAround, statistics::units::Count::get(),
             "Reads before turning the bus around for writes"),
    ADD_STAT(wrPerTurnAround, statistics::units::Count::get(),
             "Writes before turning the bus around for reads"),

    ADD_STAT(bytesReadWrQ, statistics::units::Byte::get(),
             "Total number of bytes read from write queue"),
    ADD_STAT(bytesReadSys, statistics::units::Byte::get(),
             "Total read bytes from the system interface side"),
    ADD_STAT(bytesWrittenSys, statistics::units::Byte::get(),
             "Total written bytes from the system interface side"),

    ADD_STAT(avgRdBWSys, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Average system read bandwidth in Byte/s"),
    ADD_STAT(avgWrBWSys, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Average system write bandwidth in Byte/s"),

    ADD_STAT(totGap, statistics::units::Tick::get(),
             "Total gap between requests"),
    ADD_STAT(avgGap, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "Average gap between requests"),

    ADD_STAT(requestorReadBytes, statistics::units::Byte::get(),
             "Per-requestor bytes read from memory"),
    ADD_STAT(requestorWriteBytes, statistics::units::Byte::get(),
             "Per-requestor bytes write to memory"),
    ADD_STAT(requestorReadRate, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Per-requestor bytes read from memory rate"),
    ADD_STAT(requestorWriteRate, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Per-requestor bytes write to memory rate"),
    ADD_STAT(requestorReadAccesses, statistics::units::Count::get(),
             "Per-requestor read serviced memory accesses"),
    ADD_STAT(requestorWriteAccesses, statistics::units::Count::get(),
             "Per-requestor write serviced memory accesses"),
    ADD_STAT(requestorReadTotalLat, statistics::units::Tick::get(),
             "Per-requestor read total memory access latency"),
    ADD_STAT(requestorWriteTotalLat, statistics::units::Tick::get(),
             "Per-requestor write total memory access latency"),
    ADD_STAT(requestorReadAvgLat, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "Per-requestor read average memory access latency"),
    ADD_STAT(requestorWriteAvgLat, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "Per-requestor write average memory access latency"),
    //+++++++++++++++++++++++ wxr
    ADD_STAT(EncReadReq, statistics::units::Count::get(),
             "Number of read requests checked"),
    ADD_STAT(EncWriteReq, statistics::units::Count::get(),
             "Number of write requests checked"),
    ADD_STAT(EncAvgTotGap, statistics::units::Rate<statistics::units::Cycle,statistics::units::Count>::get(),
             "Memory access gap"),
    ADD_STAT(EncTotTime, statistics::units::Cycle::get(),
             "Total Time Cycle"),
    ADD_STAT(EncBeginTimeout, statistics::units::Cycle::get(),
             "Cycle"),
    ADD_STAT(EncEndTimeOut, statistics::units::Cycle::get(),
             "Cycle"),
    ADD_STAT(EncTotInst, statistics::units::Count::get(),
             "Total Inst"),
    ADD_STAT(EncEncryptTime, statistics::units::Cycle::get(),
             "Encrypt Time Cycle"),
    ADD_STAT(EncDecryptTime, statistics::units::Cycle::get(),
             "Decrypt Time Cycle"),
    ADD_STAT(EncCacheAccessTime, statistics::units::Cycle::get(),
             "CacheAccesTime Cycle"),
    ADD_STAT(EncReset, statistics::units::Count::get(),
             "Reset Time"),
    ADD_STAT(EncReset_Mem_WrTime, statistics::units::Count::get(),
             "Reset Read"),
    ADD_STAT(EncReset_Mem_RdTime, statistics::units::Count::get(),
             "Reset Write"),
    ADD_STAT(EncReset_Mem_EncryptTime, statistics::units::Cycle::get(),
             "Reset Encryption Cycle"),
    ADD_STAT(EncReset_Mem_DecryptTime, statistics::units::Cycle::get(),
             "Reset Decryption Cycle"),
    ADD_STAT(EncReset_Mem_HashTime, statistics::units::Cycle::get(),
             "Reset Hash Cycle"),
    ADD_STAT(EncResetAesBias, statistics::units::Cycle::get(),
             "Reset AES Bias Cycle"),
    ADD_STAT(EncResetHashBias, statistics::units::Cycle::get(),
             "Reset Hash Bias Cycle"),
    ADD_STAT(EncCacheRd, statistics::units::Count::get(),
             "Cnt Cache Read"),
    ADD_STAT(EncCacheWr, statistics::units::Count::get(),
             "Cnt Cache Write"),
    ADD_STAT(EncCacheRd_hit, statistics::units::Count::get(),
             "Cnt Cache Read Hit"),
    ADD_STAT(EncCacheWr_hit, statistics::units::Count::get(),
             "Cnt Cache WriteHit"),
    ADD_STAT(EncCache_hitRate, statistics::units::Rate<statistics::units::Count,statistics::units::Count>::get(),
             "Cnt Cache WriteHit"),
    ADD_STAT(MacCacheRd, statistics::units::Count::get(),
             "Mac Cache Read"),
    ADD_STAT(MacCacheWr, statistics::units::Count::get(),
             "Mac Cache Write"),
    ADD_STAT(MacCacheRd_hit, statistics::units::Count::get(),
             "Mac Cache Read Hit"),
    ADD_STAT(MacCacheWr_hit, statistics::units::Count::get(),
             "Mac Cache WriteHit"),
    ADD_STAT(MacCache_hitRate, statistics::units::Rate<statistics::units::Count,statistics::units::Count>::get(),
             "Mac Cache WriteHit"),
    ADD_STAT(EncPredictionSuccess, statistics::units::Count::get(),
             "Prediction Success Time"),
    ADD_STAT(EncPredictionSuccessTree, statistics::units::Count::get(),
             "Prediction Success Time with Tree"),
    ADD_STAT(EncPredictionNorAesSuccess, statistics::units::Count::get(),
             "AES normal Success Time"),
    ADD_STAT(EncPrefetchWaitport, statistics::units::Count::get(),
             "Prefetch Wait Port"),
    ADD_STAT(EncReadAesBias, statistics::units::Cycle::get(),
             "Prediction Read overlap with AES"),
    ADD_STAT(EncPrefetchHashOverlap, statistics::units::Cycle::get(),
             "Prediction Prefetch overlap with Hash"),
    ADD_STAT(EncPredictionSaveAes, statistics::units::Cycle::get(),
             "Prediction Save Aes Cycle"),
    ADD_STAT(EncPredictionAesSuccess, statistics::units::Count::get(),
             "Prediction Aes table find"),
    ADD_STAT(EncHashOverlapRDMAC, statistics::units::Cycle::get(),
             "Hash overlap Read MAC"),
    ADD_STAT(EncHashGroupAvg, statistics::units::Cycle::get(),
             "hash average NUM"),
    ADD_STAT(EncHashGroupBusyMax, statistics::units::Cycle::get(),
             "hash Max NUM"),
    ADD_STAT(EncPreWaitCaled, statistics::units::Cycle::get(),
             "wait for now prediciotn caled done"),
    ADD_STAT(EncPrefetchTime, statistics::units::Cycle::get(),
             "total prediction prefetch time"),
    ADD_STAT(EncPrefetchCycle, statistics::units::Cycle::get(),
             "total prediction prefetch Cycle"),
    ADD_STAT(EncReadAllLatency, statistics::units::Cycle::get(),
             "total Read Cycle"),
    ADD_STAT(EncL0Reset, statistics::units::Cycle::get(),
             "L0 Reset Time"),
    ADD_STAT(EncPrefetchGapPreTime, statistics::units::Cycle::get(),
             "prediction during gap"),
    ADD_STAT(EncWritePredictionNoneed, statistics::units::Cycle::get(),
             "prediction write no need"),

    ADD_STAT(EncTreeL0ReadTime, statistics::units::Cycle::get(),
             "Tree L0 Read"),
    ADD_STAT(EncTreeL0MissTime, statistics::units::Cycle::get(),
             "Tree L0 Miss"),
    ADD_STAT(EncTreeL1ReadTime, statistics::units::Cycle::get(),
             "Tree L1 Read"),
    ADD_STAT(EncTreeL1MissTime, statistics::units::Cycle::get(),
             "Tree L1 Miss"),
    ADD_STAT(EncTreeL2ReadTime, statistics::units::Cycle::get(),
             "Tree L2 Read"),
    ADD_STAT(EncTreeL2MissTime, statistics::units::Cycle::get(),
             "Tree L2 Miss"),
    ADD_STAT(EncTreeL3ReadTime, statistics::units::Cycle::get(),
             "Tree L3 Read"),
    ADD_STAT(EncTreeL3MissTime, statistics::units::Cycle::get(),
             "Tree L3 Miss"),
    ADD_STAT(EncTreeL4ReadTime, statistics::units::Cycle::get(),
             "Tree L4 Read"),
    ADD_STAT(EncTreeL4MissTime, statistics::units::Cycle::get(),
             "Tree L4 Miss"),
    ADD_STAT(EncTreeNoCheck, statistics::units::Cycle::get(),
             "Read no need check tree"),


    ADD_STAT(EncWTreeL0ReadTime, statistics::units::Cycle::get(),
             "WTree L0 Read"),
    ADD_STAT(EncWTreeL0MissTime, statistics::units::Cycle::get(),
             "WTree L0 Miss"),
    ADD_STAT(EncWTreeL1ReadTime, statistics::units::Cycle::get(),
             "WTree L1 Read"),
    ADD_STAT(EncWTreeL1MissTime, statistics::units::Cycle::get(),
             "WTree L1 Miss"),
    ADD_STAT(EncWTreeL2ReadTime, statistics::units::Cycle::get(),
             "WTree L2 Read"),
    ADD_STAT(EncWTreeL2MissTime, statistics::units::Cycle::get(),
             "WTree L2 Miss"),
    ADD_STAT(EncWTreeL3ReadTime, statistics::units::Cycle::get(),
             "WTree L3 Read"),
    ADD_STAT(EncWTreeL3MissTime, statistics::units::Cycle::get(),
             "WTree L3 Miss"),
    ADD_STAT(EncWTreeL4ReadTime, statistics::units::Cycle::get(),
             "WTree L4 Read"),
    ADD_STAT(EncWTreeL4MissTime, statistics::units::Cycle::get(),
             "WTree L4 Miss"),
    ADD_STAT(EncWTreeNoCheck, statistics::units::Cycle::get(),
             "Write Read no need check tree"),
    ADD_STAT(EncWTree2HASHTIME, statistics::units::Cycle::get(),
             "Write need hash 2 time"),
    ADD_STAT(EncWriteHashTime, statistics::units::Cycle::get(),
             "Write hash time")
    //----------------------- wxr
{
}

void
MemCtrl::CtrlStats::regStats()
{
    using namespace statistics;

    assert(ctrl.system());
    const auto max_requestors = ctrl.system()->maxRequestors();

    avgRdQLen.precision(2);
    avgWrQLen.precision(2);

    readPktSize.init(ceilLog2(ctrl.system()->cacheLineSize()) + 1);
    writePktSize.init(ceilLog2(ctrl.system()->cacheLineSize()) + 1);

    rdQLenPdf.init(ctrl.readBufferSize);
    wrQLenPdf.init(ctrl.writeBufferSize);

    rdPerTurnAround
        .init(ctrl.readBufferSize)
        .flags(nozero);
    wrPerTurnAround
        .init(ctrl.writeBufferSize)
        .flags(nozero);

    avgRdBWSys.precision(8);
    avgWrBWSys.precision(8);
    avgGap.precision(2);

    // per-requestor bytes read and written to memory
    requestorReadBytes
        .init(max_requestors)
        .flags(nozero | nonan);

    requestorWriteBytes
        .init(max_requestors)
        .flags(nozero | nonan);

    // per-requestor bytes read and written to memory rate
    requestorReadRate
        .flags(nozero | nonan)
        .precision(12);

    requestorReadAccesses
        .init(max_requestors)
        .flags(nozero);

    requestorWriteAccesses
        .init(max_requestors)
        .flags(nozero);

    requestorReadTotalLat
        .init(max_requestors)
        .flags(nozero | nonan);

    requestorReadAvgLat
        .flags(nonan)
        .precision(2);

    requestorWriteRate
        .flags(nozero | nonan)
        .precision(12);

    requestorWriteTotalLat
        .init(max_requestors)
        .flags(nozero | nonan);

    requestorWriteAvgLat
        .flags(nonan)
        .precision(2);

    for (int i = 0; i < max_requestors; i++) {
        const std::string requestor = ctrl.system()->getRequestorName(i);
        requestorReadBytes.subname(i, requestor);
        requestorReadRate.subname(i, requestor);
        requestorWriteBytes.subname(i, requestor);
        requestorWriteRate.subname(i, requestor);
        requestorReadAccesses.subname(i, requestor);
        requestorWriteAccesses.subname(i, requestor);
        requestorReadTotalLat.subname(i, requestor);
        requestorReadAvgLat.subname(i, requestor);
        requestorWriteTotalLat.subname(i, requestor);
        requestorWriteAvgLat.subname(i, requestor);
    }

    // Formula stats
    avgRdBWSys = (bytesReadSys) / simSeconds;
    avgWrBWSys = (bytesWrittenSys) / simSeconds;

    avgGap = totGap / (readReqs + writeReqs);

    requestorReadRate = requestorReadBytes / simSeconds;
    requestorWriteRate = requestorWriteBytes / simSeconds;
    requestorReadAvgLat = requestorReadTotalLat / requestorReadAccesses;
    requestorWriteAvgLat = requestorWriteTotalLat / requestorWriteAccesses;

    EncAvgTotGap.precision(2);
    EncCache_hitRate.precision(3);
    MacCache_hitRate.precision(3);
    EncHashGroupAvg.precision(3);
    EncSuccessRate.precision(3);

    EncAvgTotGap = EncTotGap / (EncReadReq + EncWriteReq);
    EncCache_hitRate = (EncCacheRd_hit+EncCacheWr_hit)/(EncCacheRd+EncCacheWr);
    MacCache_hitRate = (MacCacheRd_hit+MacCacheWr_hit)/(MacCacheRd+MacCacheWr);
    EncHashGroupAvg = EncHashGroupBusyNum/EncHashGroupNum;
    EncTotTime= EncEndCycle-EncBeginCycle + encryptC*(EncEncryptTime+EncDecryptTime) + EncHasUpdata*hashC + EncCacheAccessTime*cacheC + EncResetAesBias + EncResetHashBias;
    EncSuccessRate = (EncPredictionSuccess) / (EncReadReq);
}

void
MemCtrl::recvFunctional(PacketPtr pkt)
{
    bool found = recvFunctionalLogic(pkt, dram);

    panic_if(!found, "Can't handle address range for packet %s\n",
             pkt->print());
}

bool
MemCtrl::recvFunctionalLogic(PacketPtr pkt, MemInterface* mem_intr)
{
    if (mem_intr->getAddrRange().contains(pkt->getAddr())) {
        // rely on the abstract memory
        mem_intr->functionalAccess(pkt);
        return true;
    } else {
        return false;
    }
}

Port &
MemCtrl::getPort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return qos::MemCtrl::getPort(if_name, idx);
    } else {
        return port;
    }
}

bool
MemCtrl::allIntfDrained() const
{
   // DRAM: ensure dram is in power down and refresh IDLE states
   // NVM: No outstanding NVM writes
   // NVM: All other queues verified as needed with calling logic
   return dram->allRanksDrained();
}

DrainState
MemCtrl::drain()
{
    // if there is anything in any of our internal queues, keep track
    // of that as well
    if (!(!totalWriteQueueSize && !totalReadQueueSize && respQueue.empty() &&
          allIntfDrained())) {

        DPRINTF(Drain, "Memory controller not drained, write: %d, read: %d,"
                " resp: %d\n", totalWriteQueueSize, totalReadQueueSize,
                respQueue.size());

        // the only queue that is not drained automatically over time
        // is the write queue, thus kick things into action if needed
        if (!totalWriteQueueSize && !nextReqEvent.scheduled()) {
            schedule(nextReqEvent, curTick());
        }

        dram->drainRanks();

        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

void
MemCtrl::drainResume()
{
    if (!isTimingMode && system()->isTimingMode()) {
        // if we switched to timing mode, kick things into action,
        // and behave as if we restored from a checkpoint
        startup();
        dram->startup();
    } else if (isTimingMode && !system()->isTimingMode()) {
        // if we switch from timing mode, stop the refresh events to
        // not cause issues with KVM
        dram->suspend();
    }

    // update the mode
    isTimingMode = system()->isTimingMode();
}

AddrRangeList
MemCtrl::getAddrRanges()
{
    AddrRangeList range;
    range.push_back(dram->getAddrRange());
    return range;
}

MemCtrl::MemoryPort::
MemoryPort(const std::string& name, MemCtrl& _ctrl)
    : QueuedResponsePort(name, &_ctrl, queue), queue(_ctrl, *this, true),
      ctrl(_ctrl)
{ }

AddrRangeList
MemCtrl::MemoryPort::getAddrRanges() const
{
    return ctrl.getAddrRanges();
}

void
MemCtrl::MemoryPort::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(ctrl.name());

    if (!queue.trySatisfyFunctional(pkt)) {
        // Default implementation of SimpleTimingPort::recvFunctional()
        // calls recvAtomic() and throws away the latency; we can save a
        // little here by just not calculating the latency.
        ctrl.recvFunctional(pkt);
    }

    pkt->popLabel();
}

Tick
MemCtrl::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return ctrl.recvAtomic(pkt);
}

Tick
MemCtrl::MemoryPort::recvAtomicBackdoor(
        PacketPtr pkt, MemBackdoorPtr &backdoor)
{
    return ctrl.recvAtomicBackdoor(pkt, backdoor);
}

bool
MemCtrl::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    // pass it to the memory controller
    return ctrl.recvTimingReq(pkt);
}

void
MemCtrl::MemoryPort::disableSanityCheck()
{
    queue.disableSanityCheck();
}

} // namespace memory
} // namespace gem5
