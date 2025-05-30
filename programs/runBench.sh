#!/bin/bash
echo "Enter in new space"
export BASE=YOUR_PATH
# ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath ${BASE}/gem5/m5out/${1}.cpt/ --script ${BASE}/programs/scripts/SPEC/run_${1}.sh --maxInst 1000000000
${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath ${BASE}/gem5/m5out/${1}.cpt/ --maxInst ${2} -o3