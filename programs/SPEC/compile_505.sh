export inst_count=0x200000000
export BASE=YOUR_PATH
export RVGCC=${BASE}/riscv-toolchain/bin/riscv64-unknown-linux-gnu-
export SPECPATH=/opt/cpu2017/benchspec/CPU

mkdir 505_${1}
cp enclave.c $SPECPATH/505.mcf_r/src/
cd $SPECPATH/505.mcf_r/src/

cp mcf.c mcf_new.c

if [ ${1} = "vault" ]; then
sed -i "1a #include \"enclave.c\"" mcf_new.c
sed -i "132a _EncStart();" mcf_new.c
fi

${RVGCC}gcc -std=c99   -march=rv64imafdc -S -o mcf.s -DSPEC -DNDEBUG  -DSPEC_LP64 -D INST_COUNT=${inst_count} mcf_new.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o mcfutil.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64   mcfutil.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o readmin.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  readmin.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o implicit.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64   implicit.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o pstart.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  pstart.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o output.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  output.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o treeup.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  treeup.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o pbla.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  pbla.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o pflowup.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  pflowup.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o psimplex.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64   psimplex.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o pbeampp.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  pbeampp.c
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o spec_qsort.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64  spec_qsort/spec_qsort.c

${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o enclave.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP -D INST_COUNT=${inst_count} -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64 enclave.c

cp *.s *.o ../data/refrate/input/inp.in ${BASE}/programs/SPEC/505_${1}
rm *.s *.o mcf_new.c

cd ${BASE}/programs/SPEC/505_${1}
${RVGCC}gcc -std=c99   -march=rv64imafdc -c -o mcf.o -DSPEC -DNDEBUG -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP -g -O3 -march=rv64imafdc -flto -ffast-math -fno-strict-aliasing -fno-unsafe-math-optimizations -fno-finite-math-only -fgnu89-inline -fcommon -DSPEC_LP64 mcf.s

${RVGCC}gcc -std=c99   -march=rv64imafdc -g -O3 -march=rv64imafdc -flto -ffast-math enclave.o mcf.o mcfutil.o readmin.o implicit.o pstart.o output.o treeup.o pbla.o pflowup.o psimplex.o pbeampp.o spec_qsort.o -lm -o mcf_r
