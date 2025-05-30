cd /home/ubuntu/Parsec/splash2x.raytrace
m5 resetstats
taskset -c 0 ./run.sh 1 simlarge & taskset -c 1 ./run.sh 1 simlarge & taskset -c 2 ./run.sh 1 simlarge & taskset -c 3 ./run.sh 1 simlarge