Host changes #10

Based on Ubuntu 14.04

Last update: Jan 23, 2015

Added:
1. .config
2. kernel_src dir
3. best changes so far

NOTE:
1. _ut_ = 2 (UT mode between host USB and device)
2. _adsc_debug = 1 (ADSC debug requests will be sent)
3. URB timeout issue has been fixed from device side
4. ADSC debug support with packet size 30/32/64/128/256
5. support for the TPM AIK pub key authentication
6. trigger TPM quote even if AIK auth is requested
7. double 128-byte and 256-byte ADSC debug sending
8. _ut_ = 0 (real testing!)
9. finer granuality of lock in sending quote/AIK
10. replace all print_hex_dump_bytes with print_hex_dump using log level info
11. more quote/AIK dumping among recving and sending
12. debugging code in transport.c
13. sending AIK using ADSC debug
14. bug fix the host kernel frozen
15. MxPS = 64
16. MxPS = 256
17. new ADSC sending API
18. using stack buffer to avoid DMA mapping issue - sending garbage
19. added goodbye msg support for tpmd

root@arpsec03:~/git/ubp/host10# uname -a
Linux arpsec03 3.13.0-32-generic #57 SMP Wed Aug 6 20:07:29 PDT 2014 x86_64 x86_64 x86_64 GNU/Linux

Aug 18, 2014
root@davejingtian.org
http://davejingtian.org
