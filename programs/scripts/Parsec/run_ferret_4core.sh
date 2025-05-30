cd /home/ubuntu/Parsec/ferret
m5 resetstats
taskset -c 0 ./bin/ferret corel lsh queries 10 20 1 output.txt & taskset -c 1 ./bin/ferret corel lsh queries 10 20 1 output.txt & taskset -c 2 ./bin/ferret corel lsh queries 10 20 1 output.txt & taskset -c 3 ./bin/ferret corel lsh queries 10 20 1 output.txt