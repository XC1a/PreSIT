#!/bin/bash
export BASE=YOUR_PATH
export NOW=${BASE}/programs
# for spec in 502 505 520 523 525 531 541 557 507 519 526
for spec in bc-t pr-t cc-t bc-w pr-w cc-w 525 531 541 557 507 519 526 502 520 523 505 
do
    sed -i '70s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '83s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '84s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '97s/true/false/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    sed -i '98s/false/true/g' ${BASE}/gem5/src/mem/mem_ctrl.hh
    cd ${BASE}/gem5/
    ./build_gem5.sh
    cd ${NOW}
    sh runBench.sh ${spec} ${2}
    cp ${BASE}/gem5/m5out/checkpoint2/stats.txt YOUR_PATH/recordRes/test${1}/${spec}_rpreOnly.stats
    sleep 1s
done