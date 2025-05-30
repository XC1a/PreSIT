/*
 * @Author: Xinrui Wang 
 * @Date: 2023-04-04 12:27:16 
 * @Last Modified by: Xinrui Wang
 * @Last Modified time: 2023-04-10 14:39:48
 */

//Realization of the vault class

#include "YOUR_PATH/gem5/src/Enclave/vault.hh"
#include <iostream>
#include <vector>
#include <tuple>
#include <cstring>

namespace gem5{

void 
vault::bitset_2_array(const std::bitset<512> &bits, int n_set_size, byte *buf, int  &n_bytes)
{
    n_bytes = 0;
    for (int i = 0; i < n_set_size; i += 8)
    {
        byte ch;
        for (int j = 0; j < 8; ++j)
        {
            if (bits.test(i + j))
                ch |= (1 << j);
            else
                ch &= ~(1 << j);
        }
        buf[n_bytes++] = ch;
    }
}

void 
vault::array_2_bitset(const byte *buf, int  n_bytes, std::bitset<512> &bits, int  &n_set_size)
{
    n_set_size = 0;
    int n_bits = n_bytes * 8;
    for (int i = 0; i < n_bytes; ++i)
    {
        byte ch = buf[i];
        int n_offset = i * 8;
        for (int j = 0; j < 8; ++j)
        {
            bits.set(n_offset + j, ch & (1 << j));
            ++n_set_size;
        }
    }
}


//call when class contruction initialization to get the levels of MT
void vault::getMTLevel(){
    int counter_num = (MEM_SIZE * 1024 * 1024  ) / 64 / BLK_SIZE;
    int counter_byte = counter_num * BLK_SIZE;
    //L1 MEM/64 = counter_byte, L2 L1/32
    int L2 = counter_byte / 32;
    mt_level_entry_num.push_back(counter_num);
    mt_level_entry_num.push_back(L2/BLK_SIZE);
    if(L2 < 64){ //the byte number < 64, the vault initial fault
        std::cerr<<"The vault initial fault. Please check MEM_SIZE!"<<std::endl;
        return;
    }
    int Li(L2);
    int i;
    for (i = 0; Li > 64; i++)
    {
        Li /= 16;
        mt_level_entry_num.push_back(Li/BLK_SIZE);
    }
    if(Li <64){
        std::cerr<<"Please check MEM_SIZE to satisfy VAULT arity!"<<std::endl;
        return;
    }
    MT_LEVEL = i + 2;
    if(VAULT_DEBUG){
        std::cout<<"L1 entry: "<<mt_level_entry_num[0]<<". mt level list size: "<<mt_level_entry_num.size()<<std::endl;
        std::cout<<"L2_entry: "<<mt_level_entry_num[1]<<std::endl;
        std::cout<<"MEM_SIZE: "<<MEM_SIZE<<" MT_LEVEL: "<<MT_LEVEL<<std::endl;
    }
}

//this function split the Counter by arity to single counter and pedding 0
void vault::splitCounter(byte* CounterMixed , byte* splitCounter, byte* globalCounter, int arity){
    std::bitset<512> Counter_bitset;
    std::bitset<512> tmp_bitset;
    std::bitset<512> mask_bitset(0x7f);
    std::bitset<512> mask_comp_bitset(0x3f);
    std::bitset<512> mask_bitset_32(0xfff);
    std::bitset<512> mask_bitset_16(0xffffff);
    int ptr;
    array_2_bitset(CounterMixed,64,Counter_bitset,ptr);
    // std::cout<<"Counter_bitset: "<<Counter_bitset<<std::endl;

    if (64 == arity)
    {
        for (size_t i = 0; i < 64; i++)
        {
            tmp_bitset = Counter_bitset & (VAULT_COMP ? mask_comp_bitset : mask_bitset);
            bitset_2_array(tmp_bitset,8,(splitCounter+i),ptr);
            Counter_bitset >>=7;
        }
        bitset_2_array(Counter_bitset,64,globalCounter,ptr);
        return;
    }
    if (32 == arity)
    {
        //Notice to delete hash first
        Counter_bitset >>= 64;
        for (size_t i = 0; i < 32; i++)
        {
            tmp_bitset = Counter_bitset & mask_bitset_32;
            bitset_2_array(tmp_bitset,16,(splitCounter+i*2),ptr);
            Counter_bitset >>=12;
        }
        bitset_2_array(Counter_bitset,64,globalCounter,ptr);
        return;
    }
    if (16 == arity)
    {
        Counter_bitset >>= 64;
        for (size_t i = 0; i < 16; i++)
        {
            tmp_bitset = Counter_bitset & mask_bitset_16;
            bitset_2_array(tmp_bitset,24,(splitCounter+i*3),ptr);
            Counter_bitset >>=24;
        }
        bitset_2_array(Counter_bitset,64,globalCounter,ptr);
    }
}

void vault::MixedCounter(byte* CounterMixed , byte* splitCounter, byte* globalCounter, int arity){
    std::bitset<512> splitCounter_bitset;
    std::bitset<512> Counter_bitset(0);
    std::bitset<512> globalCounter_bitset(0);
    std::bitset<512> hashPedding(0);
    std::bitset<512> tmp_bitset;
    std::bitset<512> mask_bitset(0x7f);
    std::bitset<512> mask_comp_bitset(0x3f);
    std::bitset<512> mask_bitset_32(0xfff);
    std::bitset<512> mask_bitset_16(0xffffff);
    int ptr;
    array_2_bitset(splitCounter,64,splitCounter_bitset,ptr);
    array_2_bitset(globalCounter,8,globalCounter_bitset,ptr);
    array_2_bitset(CounterMixed,8,hashPedding,ptr);
    if(arity != 64){
        Counter_bitset |= hashPedding;
    }

    if (64 == arity)
    {
        for (size_t i = 0; i < 64; i++)
        {
            tmp_bitset = splitCounter_bitset & (VAULT_COMP ? mask_comp_bitset : mask_bitset);
            splitCounter_bitset >>= 8;
            Counter_bitset |= (tmp_bitset<<(i*(VAULT_COMP ? 6 : 7)));
        }
        bitset_2_array(Counter_bitset,512,CounterMixed,ptr);
    }
    if (32 == arity)
    {
        //Notice to pedding hash first
        for (size_t i = 0; i < 32; i++)
        {
            tmp_bitset = splitCounter_bitset & mask_bitset_32;
            splitCounter_bitset >>= 16;
            Counter_bitset |= (tmp_bitset<<(i*12 + 64));
        }
        Counter_bitset |= globalCounter_bitset;
        bitset_2_array(Counter_bitset,512,CounterMixed,ptr);
    }
    if (16 == arity)
    {
        for (size_t i = 0; i < 16; i++)
        {
            tmp_bitset = splitCounter_bitset & mask_bitset_16;
            splitCounter_bitset >>= 24;
            Counter_bitset |= (tmp_bitset<<(i*24+64));
        }
        Counter_bitset |= globalCounter_bitset;
        bitset_2_array(Counter_bitset,512,CounterMixed,ptr);
    }
    // std::cout<<"debug bitset: "<<Counter_bitset<<std::endl;

}

//get the L1 enctryption key
void vault::getCounterKey(int Counter_entry, byte* key_split){
    byte CounterSplit[2*32]={0};
    byte globalCounter[8];
    splitCounter(Counter[1][Counter_entry/32],CounterSplit,globalCounter,32);//get the L2 counters
    for (size_t j = 0; j < 16; j++)
    {
        if(j==0 || j==1){
            key_split[j] = CounterSplit[Counter_entry%32 + j];
        }
        else if(j>=2 && j<=9){
            key_split[j] = globalCounter[j-2];
        }
        else{
            key_split[j] = 0;
        }
    }
}

//This function only hash a single data block
//Please note the root node has no ancestor counters.
//This function needs to depart this situation.
void vault::hashBlock(int level, int Counter_entry){
    int ptr=0;
    std::bitset<512> Counter_bitset;
    std::bitset<512> Ancestor_bitset(0);
    std::bitset<512> preHash(0);
    byte preHashChar[64];
    std::string hashValueString;
    byte hashValue[8];
    byte globalCounter[8];
    byte ancestor_counter_split[3*16];
    array_2_bitset(Counter[level][Counter_entry],64,Counter_bitset,ptr); //get L2 bitset
    Counter_bitset >>= 64;
    if(level != MT_LEVEL-1){
        splitCounter(Counter[level+1][Counter_entry/16],ancestor_counter_split,globalCounter,ptr); //get L3 counter
        array_2_bitset(ancestor_counter_split+(Counter_entry%16)*3,3,Ancestor_bitset,ptr);
        preHash = Counter_bitset | (Ancestor_bitset << 448);
    }
    else{
        preHash = Counter_bitset;
    }
    bitset_2_array(preHash,512,preHashChar,ptr);
    hashValueString = Sha256::getInstance().getHexMessageDigest(std::string((char *)preHashChar));
    memcpy(hashValue, (byte *)hashValueString.data(),8 );
    for (size_t i = 0; i < 8; i++)
    {
        Counter[level][Counter_entry][i] = hashValue[i];
    }
}

//Please note the root node has no ancestor counters.
//This function needs to depart this situation.
bool vault::checkHashBlock(int level, int Counter_entry){
    int ptr=0;
    std::bitset<512> Counter_bitset;
    std::bitset<512> Ancestor_bitset(0);
    std::bitset<512> preHash(0);
    byte preHashChar[64];
    std::string hashValueString;
    byte hashValue[8];
    byte globalCounter[8];
    byte ancestor_counter_split[3*16];
    array_2_bitset(Counter[level][Counter_entry],64,Counter_bitset,ptr); //get L2 bitset
    Counter_bitset >>= 64;
    if(level != MT_LEVEL-1){
        splitCounter(Counter[level+1][Counter_entry/16],ancestor_counter_split,globalCounter,ptr); //get L3 counter
        array_2_bitset(ancestor_counter_split+(Counter_entry%16)*3,3,Ancestor_bitset,ptr);
        preHash = Counter_bitset | (Ancestor_bitset << 448);
    }
    else{
        preHash = Counter_bitset;
    }
    bitset_2_array(preHash,512,preHashChar,ptr);
    hashValueString = Sha256::getInstance().getHexMessageDigest(std::string((char *)preHashChar));
    memcpy(hashValue, (byte *)hashValueString.data(),8 );
    return strncmp((char*)hashValue,(char*)Counter[level][Counter_entry],8)==0;
}


//the xor
void vault::XorEngine(byte* src, byte* pad, byte* dst){
    std::bitset<512> src_bitset;
    std::bitset<512> pad_bitset;
    std::bitset<512> dst_bitset(0);
    int ptr=0;
    array_2_bitset(src,64,src_bitset,ptr);
    array_2_bitset(pad,64,pad_bitset,ptr);
    dst_bitset = src_bitset ^ pad_bitset;
    bitset_2_array(dst_bitset,512,dst,ptr);
}


//This function is reHash overall MT
void vault::reHash(){
    for (size_t i = 1; i < MT_LEVEL; i++)
    {
        for (size_t j = 0; j < mt_level_entry_num[i]; j++)
        {
            if (VAULT_DEBUG)
                std::cout<<"Init hash node: "<<i<<" "<<j;
            hashBlock(i,j);
            if (VAULT_DEBUG)
                std::cout<<" Hash Value: "<<std::hex<<*((unsigned long*)(Counter[i][j]))<<std::endl;
        }
    }
}


//this function will init the hash of MT as shadow MT
//this function will be called during allocate counter
void vault::initHash(){
    /*
    1. encrypt the counters by L2 MT
       every L2 76-b is used as key(Take notice that we peeding 64 global + 6/7 + 1b as the key)
    */
   byte key_split[16]={0};
   for (size_t i = 0; i < mt_level_entry_num[0]; i++)
   {
        getCounterKey(i,key_split);
        AES_Engine.Cipher512(Counter[0][i],Counter[0][i],key_split,16);
        // std::cout<<Counter[0][i]<<std::endl;
   }
   if(VAULT_DEBUG)
        std::cout<<"+++++>>>>Beginning encryption L1 counters Done!!!"<<std::endl;
   /*
   2. L2 - Li hash
   */
   reHash();
   
}



//call when class contruction initialization to allocate the Counter
void vault::allocCounter(){
    std::vector<byte*> tmp;
    if(VAULT_DEBUG) std::cout<<"Begin to allocate counters"<<std::endl;
    for (size_t i = 0; i < MT_LEVEL; i++)
    {
        Counter_modify_list.emplace_back(0);
        Counter_modify_list.emplace_back(0);
    }
    
}

//this function is to get the write trace of counters. Every corresponding
//counters needs to be incremented
void vault::getCounterTrace(unsigned long phy_addr){
    //L1 counter directly map as the addr
    Counter_modify_list.clear();
    int l1_blk_entry = phy_addr/(64*64);
    int l1_cnt_entry = (phy_addr%(64*64))/64;
    Counter_modify_list.push_back(l1_blk_entry);
    Counter_modify_list.push_back(l1_cnt_entry);
    //following level needs to calculate with arity
    int l2_blk_entry = l1_blk_entry/32;
    int l2_cnt_entry = l1_blk_entry%32;
    Counter_modify_list.push_back(l2_blk_entry);
    Counter_modify_list.push_back(l2_cnt_entry);
    //arity 16 with for loop
    int li_blk_entry = l2_blk_entry;
    int li_cnt_entry = l2_cnt_entry;
    for (size_t i = 0; i < MT_LEVEL-2; i++)
    {
        li_cnt_entry = li_blk_entry % 16;
        li_blk_entry /= 16;
        Counter_modify_list.push_back(li_blk_entry);
        Counter_modify_list.push_back(li_cnt_entry);
    }
    if(VAULT_DEBUG){
        std::cout<<"<<====The counter trace"<<std::endl;
        for (size_t i = 0; i < MT_LEVEL; i++)
        {
            std::cout<<"Level: "<<i<<":"<<Counter_modify_list[i*2]<<" "<<Counter_modify_list[i*2+1]<<std::endl;
        }
        std::cout<<"The counter trace====>>"<<std::endl;
    }
}

//increase single block counters
void vault::CounterIncreasement(byte* counters, int counter_index, int arity){
    byte splitL1Counter[64]={0};
    byte L1GlobalCounter[8]={0};
    unsigned long* Counter_ptr;
    bool reset;
    int ptr;
    splitCounter(counters,splitL1Counter,L1GlobalCounter,arity);
    if(arity ==64){
        reset = splitL1Counter[counter_index] == (VAULT_COMP ? 0x3f : 0x7f);
        if(reset){
            splitL1Counter[64] = {0};
            Counter_ptr = (unsigned long*)(L1GlobalCounter);
            *Counter_ptr +=1 ;
        }else{
            splitL1Counter[counter_index] += 1;
        }
        MixedCounter(counters,splitL1Counter,L1GlobalCounter,64);
    }
    else if (arity == 32)
    {
        reset = (splitL1Counter[counter_index*2] == 0xff) && (splitL1Counter[counter_index*2+1]==0xf);
        if(reset){
            splitL1Counter[64] = {0};
            Counter_ptr = (unsigned long*)(L1GlobalCounter);
            *Counter_ptr +=1 ;
        }else{
            if (splitL1Counter[counter_index*2] == 0xff)
            {
                splitL1Counter[counter_index*2+1] += 1;
                splitL1Counter[counter_index*2] = 0;
            }
            else{
                splitL1Counter[counter_index*2] += 1;
            }
        }
        MixedCounter(counters,splitL1Counter,L1GlobalCounter,32);
    }
    else if (arity == 16)
    {
        reset = (splitL1Counter[counter_index*3] == 0xff) && (splitL1Counter[counter_index*3+1]==0xff)
                && (splitL1Counter[counter_index*3+2] == 0xff);
        if(reset){
            splitL1Counter[64] = {0};
            Counter_ptr = (unsigned long*)(L1GlobalCounter);
            *Counter_ptr +=1 ;
        }else{
            if (splitL1Counter[counter_index*3] == 0xff)
            {
                if (splitL1Counter[counter_index*3+1] == 0xff){
                    splitL1Counter[counter_index*3+2] += 1;
                    splitL1Counter[counter_index*3] = 0;
                    splitL1Counter[counter_index*3+1] = 0;
                }
                else{
                    splitL1Counter[counter_index*3+1] += 1;
                    splitL1Counter[counter_index*3] = 0;
                }
            }
            else{
                splitL1Counter[counter_index*3] += 1;
            }
        }
        MixedCounter(counters,splitL1Counter,L1GlobalCounter,16);
    }

    if (VAULT_DEBUG)
    {
        if (reset)
        {
            std::cout<<"====>>Counter reset happpen in index: "<<counter_index<<"  Arity: "<<arity<<std::endl;
        }
        
    }

}




//this rehash function is the "blueprint"
void vault::writeTrace(unsigned long phy_addr){
    //this function simulates the processing of a cache evicted to memory
    //1.according the counter trace. read the cipher counter and L2 counter, and so on
    //2.according to L2 counter, decrypt the counter
    //3.all correspondinh counter incresed
    ////3.1 if counter full, reset all local counters
    //4. hash the data evicted to memory and store the hash value into memory
    //5.if "reset" re-encryption with increased counter to """"""all""""""" data block
    ////5.1 if supporting compression data compress the data 
    ////5.2 encrypt the data block and store
    
    /*
    1. get the cipher L1 counter and its ancestor counter, the result is push into modify_list
    */
    getCounterTrace(phy_addr);
    byte plainTextL1Counter[64];
    byte L1CounterKey[16];
    int L1CounterEntry = Counter_modify_list[0];
    int L1CounterIndex = Counter_modify_list[1];
    if (VAULT_DEBUG)
    {
        std::cout<<"Write Trace Debug <<<<========"<<std::endl;
        std::cout<<"L1 CounterEntry: "<< L1CounterEntry<<" L1 CounterIndex: "<<L1CounterIndex<<std::endl;
    }
    getCounterKey(L1CounterEntry,L1CounterKey);
    AES_Engine.InvCipher512(Counter[0][L1CounterEntry],plainTextL1Counter,L1CounterKey,16);
    if (VAULT_DEBUG)
    {
        std::cout<<"L1 counter Key: "<<std::hex<<long(L1CounterKey[0])<<std::endl;
        std::cout<<"PlainText L1 counter: "<<std::hex<<plainTextL1Counter<<std::endl;
    }
    /*
    2.Add the counter and its ancestor counter
      **Note, there may exist counter reset situation
    */
    //splitCounter countains 64 counters
    CounterIncreasement(plainTextL1Counter,L1CounterIndex,64);
    int LiCounterEntry = Counter_modify_list[2];
    int LiCounterIndex = Counter_modify_list[3];
    CounterIncreasement(Counter[1][LiCounterEntry],LiCounterIndex,32);
    for (size_t i = 0; i < MT_LEVEL-2; i++)
    {
        LiCounterEntry = Counter_modify_list[i*2+4];
        LiCounterIndex = Counter_modify_list[i*2+5];
        CounterIncreasement(Counter[i+2][LiCounterEntry],LiCounterIndex,16);
    }
    if (VAULT_DEBUG)
    {
        std::cout<<"Counter Increasment Done!!! "<<std::endl;
        std::bitset<512> tmp;
        int ptr;
        for (size_t i = 0; i < MT_LEVEL; i++)
        {
            LiCounterEntry = Counter_modify_list[i*2];
            array_2_bitset(Counter[i][LiCounterEntry],64,tmp,ptr);
            std::cout<<"Modifed Counters Value Level: "<<i<<" "<<tmp<<std::endl;
            std::cout<<"PreHash: "<<std::hex<<*((unsigned long*)Counter[i][LiCounterEntry])<<std::endl;
        }   
    }
    /*
    3. rehash the modified block
    */
    for (size_t i = 1; i < MT_LEVEL; i++)
    {
        LiCounterEntry = Counter_modify_list[i*2];
        hashBlock(i,LiCounterEntry);
    }
    if (VAULT_DEBUG)
    {
        std::cout<<"Rehash block done"<<std::endl;
    }
    /*
    4.in real situation, here is a significant step to re-encryt all nodes of this counters
    a counter is resetting meansing of all the needs to be recrypt. if no counters reset, only
    encrypt the singal node 
    */
    /*
    5.re-encrypt the L1 Counters
    */
    getCounterKey(L1CounterEntry,L1CounterKey);
    AES_Engine.Cipher512(plainTextL1Counter,Counter[0][L1CounterEntry],L1CounterKey,16);
    if (VAULT_DEBUG)
    {
        std::cout<<"Write Trace Debug ========>>>>"<<std::endl;
    }
}

void vault::readTrace(unsigned long phy_addr){
    //this function simulates the processing of a cache evicted to memory
    //1.according the counter trace. read the cipher counter and L2 counter, and so on
    //2.decrypt the data by counter and check hash
    //3.check the correspondding nodes' hash of tree
    /*
    1. get the cipher L1 counter and its ancestor counter, the result is push into modify_list
    */
    getCounterTrace(phy_addr);
    byte plainTextL1Counter[64];
    byte L1CounterKey[16];
    int L1CounterEntry = Counter_modify_list[0];
    int L1CounterIndex = Counter_modify_list[1];
    if (VAULT_DEBUG)
    {
        std::cout<<"Read Trace Debug <<<<========"<<std::endl;
        std::cout<<"L1 CounterEntry: "<< L1CounterEntry<<" L1 CounterIndex: "<<L1CounterIndex<<std::endl;
    }
    getCounterKey(L1CounterEntry,L1CounterKey);
    AES_Engine.InvCipher512(Counter[0][L1CounterEntry],plainTextL1Counter,L1CounterKey,16);
    if (VAULT_DEBUG)
    {
        std::cout<<"L1 counter Key: "<<std::hex<<long(L1CounterKey[0])<<std::endl;
        std::cout<<"PlainText L1 counter: "<<std::hex<<plainTextL1Counter<<std::endl;
    }

    /*
    2.decrypt the data by counter and check Data hash
    */
    //reserved for futher work


    /*
    3.check tree hash
    */
    bool node_check_iter=true;
    int LiCounterEntry;
    for (size_t i = 1; i < MT_LEVEL; i++)
    {
        LiCounterEntry = Counter_modify_list[i*2];
        node_check_iter &= checkHashBlock(i,LiCounterEntry);
    }
    if (VAULT_DEBUG)
    {
        std::cout<<"Check hash result: "<<node_check_iter<<std::endl;
        std::cout<<"Read Trace Debug ========>>>>"<<std::endl;
    }
}

vault::vault(bool fake){
    Counter = new byte**[MT_LEVEL];
    for (size_t i = 0; i < MT_LEVEL; i++)
    {
        Counter[i]=new byte*[mt_level_entry_num[i]];
        for (size_t j = 0; j < mt_level_entry_num[i]; j++)
        {
            Counter[i][j] = new byte[64];
        }
    }
    for (size_t i = 0; i < MT_LEVEL; i++)
    {
        for (size_t j = 0; j < mt_level_entry_num[i]; j++)
        {
            for (size_t k = 0; k < 64; k++)
            {
                Counter[i][j][k] = 0x0;
            }
            
        }
    }
}

//default constructor
vault::vault(bool DEBUG, int MEM_SIZE){
    VAULT_DEBUG = DEBUG;
    this->MEM_SIZE = MEM_SIZE;
    getMTLevel();
    int counter_num=0;
    Counter = new byte**[MT_LEVEL];
    for (size_t i = 0; i < MT_LEVEL; i++)
    {
        Counter[i]=new byte*[mt_level_entry_num[i]];
        for (size_t j = 0; j < mt_level_entry_num[i]; j++)
        {
            Counter[i][j] = new byte[64];
        }
    }
    for (size_t i = 0; i < MT_LEVEL; i++)
    {
        for (size_t j = 0; j < mt_level_entry_num[i]; j++)
        {
            for (size_t k = 0; k < 64; k++)
            {
                Counter[i][j][k] = 0x0;
            }
            
        }
    }

    allocCounter();
    initHash();
    AES_Engine = AES();
    std::cout<<"<<<<======== The vault is ready ========>>>>"<<std::endl;
}

vault::~vault(){
    for (size_t i = 0; i < MT_LEVEL; i++)
    {
        // Counter[i]=new byte*[mt_level_entry_num[i]];
        for (size_t j = 0; j < mt_level_entry_num[i]; j++)
        {
            delete [] Counter[i][j];
        }
    }
}

}