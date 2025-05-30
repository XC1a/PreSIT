cd /home/ubuntu/Parsec/dedup
m5 resetstats
taskset -c 0 ./dedup -c -p -v -t 1 -i media.dat -o output.dat.ddp & taskset -c 1 ./dedup -c -p -v -t 1 -i media.dat -o output.dat.ddp & taskset -c 2 ./dedup -c -p -v -t 1 -i media.dat -o output.dat.ddp & taskset -c 3 ./dedup -c -p -v -t 1 -i media.dat -o output.dat.ddp