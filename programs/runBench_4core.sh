
#!/bin/bash
# 222,905,686,420,   500,000,000,000 we must set the maxTick to measure IPC
echo "Enter in new space"
export BASE=YOUR_PATH
# ${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath ${BASE}/gem5/m5out/${1}.cpt/ --script ${BASE}/programs/scripts/SPEC/run_${1}.sh --maxInst 1000000000
${BASE}/gem5/build/RISCV/gem5.opt -d ${BASE}/gem5/m5out/checkpoint2 ${BASE}/gem5/configs/example/gem5_library/riscv-fs.py --cptPath ${BASE}/gem5/m5out/${1}.cpt/ --maxTick 250000000000 -o3 --core 4