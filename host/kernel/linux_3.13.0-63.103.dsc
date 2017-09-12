-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA1

Format: 1.0
Source: linux
Binary: linux-source-3.13.0, linux-doc, linux-headers-3.13.0-63, linux-libc-dev, linux-tools-common, linux-tools-3.13.0-63, linux-cloud-tools-common, linux-cloud-tools-3.13.0-63, linux-image-3.13.0-63-generic, linux-image-extra-3.13.0-63-generic, linux-headers-3.13.0-63-generic, linux-image-3.13.0-63-generic-dbgsym, linux-tools-3.13.0-63-generic, linux-cloud-tools-3.13.0-63-generic, linux-udebs-generic, linux-image-3.13.0-63-generic-lpae, linux-image-extra-3.13.0-63-generic-lpae, linux-headers-3.13.0-63-generic-lpae, linux-image-3.13.0-63-generic-lpae-dbgsym, linux-tools-3.13.0-63-generic-lpae, linux-cloud-tools-3.13.0-63-generic-lpae, linux-udebs-generic-lpae, linux-image-3.13.0-63-lowlatency, linux-image-extra-3.13.0-63-lowlatency, linux-headers-3.13.0-63-lowlatency, linux-image-3.13.0-63-lowlatency-dbgsym, linux-tools-3.13.0-63-lowlatency, linux-cloud-tools-3.13.0-63-lowlatency, linux-udebs-lowlatency, linux-image-3.13.0-63-powerpc64-emb,
 linux-image-extra-3.13.0-63-powerpc64-emb, linux-headers-3.13.0-63-powerpc64-emb, linux-image-3.13.0-63-powerpc64-emb-dbgsym, linux-tools-3.13.0-63-powerpc64-emb, linux-cloud-tools-3.13.0-63-powerpc64-emb, linux-udebs-powerpc64-emb, linux-image-3.13.0-63-powerpc64-smp, linux-image-extra-3.13.0-63-powerpc64-smp, linux-headers-3.13.0-63-powerpc64-smp, linux-image-3.13.0-63-powerpc64-smp-dbgsym, linux-tools-3.13.0-63-powerpc64-smp, linux-cloud-tools-3.13.0-63-powerpc64-smp, linux-udebs-powerpc64-smp, linux-image-3.13.0-63-powerpc-e500, linux-image-extra-3.13.0-63-powerpc-e500, linux-headers-3.13.0-63-powerpc-e500, linux-image-3.13.0-63-powerpc-e500-dbgsym, linux-tools-3.13.0-63-powerpc-e500, linux-cloud-tools-3.13.0-63-powerpc-e500, linux-udebs-powerpc-e500, linux-image-3.13.0-63-powerpc-e500mc, linux-image-extra-3.13.0-63-powerpc-e500mc, linux-headers-3.13.0-63-powerpc-e500mc, linux-image-3.13.0-63-powerpc-e500mc-dbgsym, linux-tools-3.13.0-63-powerpc-e500mc,
 linux-cloud-tools-3.13.0-63-powerpc-e500mc, linux-udebs-powerpc-e500mc, linux-image-3.13.0-63-powerpc-smp, linux-image-extra-3.13.0-63-powerpc-smp, linux-headers-3.13.0-63-powerpc-smp, linux-image-3.13.0-63-powerpc-smp-dbgsym, linux-tools-3.13.0-63-powerpc-smp, linux-cloud-tools-3.13.0-63-powerpc-smp, linux-udebs-powerpc-smp, kernel-image-3.13.0-63-generic-di, nic-modules-3.13.0-63-generic-di, nic-shared-modules-3.13.0-63-generic-di, serial-modules-3.13.0-63-generic-di, ppp-modules-3.13.0-63-generic-di, pata-modules-3.13.0-63-generic-di, firewire-core-modules-3.13.0-63-generic-di, scsi-modules-3.13.0-63-generic-di, plip-modules-3.13.0-63-generic-di, floppy-modules-3.13.0-63-generic-di, fat-modules-3.13.0-63-generic-di, nfs-modules-3.13.0-63-generic-di, md-modules-3.13.0-63-generic-di, multipath-modules-3.13.0-63-generic-di, usb-modules-3.13.0-63-generic-di, pcmcia-storage-modules-3.13.0-63-generic-di, fb-modules-3.13.0-63-generic-di,
 input-modules-3.13.0-63-generic-di, mouse-modules-3.13.0-63-generic-di, irda-modules-3.13.0-63-generic-di, parport-modules-3.13.0-63-generic-di, nic-pcmcia-modules-3.13.0-63-generic-di, pcmcia-modules-3.13.0-63-generic-di, nic-usb-modules-3.13.0-63-generic-di, sata-modules-3.13.0-63-generic-di, crypto-modules-3.13.0-63-generic-di, squashfs-modules-3.13.0-63-generic-di, speakup-modules-3.13.0-63-generic-di, virtio-modules-3.13.0-63-generic-di, fs-core-modules-3.13.0-63-generic-di, fs-secondary-modules-3.13.0-63-generic-di, storage-core-modules-3.13.0-63-generic-di, block-modules-3.13.0-63-generic-di, message-modules-3.13.0-63-generic-di, vlan-modules-3.13.0-63-generic-di,
 ipmi-modules-3.13.0-63-generic-di
Architecture: all i386 amd64 armhf arm64 x32 powerpc ppc64el
Version: 3.13.0-63.103
Maintainer: Ubuntu Kernel Team <kernel-team@lists.ubuntu.com>
Standards-Version: 3.9.4.0
Vcs-Git: http://kernel.ubuntu.com/git-repos/ubuntu/ubuntu-trusty.git
Build-Depends: debhelper (>= 5), cpio, module-init-tools, kernel-wedge (>= 2.24ubuntu1), makedumpfile [amd64 i386], libelf-dev, libnewt-dev, libiberty-dev, rsync, libdw-dev, libpci-dev, dpkg (>= 1.16.0~ubuntu4), pkg-config, flex, bison, libunwind8-dev, openssl, libaudit-dev, bc, python-dev, gawk, device-tree-compiler [powerpc], u-boot-tools [powerpc], libc6-dev-ppc64 [powerpc]
Build-Depends-Indep: xmlto, docbook-utils, ghostscript, transfig, bzip2, sharutils, asciidoc
Package-List: 
 block-modules-3.13.0-63-generic-di udeb debian-installer standard
 crypto-modules-3.13.0-63-generic-di udeb debian-installer extra
 fat-modules-3.13.0-63-generic-di udeb debian-installer standard
 fb-modules-3.13.0-63-generic-di udeb debian-installer standard
 firewire-core-modules-3.13.0-63-generic-di udeb debian-installer standard
 floppy-modules-3.13.0-63-generic-di udeb debian-installer standard
 fs-core-modules-3.13.0-63-generic-di udeb debian-installer standard
 fs-secondary-modules-3.13.0-63-generic-di udeb debian-installer standard
 input-modules-3.13.0-63-generic-di udeb debian-installer standard
 ipmi-modules-3.13.0-63-generic-di udeb debian-installer standard
 irda-modules-3.13.0-63-generic-di udeb debian-installer standard
 kernel-image-3.13.0-63-generic-di udeb debian-installer extra
 linux-cloud-tools-3.13.0-63 deb devel optional
 linux-cloud-tools-3.13.0-63-generic deb devel optional
 linux-cloud-tools-3.13.0-63-generic-lpae deb devel optional
 linux-cloud-tools-3.13.0-63-lowlatency deb devel optional
 linux-cloud-tools-3.13.0-63-powerpc-e500 deb devel optional
 linux-cloud-tools-3.13.0-63-powerpc-e500mc deb devel optional
 linux-cloud-tools-3.13.0-63-powerpc-smp deb devel optional
 linux-cloud-tools-3.13.0-63-powerpc64-emb deb devel optional
 linux-cloud-tools-3.13.0-63-powerpc64-smp deb devel optional
 linux-cloud-tools-common deb kernel optional
 linux-doc deb doc optional
 linux-headers-3.13.0-63 deb devel optional
 linux-headers-3.13.0-63-generic deb devel optional
 linux-headers-3.13.0-63-generic-lpae deb devel optional
 linux-headers-3.13.0-63-lowlatency deb devel optional
 linux-headers-3.13.0-63-powerpc-e500 deb devel optional
 linux-headers-3.13.0-63-powerpc-e500mc deb devel optional
 linux-headers-3.13.0-63-powerpc-smp deb devel optional
 linux-headers-3.13.0-63-powerpc64-emb deb devel optional
 linux-headers-3.13.0-63-powerpc64-smp deb devel optional
 linux-image-3.13.0-63-generic deb kernel optional
 linux-image-3.13.0-63-generic-dbgsym deb devel optional
 linux-image-3.13.0-63-generic-lpae deb kernel optional
 linux-image-3.13.0-63-generic-lpae-dbgsym deb devel optional
 linux-image-3.13.0-63-lowlatency deb kernel optional
 linux-image-3.13.0-63-lowlatency-dbgsym deb devel optional
 linux-image-3.13.0-63-powerpc-e500 deb kernel optional
 linux-image-3.13.0-63-powerpc-e500-dbgsym deb devel optional
 linux-image-3.13.0-63-powerpc-e500mc deb kernel optional
 linux-image-3.13.0-63-powerpc-e500mc-dbgsym deb devel optional
 linux-image-3.13.0-63-powerpc-smp deb kernel optional
 linux-image-3.13.0-63-powerpc-smp-dbgsym deb devel optional
 linux-image-3.13.0-63-powerpc64-emb deb kernel optional
 linux-image-3.13.0-63-powerpc64-emb-dbgsym deb devel optional
 linux-image-3.13.0-63-powerpc64-smp deb kernel optional
 linux-image-3.13.0-63-powerpc64-smp-dbgsym deb devel optional
 linux-image-extra-3.13.0-63-generic deb kernel optional
 linux-image-extra-3.13.0-63-generic-lpae deb kernel optional
 linux-image-extra-3.13.0-63-lowlatency deb kernel optional
 linux-image-extra-3.13.0-63-powerpc-e500 deb kernel optional
 linux-image-extra-3.13.0-63-powerpc-e500mc deb kernel optional
 linux-image-extra-3.13.0-63-powerpc-smp deb kernel optional
 linux-image-extra-3.13.0-63-powerpc64-emb deb kernel optional
 linux-image-extra-3.13.0-63-powerpc64-smp deb kernel optional
 linux-libc-dev deb devel optional
 linux-source-3.13.0 deb devel optional
 linux-tools-3.13.0-63 deb devel optional
 linux-tools-3.13.0-63-generic deb devel optional
 linux-tools-3.13.0-63-generic-lpae deb devel optional
 linux-tools-3.13.0-63-lowlatency deb devel optional
 linux-tools-3.13.0-63-powerpc-e500 deb devel optional
 linux-tools-3.13.0-63-powerpc-e500mc deb devel optional
 linux-tools-3.13.0-63-powerpc-smp deb devel optional
 linux-tools-3.13.0-63-powerpc64-emb deb devel optional
 linux-tools-3.13.0-63-powerpc64-smp deb devel optional
 linux-tools-common deb kernel optional
 linux-udebs-generic udeb debian-installer optional
 linux-udebs-generic-lpae udeb debian-installer optional
 linux-udebs-lowlatency udeb debian-installer optional
 linux-udebs-powerpc-e500 udeb debian-installer optional
 linux-udebs-powerpc-e500mc udeb debian-installer optional
 linux-udebs-powerpc-smp udeb debian-installer optional
 linux-udebs-powerpc64-emb udeb debian-installer optional
 linux-udebs-powerpc64-smp udeb debian-installer optional
 md-modules-3.13.0-63-generic-di udeb debian-installer standard
 message-modules-3.13.0-63-generic-di udeb debian-installer standard
 mouse-modules-3.13.0-63-generic-di udeb debian-installer extra
 multipath-modules-3.13.0-63-generic-di udeb debian-installer extra
 nfs-modules-3.13.0-63-generic-di udeb debian-installer standard
 nic-modules-3.13.0-63-generic-di udeb debian-installer standard
 nic-pcmcia-modules-3.13.0-63-generic-di udeb debian-installer standard
 nic-shared-modules-3.13.0-63-generic-di udeb debian-installer standard
 nic-usb-modules-3.13.0-63-generic-di udeb debian-installer standard
 parport-modules-3.13.0-63-generic-di udeb debian-installer standard
 pata-modules-3.13.0-63-generic-di udeb debian-installer standard
 pcmcia-modules-3.13.0-63-generic-di udeb debian-installer standard
 pcmcia-storage-modules-3.13.0-63-generic-di udeb debian-installer standard
 plip-modules-3.13.0-63-generic-di udeb debian-installer standard
 ppp-modules-3.13.0-63-generic-di udeb debian-installer standard
 sata-modules-3.13.0-63-generic-di udeb debian-installer standard
 scsi-modules-3.13.0-63-generic-di udeb debian-installer standard
 serial-modules-3.13.0-63-generic-di udeb debian-installer standard
 speakup-modules-3.13.0-63-generic-di udeb debian-installer extra
 squashfs-modules-3.13.0-63-generic-di udeb debian-installer extra
 storage-core-modules-3.13.0-63-generic-di udeb debian-installer standard
 usb-modules-3.13.0-63-generic-di udeb debian-installer standard
 virtio-modules-3.13.0-63-generic-di udeb debian-installer standard
 vlan-modules-3.13.0-63-generic-di udeb debian-installer extra
Checksums-Sha1: 
 769d3e9207f796560b56b363779290a544e2e5cc 116419243 linux_3.13.0.orig.tar.gz
 8cf23a02fca77317d64fe165db82cfe4f41720bd 8904973 linux_3.13.0-63.103.diff.gz
Checksums-Sha256: 
 073d6a589655031564407e349c86a316941fc26ef3444bb73a092b43a48347ec 116419243 linux_3.13.0.orig.tar.gz
 1095d53139cb16e24e424f9d7d19ddbcfaac7e0d9933aa4f31d2101c6440c90a 8904973 linux_3.13.0-63.103.diff.gz
Files: 
 8c85f9d0962f2a9335028e4879b03343 116419243 linux_3.13.0.orig.tar.gz
 4a74590e738c1d0f13882c7635034028 8904973 linux_3.13.0-63.103.diff.gz
Testsuite: autopkgtest

-----BEGIN PGP SIGNATURE-----
Version: GnuPG v1

iQIcBAEBAgAGBQJVzmAjAAoJEAx7WJsQW+f3fwwQAKDXsc39d5czdby8e1rmHLlg
gFTJB5/eekkNt+H2OAtczNLzZ48MOWlGe62oQJV4BBuAfEDcz37VgVgyVoLY9uG/
r/85pHxlR8mqvbaiQ0/wS/bVAb8ZFEfcA01lpxm1l8RX/5OgvnsAlf5JrsMSVXt7
dakBkaiNpF7VPVPvIlnl42NtBjVqL6B8pRTKTHP2Uhj65lz3wACdVV4N/D3pFQkr
t2mvjrpMRZlU5SY5RtnZTFu2G17zviCbpCXOaS9lLUZtDC4YIA/ugKZ2+tv0opUY
2biT5T9bp54CZZm14GwMkFeSZR8ZsrR9aK72PLLQ7yw6OPZR7UqeQO+YAKG7VmRg
q4bx+Wzx6PNO/k9cKJDmPckunVsfttK5j6p4n1NAg5bGoU/KD8NRZWfeYkyLRcGf
FP+WirJ3n/S0KhDOq5J6GunoU4IlZZJ8QXBkyNSbnnM/bW0I1GlXEqSp7lEd6o26
bf4+ub4rDQxjSSsKymeF3xBvaUJtx+qa7ATGiOaFRfgsFQ2/7qro3FCpdlEpQqhI
Y6TScd6gMUQZ+d/CxLxGxOr5Yef4FmKKf7LkP+fqVK5Ul1lASeuXIdRZIlilNaAA
LIHgfZ+u7oMVIno8RAu4KNcyAOrWAAtVhLQv7V5VY6Rb3iovZjSMbEIhKWYd7T3p
aelpQGdaUISA9ka2FoyS
=MpO8
-----END PGP SIGNATURE-----
