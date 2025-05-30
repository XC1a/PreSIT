export inst_count=0x100000000
export BASE=YOUR_PATH
export RVGCC=${BASE}/riscv-toolchain/bin/riscv64-unknown-linux-gnu-
export SPECPATH=/opt/cpu2017/benchspec/CPU

mkdir 519_${1}
cp enclave.c $SPECPATH/519.lbm_r/src/
cd $SPECPATH/519.lbm_r/src/

cp main.c main_new.c
if  [ ${1} = "vault" ]; then
sed -i "30a _EncStart();" main_new.c
fi

${RVGCC}gcc     -std=c99   -march=rv64imafdc -c -o enclave.o -DSPEC -DNDEBUG -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math         -fno-strict-aliasing  -D INST_COUNT=${inst_count}   -DSPEC_LP64  enclave.c
${RVGCC}gcc     -std=c99   -march=rv64imafdc -c -o lbm.o -DSPEC -DNDEBUG -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math         -fno-strict-aliasing     -DSPEC_LP64  lbm.c
${RVGCC}gcc     -std=c99   -march=rv64imafdc -c -o main.o -DSPEC -DNDEBUG -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math         -fno-strict-aliasing     -DSPEC_LP64  main_new.c
${RVGCC}gcc     -std=c99   -march=rv64imafdc      -g -O3 -march=rv64imafdc -flto -ffast-math   enclave.o      lbm.o main.o             -lm         -o lbm_r  


cp lbm_r ../data/refrate/input/* ${BASE}/programs/SPEC/519_${1}
rm  *.o 