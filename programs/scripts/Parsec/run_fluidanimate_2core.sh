cd /home/ubuntu/Parsec/fluidanimate
m5 resetstats
taskset -c 0 ./fluidanimate 1 5 in_100K.fluid out.fluid & taskset -c 1 ./fluidanimate 1 5 in_100K.fluid out.fluid
