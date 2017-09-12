#!/usr/bin/env python

# Changes:
# 	Disable reverse byte order
#	Aug 18, 2014
#	daveti
#
# A simple python script used to convert the txt file into binary file
# ./genbin2.py -i input_file -o output.file
# Aug 14, 2014
# root@davejingtian.org
# http://davejingtian.org

import sys
import getopt

# Global defs
data_array = []
enable_debug = True
reverse_byteorder = False
input_file = ''
output_file = 'output.bin'
help_string = './genbin2.py -i <input_file> -o <output_file>'

# Process the arguments
try:
	opts, args = getopt.getopt(sys.argv[1:], "hi:o:", ["ifile=","ofile="])
except getopt.GetoptError:
	print(help_string)
	sys.exit(2)

for opt, arg in opts:
	if opt == '-h':
		print(help_string)
		sys.exit()
	elif opt in ("-i", "--ifile"):
		input_file = arg
	elif opt in ("-o", "--ofile"):
		output_file = arg

if enable_debug == True:
	print('input [%s], output [%s]' %(input_file, output_file))

# Read the input
ifile = open(input_file, 'r')
for line in ifile:
	if enable_debug == True:
		print(line)
	for hd in line.split():
		data_array.append(int(hd, 16))
ifile.close()

# Reverse byte order?
if reverse_byteorder == True:
	left_array = data_array[::2][:]
	right_array = data_array[1::2][:]
	data_array = []
	array_len = min(len(left_array), len(right_array))
	for i in range(array_len):
		data_array.append(right_array[i])
		data_array.append(left_array[i])
	if len(left_array) > len(right_array):
		data_array.append(left_array[-1])
	elif len(right_array) > len(left_array):
		data_array.append(right_array[-1])

if enable_debug == True:
	print('data_array length [%d]' %(len(data_array)))
	print(data_array)

# Write to the output
ofile = open(output_file, 'w')
ofile.write(bytearray(data_array))
ofile.close()
