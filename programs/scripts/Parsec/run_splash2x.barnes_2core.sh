cd /home/ubuntu/Parsec/splash2x.barnes
m5 resetstats
taskset -c 0 ./run.sh 1 & taskset -c 1 ./run.sh 1
