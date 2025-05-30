#!/bin/bash
export BASE=YOUR_PATH
#use timing to save checkpoint will be faster
for spec in dedup ferret fluidanimate freqmine streamcluster splash2x.barnes splash2x.radiosity splash2x.raytrace
do
    echo "Enter in new space"
    cd ${BASE}/gem5/m5out
    mkdir ${spec}_${1}core.cpt
    cd ${spec}_${1}core.cpt
    rm *
    ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/Parsec/run_${spec}_${1}core.sh --saveCpt ${BASE}/gem5/m5out/${spec}_${1}core.cpt/  --maxInst 20000000 --core ${1}
    sleep 1s
done


for spec in 525 531 541 557 507 519 526 502 520 523 505
# for spec in 541 520
do
    echo "Enter in new space"
    cd ${BASE}/gem5/m5out
    mkdir ${spec}_${1}core.cpt
    cd ${spec}_${1}core.cpt
    rm *
    ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/SPEC/run_${spec}_${1}core.sh --saveCpt ${BASE}/gem5/m5out/${spec}_${1}core.cpt/  --maxInst 1000000000 --core ${1}
    sleep 1s
done

for spec in bc-t pr-t cc-t bc-w pr-w cc-w
do
    echo "Enter in new space"
    cd ${BASE}/gem5/m5out
    mkdir ${spec}_${1}core.cpt
    cd ${spec}_${1}core.cpt
    rm *
    ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/GAP/run_${spec}_${1}core.sh --saveCpt ${BASE}/gem5/m5out/${spec}_${1}core.cpt/  --maxInst 1000000000 --core ${1}
    sleep 1s
done