Device changes

Based on Yocto Daisy 1.6.1 and Kernel 3.5 Overo recipe

Last udpate: Jan 19, 2016

NEW:
1. Kernel RSA (krsa) implementation
2. Crypto patch to make it build
3. MPI patch for missing functions to make it build
4. f_mass_storage patch to use krsa API
5. A lot of patches to MPI, crypto API, shash and krsa to make it IRQ callable!
6. A lot of refactoring to make it work finally - God bless us!


TODO:
1. Crypto patch to make it usable in IRQ context - DONE
2. MPI patch to make it usable in IRQ context - DONE
3. f_mass_storage patch to use krsa API - DONE


NOTE:
1. _ut is true
2. _debug is true
3. Crypto is NOT implemented yet
4. A completion callback for ADSC request using OUT transfer
5. fsg_trusted_dev state update in fsg_main_thread
6. ADSC debug request with 256 bytes
7. TPM AIK pub key verification code support
8. New kernel module parameters: key_file and force_fail
	Assume cmd like: modprobe g_mass_storage file=/home/root/daveti.img key_file=/home/root/aik_pub.key force_fail=false
9. load_key_file() bug fix and printk fix
10. little endian bug fix and added config files to ease the device
11. more debugging once AIK failure
12. bug fix for le16_to_cpu calling
13. support for double 256-byte ADSC debug
14. forcing success internally
15. bug fix for the endian issue
16. debugging code to find the right byte-order kernel API
17. use be16_to_cpu as the byte-order call
18. disable byte-order debugging
19. handle AIK sending in the ADSC debug from the host side
20. support MxPS = 64
21. back to MxPS = 256
22. #ifdef USB_ADSC_FRAGMENT
23. TPM quote/AIK recving without fragmentation
23. Global skb for provusb_nl_send
24. Back to skb dynamic allocation
25. Workaround provd logging using kernel logging

Jan 6, 2015
root@davejingtian.org
http://davejingtian.org
