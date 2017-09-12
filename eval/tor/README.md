# Benchmark the Tor browser installed/started from USB storage
# I have to say that again: I hate udisks2!
# Jan 21, 2016
# daveti
1. umount whatever udisks2 automatically mounted
	umount /media/daveti/KINGSTON2G
2. remout the disk with proper permission to let Tor be happy
	mount -t vfat /dev/sdb1 /mnt/iso -o uid=1001,gid=1001,umask=000
3. unzip the Tor bundle and go into the dir
	./start-tor-browser.desktop
4. goto web.basemark.com
	click the damn button!
