
cd /home/ubuntu/541_ori/
m5 resetstats
taskset -c 0 ./leela_r ref.sgf &
taskset -c 1 ./leela_r ref.sgf &
taskset -c 2 ./leela_r ref.sgf &
taskset -c 3 ./leela_r ref.sgf