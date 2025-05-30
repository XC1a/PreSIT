# Copyright (c) 2021 The Regents of the University of California
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
This example runs a simple linux boot. It uses the 'riscv-disk-img' resource.
It is built with the sources in `src/riscv-fs` in [gem5 resources](
https://gem5.googlesource.com/public/gem5-resources).

Characteristics
---------------

* Runs exclusively on the RISC-V ISA with the classic caches
* Assumes that the kernel is compiled into the bootloader
* Automatically generates the DTB file
* Will boot but requires a user to login using `m5term` (username: `root`,
  password: `root`)
"""

import m5
from gem5.components.boards.riscv_board import RiscvBoard
from gem5.components.memory import SingleChannelDDR4_2400
from gem5.components.memory import DualChannelDDR4_2400
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.components.cachehierarchies.classic.private_l1_private_l2_cache_hierarchy import (
    PrivateL1PrivateL2CacheHierarchy,
)
from gem5.components.cachehierarchies.classic.private_l1_shared_l2_cache_hierarchy import (
    PrivateL1SharedL2CacheHierarchy,
)
from gem5.components.processors.cpu_types import CPUTypes
from gem5.isas import ISA
from gem5.utils.requires import requires
from gem5.resources.resource import Resource,CustomDiskImageResource,CustomResource
from gem5.simulate.simulator import Simulator
from m5.stats import reset, dump
from gem5.simulate.exit_event import ExitEvent

import sys
import pathlib
root_dir = str(pathlib.Path.home())
# Run a check to ensure the right version of gem5 is being used.
requires(isa_required=ISA.RISCV)
cmd={}
cmd['cptPath'] = ''
cmd['savecptPath'] = ''
cmd['command'] = ''
saveCheckpoint = False
maxTick=10**20
TickMode=False
maxInst=10**20
InstMode=False
i=0
useO3 = False
useTimg = False
coreNum = 1
memSize = 16
for line in sys.argv:
    if(line == "--cptPath"):
        cmd['cptPath'] = sys.argv[i+1]
    if(line == "--script"):
        cmd['command'] = sys.argv[i+1]
    if(line == "-o3"):
        useO3 = True
    if(line == "-timing"):
        useTimg = True
    if(line == "--mem"):
        memSize = sys.argv[i+1]
    if(line == "--saveCpt"):
        saveCheckpoint = True
        cmd['savecptPath'] = sys.argv[i+1]
    if(line == "--maxTick"):
        maxTick=int(sys.argv[i+1])
        TickMode = True
    if(line == "--maxInst"):
        maxInst=int(sys.argv[i+1])
        InstMode = True
    if(line == "--core"):
        coreNum=int(sys.argv[i+1])

    i+=1

def max_inst():
    warmed_up = False
    while True:
        if warmed_up:
            print("end of SimPoint interval")
            yield True
        else:
            print("end of warmup, starting to simulate SimPoint")
            warmed_up = True
            # Schedule a MAX_INSTS exit event during the simulation
            # simulator.schedule_max_insts(maxInst)
            dump()
            reset()
            yield True
# Setup the cache hierarchy.
# For classic, PrivateL1PrivateL2 and NoCache have been tested.
# For Ruby, MESI_Two_Level and MI_example have been tested.
cache_hierarchy = PrivateL1SharedL2CacheHierarchy(
    l1d_size="32KiB", l1i_size="32KiB", l2_size="8MB", l1d_assoc=2, l1i_assoc=2, l2_assoc=8
)

# Setup the system memory.
# memory = SingleChannelDDR4_2400(size="16GB")
memory = SingleChannelDDR4_2400(size=str(memSize)+"GB")
# memory = SingleChannelDDR4_2400(size="4GB")

# Setup a single core Processor.
if useO3:
    processor = SimpleProcessor(
        cpu_type= CPUTypes.O3, isa=ISA.RISCV, num_cores=coreNum
    )
else:
    if useTimg:
        processor = SimpleProcessor(
            cpu_type= CPUTypes.TIMING, isa=ISA.RISCV, num_cores=coreNum
        )
    else:
        processor = SimpleProcessor(
            cpu_type= CPUTypes.ATOMIC, isa=ISA.RISCV, num_cores=coreNum
        )

# The next exit_event is to simulate the ROI. It should be exited with a cause
# marked by `workend`.

# We exepect that ROI ends with `workend` or `simulate() limit reached`.
def handle_workend():
    print("Dump stats at the end")
    m5.stats.dump()
    yield True



# Setup the board.
board = RiscvBoard(
    clk_freq="2GHz",  # xrwang, to adapt the aes/hash latency (40ns = 80cyclse)
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)



# Set the Full System workload.
if coreNum == 1:
    board.set_kernel_disk_workload(
        kernel=CustomResource(
        root_dir+"/gem5-riscv/riscv64-sample/riscv-pk/build"+str(memSize)+"G/bbl",
        ),
        disk_image=CustomDiskImageResource(
        root_dir+"/gem5-riscv/riscv_disk",
        ),
        readfile=cmd['command']
    )
elif coreNum == 4 or coreNum == 2:
    board.set_kernel_disk_workload(
        kernel=CustomResource(
        root_dir+"/gem5-riscv/riscv64-sample/riscv-pk/build16G/bbl",
        ),
        disk_image=CustomDiskImageResource(
        root_dir+"/gem5-riscv/disk-image/ubuntu.img",
        disk_root_partition="1"
        ),
        readfile=cmd['command']
    )
    # board.set_kernel_disk_workload(
    #     kernel=CustomResource(
    #     root_dir+"/gem5-riscv/riscv64-sample/riscv-pk/build16G/bbl",
    #     ),
    #     disk_image=CustomDiskImageResource(
    #     root_dir+"/gem5-riscv/riscv_disk",
    #     ),
    #     readfile=cmd['command']
    # )

if coreNum == 1:
    if memSize == 16:
        checkpoint_default="YOUR_PATH/linux_checkpoint/cpt.13371155711500"
    elif memSize == 8:
        checkpoint_default="YOUR_PATH/linux_checkpoint/cpt.8G"
    elif memSize == 32:
        checkpoint_default="YOUR_PATH/linux_checkpoint/cpt.32G"
elif coreNum == 4:
    checkpoint_default="YOUR_PATH/linux_checkpoint/cpt.117611076246500"
elif coreNum == 2:
    checkpoint_default="YOUR_PATH/linux_checkpoint/cpt.2core"
if cmd['cptPath'] != 'xx':
    checkpoint_default = cmd['cptPath']

if cmd['cptPath']!='':
    # checkpoint_p="YOUR_PATH/linux_checkpoint/cpt.13371155711500"
    checkpoint_p = checkpoint_default
    if InstMode:
        simulator = Simulator(
            board=board,
            checkpoint_path=checkpoint_p,
            on_exit_event={ExitEvent.MAX_INSTS: max_inst()},
        )
    else:
        simulator = Simulator(
            board=board,
            checkpoint_path=checkpoint_p,
            on_exit_event={
            #     ExitEvent.WORKBEGIN: handle_workbegin(),
                ExitEvent.SCHEDULED_TICK: handle_workend(),
            },
        )
else:
    if InstMode:
        simulator = Simulator(
            board=board,
            on_exit_event={ExitEvent.MAX_INSTS: max_inst()},
        )
    else:
        simulator = Simulator(board=board)
print("Beginning simulation!")
print("Checkpoint is "+checkpoint_default)
# Note: This simulation will never stop. You can access the terminal upon boot
# using m5term (`./util/term`): `./m5term localhost <port>`. Note the `<port>`
# value is obtained from the gem5 terminal stdout. Look out for
# "system.platform.terminal: Listening for connections on port <port>".
if TickMode:
    # simulator.run(max_ticks=maxTick)
    # m5.scheduleTickExitFromCurrent(ticks=maxTick)
    content = open(checkpoint_p+'/m5.cpt')
    nowTick = 0
    for line in content:
        if line[0:11] == 'prvEvalTick':
            nowTick = int(line[12:])
            print("Now tick is " + str(nowTick))
            print("End tick is " + str(nowTick+maxTick))
            break
    m5.setMaxTick(nowTick+maxTick)
    simulator.run()
elif InstMode:
    simulator.schedule_max_insts(maxInst)
    simulator.run()
else:
    simulator.run()


simulator.get_simstats()
dump()

print(
    "Exiting @ tick {} because {}.".format(
        simulator.get_current_tick(), simulator.get_last_exit_event_cause()
    )
)

if saveCheckpoint:
    if cmd['savecptPath']!='':
        print("Taking a checkpoint at", cmd['savecptPath'])
        simulator.save_checkpoint(cmd['savecptPath'])
        print("Done taking a checkpoint")
