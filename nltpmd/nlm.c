/*
 * Source file for protocol NLM
 * Used by the nltpmd to save the raw packet from the kernel
 * Jan 8, 2014
 * root@davejingtian.org
 * http://davejingtian.org
 *
 * Apr 15, 2014
 * Update for USB provenance
 * root@davejingtian.org
 *
 * Updated on Aug 18, 2014
 * abdul@cs.uoregon.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nlm.h"

/* NLM queue definitions */
static nlmsgt nlm_queue[NLM_QUEUE_MSG_NUM];
static int nlm_queue_index; /* Always pointing to the next avalible position */

/* NLM protocol related methods */

/* Display the uchar given length */
void nlm_display_uchar(unsigned char *src, int len, char *header)
{
    int i;
    printf("%s\n", header);
    for (i = 0; i < len; i++)
    {
        if ((i+1) % NLM_UCHAR_NUM_PER_LINE != 0)
            printf("%02x ", src[i]);
        else
            printf("%02x\n", src[i]);
    }
    printf("\n");   
}

/* Convert bit mask to byte mask */
void bit_mask_to_byte(unsigned char *byte_mask, unsigned char *bit_mask, int mask_len) {
    int i, j, k = 0;
    for (i = 0; i < mask_len; i++) 
        for(j = 7; j >= 0; j--) 
            byte_mask[k++] = (bit_mask[i] >> j) & (0x1);
}

/* Display the NLM message */
void nlm_display_msg(nlmsgt *msg)
{
    printf("NLM msg opcode: [%d]\n", msg->opcode);
    if (msg->opcode == NLM_MSG_REQ)
    {
        nlm_display_uchar((msg->request+NLM_NONCE_LEN), NLM_PCR_MASK_LEN, "PCR Mask:");
        nlm_display_uchar(msg->request, NLM_NONCE_LEN, "Nonce:");
    }
    else if (msg->opcode == NLM_MSG_REP)
        nlm_display_uchar(msg->reply, NLM_QUOTE_LEN, "Quote:");
    else if (msg->opcode == NLM_MSG_INIT)
        printf("Init\n");
    else if (msg->opcode == NLM_MSG_GOODBYE)
        printf("Goodbye\n");
    else
        printf("nlm_display_msg - Error: unsupported opcode\n");
}

/* NLM queue related methods */

/* Init the NLM queue */
void nlm_init_queue(void)
{
    memset((unsigned char *)nlm_queue, 0, NLM_QUEUE_SIZE);
    nlm_queue_index = 0;
}

/* Add msgs into the NLM queue from raw binary data */
int nlm_add_msg_queue(nlmsgt *msg)
{
    /* Save the TLV into nlm msg queue */
    if (nlm_queue_index < NLM_QUEUE_MSG_NUM)
        nlm_queue[nlm_queue_index++] = *msg;
    else
    {
        printf("nlm_add_raw_msg_queue: Error - nlm queue is full\n");
        return -1;
    }
    return 0;
}

/* Clear all the msgs in the queue */
void nlm_clear_all_msg_queue(void)
{
    int i;

    /* Zero out the structs */
    for (i = 0; i < nlm_queue_index; i++)
        memset(&nlm_queue[i], 0, sizeof(nlmsgt));
        //free(&nlm_queue[i]);

    /* Reinit the index */
    nlm_queue_index = 0;
}

/* Get the number of msgs in the queue */
int nlm_get_msg_num_queue(void)
{
    return nlm_queue_index;
}

/* Get the msg from the queue based on the index */
nlmsgt * nlm_get_msg_queue(int index)
{
    return &(nlm_queue[index]);
}

