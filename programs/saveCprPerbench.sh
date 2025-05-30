#!/bin/bash
export BASE=YOUR_PATH
#use timing to save checkpoint will be faster
# for spec in 502 505 520 523 525 531 541 557 507 519 526
for spec in 525 531 541 557 507 519 526 502 520 523 505 
do
    echo "Enter in new space"
    cd ${BASE}/gem5/m5out
    mkdir ${spec}.cpt
    cd ${spec}.cpt
    rm *
    ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/SPEC/run_${spec}.sh --saveCpt ${BASE}/gem5/m5out/${spec}.cpt/  --maxInst 1000000000
    sleep 1s
done

for gap in bc-t pr-t cc-t bc-w pr-w cc-w
do
    echo "Enter in new space"
    cd ${BASE}/gem5/m5out
    mkdir ${gap}.cpt
    cd ${gap}.cpt
    rm *
    ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/GAP/run_${gap}.sh --saveCpt ${BASE}/gem5/m5out/${gap}.cpt/ --maxInst 1000000000 
    sleep 1s
done

for parsec in blackscholes dedup ferret fluidanimate freqmine streamcluster swaptions
do
    echo "Enter in new space"
    cd ${BASE}/gem5/m5out
    mkdir ${parsec}.cpt
    cd ${parsec}.cpt
    rm *
    ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/Parsec/run_${parsec}.sh --saveCpt ${BASE}/gem5/m5out/${parsec}.cpt/ --maxInst 1000000000 
    sleep 1s
done

for parsec in splash2x.barnes splash2x.radiosity splash2x.raytrace
do
    echo "Enter in new space"
    cd ${BASE}/gem5/m5out
    mkdir ${parsec}.cpt
    cd ${parsec}.cpt
    rm *
    ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/Parsec/run_${parsec}.sh --saveCpt ${BASE}/gem5/m5out/${parsec}.cpt/ --maxInst 20000000
    sleep 1s
done