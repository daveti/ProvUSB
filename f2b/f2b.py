#!/usr/bin/python

# f2b.py
# Find the block number given the file name
# Only supports FAT16 so far
# Depends on Python Construct
# Aug 26, 2015
# root@davejingtian.org
# http://davejingtian.org

import sys
from fat16 import FatFs

# Global vars
debug = True

# Get the file name and FAT16 image
if (len(sys.argv)) != 3:
	print("usuage: ./f2b.py filename image")
	sys.exit(-1)

filename = sys.argv[1]
image = sys.argv[2]
if debug:
	print("filename: %s, image: %s" %(filename, image))

# Parse
with open(image) as f:
	fs = FatFs(f)
	for rootdir in fs:
		if debug:
			print(rootdir)
			print("firstCluster: %s, name: %s" %(rootdir.dirEntry.firstCluster, rootdir.name))
			print("filename(low): %s, name(low): %s" %(filename.lower(), rootdir.name.lower()))
		# Check the filename
		if filename.lower().strip() == rootdir.name.lower().strip():
			# Print the block num
			# NOTE: we assume the allocation unit is the same size
			# of a lba (512 bytes)
			print("block#: %d" %(rootdir.dirEntry.firstCluster-2))
			f.close()
			sys.exit(0)

print("Error: unable to find file: %s" %(filename))
f.close()

