# PreSIT: Predict Cryptography Computations in SGX-Style Integrity Trees

# 1. Introduction of this Repo

This repository contains the simulation of PreSIT based on GEM5 FS mode. PreSIT can predict the cryptographic resulst in SGX-style Integrity Tree (SIT) to reduce the performance overhead caused by SIT. Two kinds of computations will be predicted, including AES decryption and Message Authentication Code (MAC). More details are shown in IEEE TCAD 2025 Paper:

Xinrui Wang, Lang Feng, Zhongfeng Wang, [PreSIT: Predict Cryptography Computations in SGX-Style Integrity Trees](https://ieeexplore.ieee.org/document/10643555), *IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems*, vol. 44, no. 3, pp. 882-896, March 2025.

Following sections will introduce the details of the implementation.

Contact: Xinrui Wang (mailto:xrwang@smail.nju.edu.cn) and Lang Feng ([fenglang3@mail.sysu.edu.cn](mailto:fenglang3@mail.sysu.edu.cn)).

# 2. Repository Contents
1. Folder `gem5`: This folder contains the gem5 simulator, the primary realization of PreSIT is in `gem5/src/mem/mem_ctrl.cc`
2. Folder `programs`: This folder contains the benchmarks that needs to run on PreSIT.
3. Folder `qemu` is empty, but needs to install by the user (will be further introduced later).
4. Folder `recordRest`: This folder records the meta results of benchmarks.
4. Folder `linux_checkpoint`: This folder records linux checkpoint after booting the linux system under different configurations, such as 2 4 cores or 32GB memory size.
5. Folder `riscv-toolchain` is empty, but needs to install riscv gcc chain by the user (will be further introduced later).
6. Folder `riscv64-sample` is key for linux system.
7. Folder `disk-image` stores the ubuntu linux image, but now is absent.
8. `README_UbuntuImage.md` records how to get the linux imgae for multicore.
9. `README_BBL.md` records how to get the linux imgae for single core.
10. `riscv_disk` is the linux disk image, storing the benchmark execution files for Gem5 Full-System (FS mode) simulation.
11. `ATT1/2.log` records the results of two simulated attacks mentioned in the paper.
12. `saveUbuntuImage.sh` is shell to store the linux booting checkpoint.
13. `TEE-paper-help.zip` contains the data and picture drawing python files for the paper. 


# 3. Environment Setup
## 3.1 Replace Path
After you download this repo, the first step is to replace the `YOUR_PATH` in this repo (this string will occur in a lot of files, you can use shell command or vscode to find and replace) to the absolute path of your repo, e.g. `YOUR_PATH` to `/home/your_name/PreSIT-master`

## 3.2 Prepare Linux system
We need to use the FS to simulate PreSIT and also need to reserve an enclave memory region to store the integrity tree. To achieve this goal, we refer to a [website](https://gem5.googlesource.com/public/gem5-resources/+/refs/heads/develop/src/riscv-fs/) or `README_BBL.md` in this repo to build. Here is a brief introduction of how to build, for details, please refer website.
- Install the riscv gcc toolchain in `riscv-toolchain`. The detailed information is shown in the website. Note that after you install it, you can see `riscv-toolchain/bin/riscv64-unknown-linux-gnu-*` in this repo.
- Build the `bbl` following the instruction. You need to store `bbl` in `riscv64-sample/riscv-pk/build{memSize}G/bbl`, the `memSize` can be 8,16, and 32, which means you need to rebuild the bbl if you want to change your memory size. **NOTE** that we provide the prebuilt bbl in the right place, which you can directly use.
- Build the `riscv_disk` and put it the repo root directory **NOTE this is enough for simulating single core configuration**.
- Download the ubuntu image to `disk-image/`. The link is https://gem5.googlesource.com/public/gem5-resources/+/8786f809bbbbcfb9aff4e4df0d2d21848442d706/src/riscv-ubuntu/README.md (also stored in this repo `README_UbuntuImage.md`). Please follow the necessary steps in this link **This is essential for simulating multicore configurations**.

Now you get all the requirements for linux system. If the user only wants to simulate 1-core configuation, it is no need to get the ubuntu image.

## 3.3 Boot your Linux system and get the checkpoint
Firstly, the reason for storing the checkpoint after booting is very simple: every time booting linux system consumes a lot of time even under `TimingCPU` model, about 30 minutes on regular PC. Therefore, we need to boot it at frist, and then restore it to simulate our programs. **NOTE** that we have already built all the checkpoints under different configurations, stored in `linux_checkpoint`. The user can direcly unzip the files in this folder to easily use. The user can also build their own checkpoint. This operation needs some special tricks so I listed blow:
1. Change line 70 `#define ENC_PROTECT true` of `gem5/src/mem/mem_ctrl.hh` to `false`, which means to disable the integrity tree operations. And please the change `line 109` and `line 110` to the right configurations, and `memSize` in `line 79` of `gem5/configs/example/gem5_library/riscv-fs.py`, details shown in Section 5.1. Remember to build Gem5 after you modify the C code.
2. Enter `gem5` folder, and run the shell `build_gem5.sh`.
3. If you want to get singlec-core checkpoint, run shell `saveUbuntuImage.sh 1`. After booting the system, inpur your username and keyword (mentioned `README_BBL.md`).
4. If you want get the multi-core checkpoint, you can directly run shell `saveUbuntuImage.sh n` (n means the number of cores). Then you will see the linux system with ubuntu is booting. Please firstly complete the operations in `README_UbuntuImage.md` (the linux username and password are also mentioned in this document), therefore you also need to install `qemu`.
5. After booting the system, you can connect this linux via `telnet localhost <port>`. Then the key is to execute `m5 checkpoint` and `m5 exit`. Then you will the latest checkpoint stored in `gem5/m5out/checkpoint2/`.

# 4. Compile the Benchmarks
**NOTE** that there is a key file `programs/SPEC/enclave.cc`, wich takes responsibility to initialize the enclave memory region.
## 4.1 SPEC CPU 2017
All the compile scripts of this benchmark are stored in `programs/SPEC`. You need to set the path of SPEC in every `compule_xxx.sh`, where `xxx` could be `502/505/...`. After that you can directly enter `programs/SPEC/` and run the shell `compile_all.sh`. Then your will get 11 cases in SPEC CPU 2017.

## 4.2 GAP
Enter `programs/GAP/` and run the shell `compile.sh`. Then, you will get the execution files.

## 4.3 PARSEC and SPLASH2x
This benchmark is very hard to download, so we directly provide the compiled version in `programs/Parsec`. The user can directly enter the folder and unzip the files. If users wants to build by themselves, please download the source code and imitate the SPEC compilation.

After you get all the compilted files, please copy them to the disk, i.e. `riscv_disk` or `ubuntu.image`. You can further rerfer to `README_BBL.md` or `README_UbuntuImage.md` for how to copy execution files to the disk.

# 5. Run the Benchmark Simulation
**NOTE** that we support 16GB + 1core, 16GB + 2cores, 16GB + 4cores, 4GB + 1core, and 32GB + 1core. Please remember and check every time you want to change the configurations as the following operations.
## 5.1 Basic Code Configurations
After getting the exeuction files and preparing the linux system, you can run the benchmarks. We firstly introduce the different configurations invloved in our code. The different configurations can be done by setting the `macro` in `gem5/src/mem/mem_ctrl.hh`. We list all the supported configurations below:
```c 
// this is file "gem5/src/mem/mem_ctrl.hh"
#define ENC_PROTECT true     //line 70
#define TURNON_PRE true      //line 83
#define TURNON_PRE_AES false //line 84
#define INPROG true          //line 95
#define PDA true             //line 97
#define APA true             //line 98
#define COREMEM 162          //line 109
#define _MEM 16              //line 110
#define ATT1 false           //line 134
#define ATT2 false           //line 135
#define ONLY_4KB false       //line 137
```
- `ENC_PROTECT` indicates if we enable the integrity tree operations, which can be used when we want to obtain the checkpoint.
- `TURNON_PRE` indicates if we enable `PreSIT-BASIC` mentioned in the paper, which means we predit MAC + 1 OTP.
- `TURNON_PRE_AES` indicates if we enable `OTPO` mentioned in the paper, which means we predict MAC + 2 OTPs.
- `INPROG` indicates if the program we want to evalute has loaded the checkpoint. Remember to set this value to `True` when you restore the corresponding benchmark' checkpoint. Otherwise, please set this value to `False`.
- `PDA` indicates if we enable `Prefetch Deciding Algorithm` mentioned in the paper, which will decide if to predict every time by using the recorded history GAP (LFIFO).
- `APA` indicates if enable `Address Preidcting Algorithm` mentioned in the paper, which means if we uses the address prediction algorithm or randomly predicting.
- `COREMEM` indicates the different configurations of the core number and memory size. `161` means 16GB+ 1core.
- `_MEM` indicates the memory size to calculate the address mask. `16` means 16GB.
- `ATT1` indicates if we are simulating the rowhammer attack.
- `ATT2` indicates if we are simulating the replay attack.
- `ONLY_4KB` indicates if we only check the beginning address of the 4kb region.

And please further manually modify the `memSize` in `line 79` of `gem5/configs/example/gem5_library/riscv-fs.py`.

## 5.2 Jump and Get Checkpoint
As we mentioned in the paper, we firstly jump the beginning segment of the benchmark, therefore we need to execute the program and then get each case's checkpoint. Then, we can enter the real simulation for performance measurement or other tests. Please set `ENC_PROTECT` as `false` and the right value of `COREMEM` and `_MEM` of the C code,and then build the gem5. Remember to modify `memSize` of `riscv-fs.py`. Then you can just enter `programs` run:
```bash
# for single core configuration
./saveCprPerbench.sh
// for 2 cores
./saveCprPerbench4Core.sh 2
// for 4 cores
./saveCprPerbench4Core.sh 4
```
## 5.3 Run the Benchmark
Then after get all the benchmarks and mannually modify `CORENUM` and `_MEM` to the right value based on your current configuration. Then also remember to set the `INPROG` to `true`, representing the program is already being executed. You can enter `programs` and run:
``` bash
# get 1-core results
./runAllSpec.sh [configuration] [maxInstruction]
# get 2-core results
./runAll2core.sh [configuration] [maxInstruction]
# get 4-core results
./runAll4core.sh [configuration] [maxInstruction]
```
These shells listed above can automatively change `TURNON_PRE` and `TURNON_PRE_AES` and build the gem5.

# 6. Run the Attack Simulation
The basic flow of running attack is simple. The first step is to compile the attack programs:
```bash
cd programs/Attack1-gradient
make
cd ../Attack2-matrix
make
```
Then you need to copy the execution file to the disk file, similar to the benchmarks Section 4.
Then please set the `ATT1/2` to `True`, `_MEM` to `16`, and `COREMEM` to `161` of `mem_ctrl.hh`.
```bash
cd gem5
./build_gem5.sh
./gem5-riscv/gem5/build/RISCV/gem5.opt -d ./gem5-riscv/gem5/m5out/checkpoint2 ./gem5-riscv/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx -timing
```
Then you will see the linux shell window. Enter the directory of `ATT1/2` and execute the exeuction file. You will get the results of the rowhammer/replay attack simulation.

# Appendix: Code Introduction

**NOTE** that we only evaluate the performance of SIT and perfomance improvoment of our proposed PreSIT, we only focus on the latency of AES and HMAC2 (both are 40ns) when we are evaluating the SIT performance.

The main code of SIT parallel operation flow and PreSIT is located within `gem5/src/mem/mem_ctrl.cc`, and some critical data structures and functions are defined within `gem5/src/mem/mem_ctrl.hh`. In this section, we will roughtly introduce the idea and operations of the implementation (following instruction can hugely help the user understand, but the details still need user to read carefully). We will follow the flow of memory writing or reading to introduce. The line number mentioned in this appendix all relates to the `mem_ctrl.cc`.
### Step 1: Recieve Request
```c
//memory controller receive the memory request from LLC
MemCtrl::recvTimingReq(PacketPtr pkt)   //line 2760
{
    //...
    //clear the data structure for this round' process
    splitOriPkt_cnt = 0;                    //line 3058
    //...
    //use the EncEvent to handle SIT operations
    schedule(EncEvent, curTick());           //line 3084
    //...
    //use the 
    if(... hasResponseRead && pkt->isRead()) //line 3088
    {
        //...
    }
}
```
When the LLC needs to send the request, it will be captured by the `recvTimingReq` in line 2760 of the `mem_ctrl.cc`. The user can read this code to learn more information. Then the data structure for SIT operations and PreSIT will be cleared from line 3058. In line 3085, a critical event `EncEvent` will be scheduled to handle the SIT operations (gem5 is based on event-programming). Then from line 3088, we handle the special situation: the reading request hit the write buffer. As the user could see, we will directly `return true;` in this function, which means the SIT operation is partly in parallel with LLC reading/writing request.

### Step2: Handle Request by `EncEvent`
#### Parallel Operation Flow
After the `EncEvent` being scheduled, the function `void MemCtrl::EncStateMachine()` will be called back to handle this event. For simply understand, scheduling `EncEvent` means to call `EncStateMachine` in a certain time. This function is designed as a statemachine by calling itself (`schedule(EncEvent, [time])`) to change the state.
```c
MemCtrl::EncStateMachine()                    //line 620
{
    //...
    if (stateEnc == StatesEnc::SPLITAG)       //line 632
    //...
    if (stateEnc == StatesEnc::RDCNT)         //line 658
    {
        + vault_engine.getCounterTrace(splitOriPkt[splitOriPkt_cnt] & ENCALVE_MASK); //line 725
        + bool evict = encCache.EncRead(counterAddr,holdCnt);      //line 766
        + EncGenPacket(pkt_encBuild,req0,counterAddr,64,encCache.RdMiss->blk,0); //line 931
    }
    //...
    if (stateEnc == StatesEnc::CNTOP)         //line 1056
    {
        + vault_engine.counterPropragate(writeHashNum); //1098
    }
    //...
    if (stateEnc == StatesEnc::CNTSANITY)     //line 1176 Deprecated
    //...
    if (stateEnc == StatesEnc::WBLIMB)        //line 1222
    {
        + updateTreeMAC();                    //line 1233
        + bool ifwb = encCache.EncWrite() ... //line 1243
        + hasReset |= vault_engine.resetRecord[i]; //line 1271
    }
    //...
    if (stateEnc == StatesEnc::DORESETTREE)   //line 1335
    {
        ... // RstSate; from line 1335 to 1818
    }
    //...
    if (stateEnc == StatesEnc::WUPMAC)        //line 1821
    {
        + EncGenPacket(pkt_encBuild,req0,macCache.Evicted_ptr.addr,64,...); //line 1867
    }
    //...
    if (stateEnc == StatesEnc::RRDMAC)        //line 1883
    {
        + bool evict = macCache.EncRead(MACbeginAddr,chunk); //line 1901
    }
    //...
    if (stateEnc == StatesEnc::RRDMAC_WB)     //line 1974
    //...
    if (stateEnc == StatesEnc::RRDMAC_REFILL) //line 1961
    //...
}
```
Details such as operations of above states are shown below, **RW** means both memory reading and writing need this operation:
- `SPLITAG` handles the situation, where a large packet needs spliting to several packets. This can be ignored under riscv.
- `RDCNT` (**RW** Both) handles the operation of reading SIT counters from the `encCache` (is dedicated for caching counter and the definition is shown in `mem_ctrl.hh`) or memory, depending on if the counter is hit. (1) This state will first get the address of the counters in line 725, by a data strcuture `vault_engine` (details are shwon in `mem_ctrl.hh`), which is designed to handle the general operations needed by SIT, such as getting the address of counters and update the counter values. (2) Then, based on counter address, `encCache` will be accessed in line 766. (3) If the counter misses, the statemachine will generate new memory reading request to fetch the counter in line 931.
- `CNTOP` (**W** Only) handles the memory writing's SIT counter incresement. The counter will increase based on the writing address, by the `vault_engine.counterPropragate()` in line 1098.
- `WBLIMB` (**W** Only) will firstly update the calculated Tree MAC (please distinguish this with Data MAC by reading the `Fig 1` of the paper) and hash by `updateTreeMAC()` then write back them in line 1243. At the same,`vault_engine` will also record if there will be a reset in line 1271.
- `DORESETTREE` (**W** only) handles the operations of counter reset, which includes a lot of operations, managed by another sub-statemachine named `RstSate`. If the user is interested, more details are shown for line 1335 to 1818.
- `WUPMAC` (**W** only) will write the fresh/calculated Data MAC to the memory. Note that the operations in `P.2`, including writing back Ciphertext, counter, and MAC will be parallel because of the write buffer of the memory controller.
- `RRDMAC` (**R** only) will read the Data MAC for verification when memory reading happens. Note that the Data MAC will also be cached by `macCache`, which is defined in `mem_ctrl.hh`.
- `RRDMAC_WB` and `RRDMAC_REFILL` (**R** only) will handle the situation if the MAC misses or replacement happens in `macCache`.

Besides, another function is also modified to process the next request (function `processNextReqEvent` from line 3885 to 4408) because we needs to schedule or reschedule during SIT or prediction operations in `EncEvent`.
```c
//line 3885 to 4408
void
MemCtrl::processNextReqEvent(MemInterface* mem_intr,
                        MemPacketQueue& resp_queue,
                        EventFunctionWrapper& resp_event,
                        EventFunctionWrapper& next_req_event,
                        bool& retry_wr_req) {
                            ...
                        }
```

#### PreSIT Prediction Operations
```c
MemCtrl::EncStateMachine()                    //line 620
{
    //....
    if (stateEnc == StatesEnc::RDONE)         //line 2006
    {
        if (hasCorrectPrediction)             //line 2044
        {
            stateEnc = StatesEnc::DATATRAN;   //line 2054
        }
        else if(hasToWaitCal)                 //line 2056
        {
            stateEnc = StatesEnc::DATATRAN;   //line 2080
        }
        else
        {
            if (overlapHashRDMAC < gapPre.getAvgPrefetch())       //line 2097
            {
                if (isChecking)                                   //line 2102
                {
                    stateEnc = StatesEnc::DATATRAN;
                }
                else
                {
                    if ((overlapHashRDMAC + gapPre.getAvgGap()) < gapPre.getAvgPrefetch()) //line 2112
                    {
                        stateEnc = StatesEnc::DATATRAN;
                    }
                    else
                    {
                        stateEnc = StatesEnc::PREDICTION_PREFETCH_DECIDE;
                    }
                }
            }
            else
            {
                stateEnc = StatesEnc::PREDICTION_PREFETCH_DECIDE;
            }
        }
    }
}
```
Then we will introduce the code about prediction. The first step for PreSIT is to decide if to enable prediction this time in the state `RDONE`. The rough code for this deciding algorihtm is shown above and there are serval situations:
- line 2044: If `hasCorrectPrediction` is true, it represents get the right prediction, we dont predict this time and jump to `DATATRAN` and do nothing for prediction.
- line 2050: If `hasToWaitCal` is true, it represents the right MAC is being calculated. Similar to line 2044, we do nothing for prediction but waiting for the end of the calculation.
- line 2097: The remaining time for prediction after overlapping reading MAC and verifying MAC is `overlapHashRDMAC`. IF this value larget than the time for prefetching (line 2097), we directly enbale the prediction. Otherwise it will further analyze.
- line 2102: If another memory reading/writing request is coming into memory controller, indicated by `isChecking` in line 2102, we will cancel the prediction because dealing with the new request is more urgent (for better performance).
- line 2112: This is contion for further to see if the controller has the enough time to predict, as introduced in `Fig 8` of the paper.

```c
    //...
    if (stateEnc==StatesEnc::PREDICTION_PREFETCH_DECIDE)  //line 
    {
        + bool hasFindGroup = teePrediction.findGroupRange(preDataAddr,groupPreTInx,true); //line 2258
    }
    //...
    if (stateEnc==StatesEnc::PREDICTION_PREFETCH_DO)      //line 2409
    //...
    if (stateEnc == StatesEnc::PREDICTION_PREFETCH_WAIT)  //line 2507

    //...
    if (stateEnc == StatesEnc::PREDICTION_CAL)            //line 2512
    {
        + schedule(EncEvent, curTick() + (overlapHashRDMAC-PrefetchCycle)*CYCLETICK ); //line 2563
    }
```
If the controller decides to predict, there are mainly four steps.
- `PREDICTION_PREFETCH_DECIDE` will decide the address to prefetch and predict. At the same time it will access the prediction table (PT in the paper), which is named `teePrediction` in the code. The group and entry will be also allocated in this state.
- `PREDICTION_PREFETCH_DO` and `PREDICTION_PREFETCH_WAIT` wiil prefetch the data and waiting for the reponse from the memory side.
- `PREDICTION_CAL` wiil precompute MAC by scheduling the event to simulate the time consuming (line 2563).

### Step3: Handle the Response
As we can see, SIT operations or prediction operations need to access the memroy. Therefore, we need also to handle the memory response after sending the memory access request. The function to deal with the response is `processRespondEvent`. This part is relatively simple so that the user can directly read the code.

```c
// line 3222 to 3547
void
MemCtrl::processRespondEvent(MemInterface* mem_intr,
                        MemPacketQueue& queue,
                        EventFunctionWrapper& resp_event,
                        bool& retry_rd_req)
```