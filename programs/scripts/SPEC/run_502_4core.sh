cd /home/ubuntu/502_ori/
m5 resetstats
taskset -c 0 ./cpugcc_r gcc-smaller.c -O3 -fipa-pta -o gcc-smaller.opts-O3_-fipa-pta.s &
taskset -c 1 ./cpugcc_r gcc-smaller.c -O3 -fipa-pta -o gcc-smaller.opts-O3_-fipa-pta.s &
taskset -c 2 ./cpugcc_r gcc-smaller.c -O3 -fipa-pta -o gcc-smaller.opts-O3_-fipa-pta.s &
taskset -c 3 ./cpugcc_r gcc-smaller.c -O3 -fipa-pta -o gcc-smaller.opts-O3_-fipa-pta.s