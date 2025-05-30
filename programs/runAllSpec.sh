#!/bin/bash
export BASE=YOUR_PATH
export NOW=${BASE}/programs
# for spec in 502 505 520 523 525 531 541 557 507 519 526
# for spec in bc-t pr-t cc-t bc-w pr-w cc-w 525 531 541 557 507 519 526 502 520 523 505 
# for spec in bc-t pr-t cc-t bc-w pr-w cc-w 525 531 541 557 507 519 526 502 520 523 505 blackscholes dedup ferret fluidanimate freqmine streamcluster swaptions
# for spec in dedup ferret fluidanimate freqmine streamcluster splash2x.barnes splash2x.radiosity splash2x.raytrace
for spec in dedup ferret fluidanimate freqmine streamcluster splash2x.barnes splash2x.radiosity splash2x.raytrace bc-t pr-t cc-t bc-w pr-w cc-w 525 531 541 557 507 519 526 502 520 523 505
# for spec in splash2x.ocean_np splash2x.barnes splash2x.radiosity splash2x.raytrace
do
    echo "Enter in new space"
    sed -i '70s/true/false/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '83s/true/false/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '84s/true/false/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    cd ${BASE}/gem5/
    ./build_gem5.sh
    cd ${NOW}
    sh runBench.sh ${spec} ${2}
    cp ${BASE}/gem5/m5out/checkpoint2/stats.txt YOUR_PATH/recordRes/test${1}/${spec}_ori.stats
    sleep 1s

    sed -i '70s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '83s/true/false/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '84s/true/false/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    cd ${BASE}/gem5/
    ./build_gem5.sh
    cd ${NOW}
    sh runBench.sh ${spec} ${2}
    cp ${BASE}/gem5/m5out/checkpoint2/stats.txt YOUR_PATH/recordRes/test${1}/${spec}_vault.stats
    sleep 1s

    sed -i '70s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '83s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '84s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    cd ${BASE}/gem5/
    ./build_gem5.sh
    cd ${NOW}
    sh runBench.sh ${spec} ${2}
    cp ${BASE}/gem5/m5out/checkpoint2/stats.txt YOUR_PATH/recordRes/test${1}/${spec}_rpreOnly.stats
    sleep 1s

    sed -i '70s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '83s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '84s/true/false/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    cd ${BASE}/gem5/
    ./build_gem5.sh
    cd ${NOW}
    sh runBench.sh ${spec} ${2}
    cp ${BASE}/gem5/m5out/checkpoint2/stats.txt YOUR_PATH/recordRes/test${1}/${spec}_rpreOnlyNoAes.stats
    sleep 1s
done

# for gap in bc-t pr-t cc-t bc-w pr-w cc-w
# do
#     echo "Enter in new space"
#     ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath xx --script ${BASE}/programs/scripts/GAP/run_${gap}.sh --maxInst ${2}
#     cp ${BASE}/gem5/m5out/checkpoint2/stats.txt YOUR_PATH/recordRes/test${1}/${gap}.stats
#     sleep 1s
# done