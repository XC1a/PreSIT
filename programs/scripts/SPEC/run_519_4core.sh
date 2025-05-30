cd /home/ubuntu/519_ori/
m5 resetstats
taskset -c 0 ./lbm_r 3000 reference.dat 0 0 100_100_130_ldc.of &
taskset -c 1 ./lbm_r 3000 reference.dat 0 0 100_100_130_ldc.of &
taskset -c 2 ./lbm_r 3000 reference.dat 0 0 100_100_130_ldc.of &
taskset -c 3 ./lbm_r 3000 reference.dat 0 0 100_100_130_ldc.of