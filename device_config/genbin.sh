#!/bin/bash

# genbin.sh
# Generate the binary data
# currently used to generate 284 bytes 0x1
# Jul 29, 2014
# root@davejingtian.org
# http://davejingtian.org

i=0
output="output.bin"
touch "$output" 
while [ $i -lt 284 ]
do
	echo -n $'\x01' >> "$output"
	i=$((i+1))
done
