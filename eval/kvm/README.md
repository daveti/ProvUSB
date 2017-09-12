# How to measure a KVM VM installation time
# Jan 20, 2016
# daveti
1. Allocate 8G for the KVM VM
	fallocate -l 8192M /var/lib/libvirt/images/provusb.img
2. Download the ubuntu-12.04-unattended to generate the unattended Ubuntu 14.04 server ISO
3. umount /media/daveti/whatever - "Fork udisks2 and the ACL"
4. mount -t vfat /dev/sdb1 /mnt/iso -o umask=000
5. Use the virt-install script with timing info to install the VM
	virt-install -r 1024 --accelerate -n ProvUbuntu -f /var/lib/libvirt/images/provusb.img --cdrom /mnt/iso/ubuntu-1404-server-amd64-provusb.iso
