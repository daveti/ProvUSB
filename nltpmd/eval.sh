#!/bin/bash

echo "Loading the testing kernel module"

sudo insmod ./ut_kmod/nltpmd_kernel_mod.ko

echo "Running nltpmd ..."

(time ./nltpmd > /dev/null) 2> time_out.txt

echo "Done!"

echo "Unloading the testing kernel module"

sudo rmmod nltpmd_kernel_mod
