#!/bin/bash
for spec in 500 502 505 520 523 525 531 541 557 507 519 526
do
    echo "Enter in new space"
    cd YOUR_PATH/programs/SPEC
    sh compile_${spec}.sh ori
done