#!/bin/bash
#./build/RISCV/gem5.opt -d m5out/checkpoint0 configs/example/gem5_library/riscv-fs.py --checkpoint-dir=m5out/checkpoint0 --checkpoint-restore=1  --xx
./build/RISCV/gem5.opt \
-d m5out/checkpoint1 \
configs/example/riscv/fs_linux.py --virtio-rng \
 --cpu-clock=2GHz \
 --sys-clock=2GHz \
--num-cpus=1 \
--kernel=YOUR_PATH/riscv64-sample/riscv-pk/build16G/bbl \
--disk=YOUR_PATH/riscv_disk \
--cpu-type=RiscvTimingSimpleCPU \
--mem-type=DDR4_2400_16x4 \
--mem-size=16GB \
--mem-channels=1 \
--l1d_size=32kB --l1i_size=32kB --l1d_assoc=8 --l1i_assoc=8 \
--l2_size=512kB --l2_assoc=8 \
--l3_size=16MB --l3_assoc=8


