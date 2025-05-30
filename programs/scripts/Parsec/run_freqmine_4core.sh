cd /home/ubuntu/Parsec/freqmine
m5 resetstats
export OMP_NUM_THREADS=1
taskset -c 0 ./freqmine kosarak_500k.dat 410 & taskset -c 1 ./freqmine kosarak_500k.dat 410 & taskset -c 2 ./freqmine kosarak_500k.dat 410 & taskset -c 3 ./freqmine kosarak_500k.dat 410