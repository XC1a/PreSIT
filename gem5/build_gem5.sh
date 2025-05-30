#!/bin/bash

export target=${1}

export mode="inst"
export runmode="normal"
export exeplace="pc"

for var in "$@"
do
	if [ ${var} == "-pc" ]; then
		export exeplace="pc"
	elif [ ${var} == "-s3" ]; then
		export exeplace="s3"
	elif [ ${var} == "-s161" ]; then
		export exeplace="s161"
	fi
done

if [ ${exeplace} == "pc" ]; then

scons build/RISCV/gem5.opt -j8 

elif [ ${exeplace} == "s3" ]; then

scons build/RISCV/gem5.opt -j80

else

/usr/bin/env python3 $(which scons) build/RISCV/gem5.opt -j80

fi
