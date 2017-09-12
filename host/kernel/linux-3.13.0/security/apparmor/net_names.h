static const char *address_family_names[] = {
[0] = "unspec",
[1] = "unix",
[2] = "inet",
[3] = "ax25",
[4] = "ipx",
[5] = "appletalk",
[6] = "netrom",
[7] = "bridge",
[8] = "atmpvc",
[9] = "x25",
[10] = "inet6",
[11] = "rose",
[13] = "netbeui",
[14] = "security",
[15] = "key",
[16] = "netlink",
[17] = "packet",
[18] = "ash",
[19] = "econet",
[20] = "atmsvc",
[21] = "rds",
[22] = "sna",
[23] = "irda",
[24] = "pppox",
[25] = "wanpipe",
[26] = "llc",
[27] = "ib",
[29] = "can",
[30] = "tipc",
[31] = "bluetooth",
[32] = "iucv",
[33] = "rxrpc",
[34] = "isdn",
[35] = "phonet",
[36] = "ieee802154",
[37] = "caif",
[38] = "alg",
[39] = "nfc",
[40] = "vsock",
};
#define AA_FS_AF_MASK "unspec unix local inet ax25 ipx appletalk netrom bridge atmpvc x25 inet6 rose netbeui security key netlink packet ash econet atmsvc rds sna irda pppox wanpipe llc ib can tipc bluetooth iucv rxrpc isdn phonet ieee802154 caif alg nfc vsock max"
static const char *sock_type_names[] = {
[1] = "stream",
[2] = "dgram",
[3] = "raw",
[4] = "rdm",
[5] = "seqpacket",
[6] = "dccp",
[10] = "packet",
};
