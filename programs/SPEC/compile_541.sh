export inst_count=0x100000000
export BASE=YOUR_PATH
export RVGCC=${BASE}/riscv-toolchain/bin/riscv64-unknown-linux-gnu-
export SPECPATH=/opt/cpu2017/benchspec/CPU

mkdir 541_${1}
cp enclave.c $SPECPATH/541.leela_r/src/
cd $SPECPATH/541.leela_r/src/

cp Leela.cpp Leela_new.cpp
if [ ${1} = "vault" ]; then
sed -i "9a #include \"./enclave.c\"" Leela_new.cpp
sed -i "25a _EncStart();" Leela_new.cpp
fi


${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o TimeControl.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  TimeControl.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o FullBoard.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  FullBoard.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o Playout.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  Playout.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o KoState.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  KoState.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o UCTSearch.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  UCTSearch.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o Timing.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  Timing.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o GameState.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  GameState.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o SGFParser.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  SGFParser.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o Utils.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  Utils.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o Leela.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto  -D INST_COUNT=${inst_count}         -DSPEC_LP64  Leela_new.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o TTable.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  TTable.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o Matcher.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  Matcher.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o Zobrist.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  Zobrist.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o SGFTree.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  SGFTree.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o FastBoard.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  FastBoard.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o FastState.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  FastState.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o Random.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  Random.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o SMP.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  SMP.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o MCOTable.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  MCOTable.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o GTP.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  GTP.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc -c -o UCTNode.o -DSPEC -DNDEBUG -I. -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -march=rv64imafdc -flto         -DSPEC_LP64  UCTNode.cpp
${RVGCC}g++     -std=c++03 -march=rv64imafdc      -g -O3 -march=rv64imafdc -flto          FullBoard.o KoState.o Playout.o TimeControl.o UCTSearch.o GameState.o Leela.o SGFParser.o Timing.o Utils.o FastBoard.o Matcher.o SGFTree.o TTable.o Zobrist.o FastState.o GTP.o MCOTable.o Random.o SMP.o UCTNode.o                      -o leela_r  

cp leela_r ../data/refrate/input/*  ${BASE}/programs/SPEC/541_${1}
rm *.o leela_r
