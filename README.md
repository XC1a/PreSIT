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
## 5.1 Basic Code View
After getting the exeuction files and preparing the linux system, you can run the benchmarks. We firstly introduce the different configurations invloved in our code. The different configurations can be done by setting the `macro` in `gem5/src/mem/mem_ctrl.hh`. We list all the supported configurations below:
```c 
// this is file "gem5/src/mem/mem_ctrl.hh"
#define ENC_PROTECT true     //line 70
#define TURNON_PRE true      //line 83
#define TURNON_PRE_AES false //line 84
#define PDA true             //line 97
#define APA true             //line 98
#define COREMEM 162          //line 109
#define _MEM 16              //line 110
#define ATT1 false           //line 134
#define ATT2 false           //line 135
```
- `ENC_PROTECT` indicates if we enable the integrity tree operations, which can be used when we want to obtain the checkpoint.
- `TURNON_PRE` indicates if we enable `PreSIT-BASIC` mentioned in the paper, which means we predit MAC + 1 OTP.
- `TURNON_PRE_AES` indicates if we enable `OTPO` mentioned in the paper, which means we predict MAC + 2 OTPs.
- `PDA` indicates if we enable `Prefetch Deciding Algorithm` mentioned in the paper, which will decide if to predict every time by using the recorded history GAP (LFIFO).
- `APA` indicates if enable `Address Preidcting Algorithm` mentioned in the paper, which means if we uses the address prediction algorithm or randomly predicting.
- `COREMEM` indicates the different configurations of the core number and memory size. `161` means 16GB+ 1core.
- `_MEM` indicates the memory size to calculate the address mask. `16` means 16GB.
- `ATT1` indicates if we are simulating the rowhammer attack.
- `ATT2` indicates if we are simulating the replay attack.

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
Then after get all the benchmarks and mannually modify `CORENUM` and `_MEM` to the right value based on your current configuration. You can enter `programs` and run:
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
You will get the results of the rowhammer/replay attack simulation.