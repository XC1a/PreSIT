cd /home/ubuntu/ubuntu/500_ori/
m5 resetstats
taskset -c 0 ./perlbench_r -I./lib splitmail.pl 6400 12 26 16 100 0 &
taskset -c 1 ./perlbench_r -I./lib splitmail.pl 6400 12 26 16 100 0 &
taskset -c 2 ./perlbench_r -I./lib splitmail.pl 6400 12 26 16 100 0 &
taskset -c 3 ./perlbench_r -I./lib splitmail.pl 6400 12 26 16 100 0