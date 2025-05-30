cd /home/ubuntu/Parsec/streamcluster
m5 resetstats
taskset -c 0 ./streamcluster 10 20 64 8192 8192 1000 none output.txt 1 & taskset -c 1 ./streamcluster 10 20 64 8192 8192 1000 none output.txt 1 & taskset -c 2 ./streamcluster 10 20 64 8192 8192 1000 none output.txt 1 & taskset -c 3 ./streamcluster 10 20 64 8192 8192 1000 none output.txt 1