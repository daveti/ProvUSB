# ClamAV benchmarking for USB storages
# We use the official Linux kernel 4.4
# Jan 22, 2016
# No, fuck linux kernel source and fat16!
# We unzip the ubuntu14.04-amd64 server iso and do scanning
# Jan 23. 2016
# daveti
1. unzip the kernel into the storage
	tar xvfJ linux*.xz -C /media/daveti/whatever/
1. unzip the ubuntu-1404-amd64-server.iso (we have created for KVM testing)
	mount -o loop /path/to/iso /media/daveti/iso
	cp -rf /media/daveti/iso/* /path/to/usb/drive
2. apt-get install clamav
	installation from source code is NOT fun!
3. freshclam
4. clamscan -r /path/to/the/dir 
