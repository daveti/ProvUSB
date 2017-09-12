/*
 * Header file for netlink messaging
 * Dec 12, 2013
 * root@davejingtian.org
 * http://davejingtian.org
 *
 * Update for USB block-level provenance
 * Apr 15, 2014
 * root@davejingtian.org
 *
 * Updated on Aug 18, 2014
 * abdul@cs.uoregon.edu
 *
 * Ported for provd on Aug 25, 2015
 * Reference: drivers/usb/gadget/provusb.h
 * root@davejingtian.org
 */

#ifndef NLM_INCLUDE
#define NLM_INCLUDE

/* NLM macros */
#define PROVUSB_ACTION_READ	0
#define PROVUSB_ACTION_WRITE	1
#define PROVUSB_NETLINK_OP_INIT	0
#define PROVUSB_NETLINK_OP_REP	1
#define PROVUSB_NETLINK_OP_POL	2
#define PROVUSB_NETLINK_OP_BLK	3
#define PROVUSB_NETLINK_OP_TPM	4
#define PROVUSB_NETLINK_OP_BYE	5
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

/* For block security level */
struct provusb_block {
	unsigned int		lba;
	int			sec_level;
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

/* Definition for the netlink msgs */
typedef struct provusb_nlmsg nlmsgt;

#define NLM_UCHAR_NUM_PER_LINE	20
#define NLM_MSG_LEN		sizeof(nlmsgt)
#define NLM_QUEUE_MSG_NUM	1024
#define NLM_QUEUE_SIZE		(NLM_MSG_LEN*NLM_QUEUE_MSG_NUM)

/* NLM protocol related methods */

/* Display the nlmsgt msg */
void nlm_display_msg(nlmsgt *msg);

/* Display the uchar given length */
void nlm_display_uchar(unsigned char *src, int len, char *header);

/* Convert bit mask to byte mask */
void bit_mask_to_byte(unsigned char *byte_mask, unsigned char *bit_mask, int mask_len);

/* NLM queue related methods */

/* Init the NLM queue */
void nlm_init_queue(void);

/* Add msgs into the NLM queue from raw binary data */
int nlm_add_msg_queue(nlmsgt *msg);

/* Clear all the msgs in the queue */
void nlm_clear_all_msg_queue(void);

/* Get the number of msgs in the queue */
int nlm_get_msg_num_queue(void);

/* Get the msg based on index */
nlmsgt * nlm_get_msg_queue(int index);

#endif
