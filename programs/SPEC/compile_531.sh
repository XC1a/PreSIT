export inst_count=0x100000000
export BASE=YOUR_PATH
export RVGCC=${BASE}/riscv-toolchain/bin/riscv64-unknown-linux-gnu-
export SPECPATH=/opt/cpu2017/benchspec/CPU

mkdir 531_${1}
cp enclave.c $SPECPATH/531.deepsjeng_r/src/
cd $SPECPATH/531.deepsjeng_r/src/

cp sjeng.cpp sjeng_new.cpp
if [ ${1} = "vault" ]; then
sed -i "8a #include \"./enclave.c\"" sjeng_new.cpp
sed -i "48a _EncStart();" sjeng_new.cpp
fi


${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o bits.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  bits.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o attacks.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  attacks.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o board.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  board.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o draw.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  draw.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o endgame.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  endgame.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o epd.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  epd.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o bitboard.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  bitboard.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o initp.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  initp.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o moves.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  moves.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o make.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  make.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o generate.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  generate.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o pawn.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  pawn.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o preproc.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  preproc.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o sjeng.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto   -D INST_COUNT=${inst_count}      -DSPEC_LP64  sjeng_new.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o see.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  see.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o neval.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  neval.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o state.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  state.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o ttable.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  ttable.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o search.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  search.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o utils.o -DSPEC -DNDEBUG -DSMALL_MEMORY -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  utils.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc      -g -O3 -march=rv64imafdc -flto          attacks.o bitboard.o bits.o board.o draw.o endgame.o epd.o generate.o initp.o make.o moves.o neval.o pawn.o preproc.o search.o see.o sjeng.o state.o ttable.o utils.o                      -o deepsjeng_r  


cp deepsjeng_r ../data/refrate/input/*  ${BASE}/programs/SPEC/531_${1}
rm *.o deepsjeng_r
