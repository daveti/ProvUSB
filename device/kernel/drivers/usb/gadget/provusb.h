/*
 * ProvUSB.h
 * Header file for ProvUSB
 * Definitions for the provenance data for blocks
 * Aug 24, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 *
 * Added simple policy support
 * Aug 26, 2015
 * daveti
 */
#include <linux/list.h>

#define PROVUSB_ACTION_READ	0
#define PROVUSB_ACTION_WRITE	1
#define PROVUSB_NETLINK_OP_INIT	0
#define PROVUSB_NETLINK_OP_REP	1
#define PROVUSB_NETLINK_OP_POL	2
#define PROVUSB_NETLINK_OP_BLK	3
#define PROVUSB_NETLINK_OP_TPM	4
#define PROVUSB_POLICY_SEC_LEVEL_HIGH	0
#define PROVUSB_POLICY_SEC_LEVEL_MED	1
#define PROVUSB_POLICY_SEC_LEVEL_LOW	2
#define PROVUSB_POLICY_BLACKLIST_NUM	16

/* For provenance logging */
struct provusb_report {
	int			id;
	int			act;
	unsigned int		lba;
	unsigned long long	offset;
	unsigned int		amount;	/* Should be times of 512 */
	/* Leave the time stamp to the user space */
};

/* For simple policy control */
struct provusb_policy {
	int			id;
	int			sec_level;
};

struct provusb_policy_entry {
	struct provusb_policy	policy;
	struct list_head	list;
};

/* For block security level */
struct provusb_block {
	unsigned int		lba;
	int			sec_level;
};

struct provusb_block_entry {
	struct provusb_block	block;
	struct list_head	list;
};

/* For general netlink msg */
struct provusb_nlmsg {
	int			opcode;
	union {
		struct provusb_report	report;
		struct provusb_policy	policy;
		struct provusb_block	block;
		/* TPM management should be added here */
	};
};

/* Blacklist for block policy control */
unsigned int blk_pol_bl[PROVUSB_POLICY_BLACKLIST_NUM] = {
	0,		/* 0 */
	1,		/* 1 */
	2,		/* 2 */
	2505,		/* 3 */
	0xffffffff,	/* 4 */
	0xffffffff,	/* 5 */
	0xffffffff,	/* 6 */
	0xffffffff,	/* 7 */
	0xffffffff,	/* 8 */
	0xffffffff,	/* 9 */
	0xffffffff,	/* 10 */
	0xffffffff,	/* 11 */
	0xffffffff,	/* 12 */
	0xffffffff,	/* 13 */
	0xffffffff,	/* 14 */
	0xffffffff	/* 15 */
};

#define PROVUSB_INIT_RESPONSE(r)		\
	do {					\
		r.id = 7;			\
		r.act = 7;			\
		r.lba = 7;			\
		r.offset = 7;			\
		r.amount = 7;			\
	} while (0);
