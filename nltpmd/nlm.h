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
 */

#ifndef NLM_INCLUDE
#define NLM_INCLUDE

/* NLM macros */
#define NLM_AIK_PKEY_LEN        284
#define NLM_SIG_LEN             256
#define NLM_DIGEST_LEN          20
#define NLM_VALID_LEN           48
#define NLM_PCR_LEN             24
#define NLM_PCR_MASK_LEN        3
#define NLM_NONCE_LEN           20
#define NLM_UCHAR_NUM_PER_LINE  20
#define NLM_QUOTE_LEN           (NLM_VALID_LEN + NLM_SIG_LEN)
#define NLM_MSG_INIT            0
#define NLM_MSG_REQ             1
#define NLM_MSG_REP             2
#define NLM_MSG_AIK             3
#define NLM_MSG_GOODBYE         4

/* Definition for the netlink msgs */
typedef struct _nlmsgt
{
    unsigned char opcode;
    union {
        unsigned char request[NLM_NONCE_LEN + NLM_PCR_MASK_LEN]; 
        unsigned char reply[NLM_QUOTE_LEN]; 
    };
} nlmsgt;

#define NLM_MSG_LEN         sizeof(nlmsgt)
#define NLM_QUEUE_MSG_NUM   100
#define NLM_QUEUE_SIZE      (NLM_MSG_LEN*NLM_QUEUE_MSG_NUM)

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
