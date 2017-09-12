/*
 * nltpmd_kernel_mod.c
 * nltpmd kernel module using netlink
 * Kernel version: 2.6.X
 * NOTE: the way to create netlink socket is different in the kernel 3.9.X!
 * Comment the LATEST_KERNEL define would force the kernel 2.6.X settings!
 * Jan 7, 2014
 * daveti@cs.uoregon.edu
 * http://daveti.blog.com
 *
 * Updated on Aug 18, 2014
 * abdul@cs.uoregon.edu
 */

#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/delay.h>

#define LATEST_KERNEL       1
#define NLTPMD_NETLINK      31

/* NLM macros */
#define NLM_AIK_PKEY_LEN    284
#define NLM_SIG_LEN         256
#define NLM_DIGEST_LEN      20
#define NLM_VALID_LEN       48
#define NLM_PCR_LEN         3
#define NLM_NONCE_LEN       20
#define NLM_QUOTE_LEN       (NLM_VALID_LEN + NLM_SIG_LEN)
#define NLM_MSG_INIT        0
#define NLM_MSG_REQ         1
#define NLM_MSG_REP         2
#define NLM_MSG_AIK         3
#define NLM_MSG_GOODBYE     4

/* Definition for the netlink msgs */
typedef struct _nlmsgt
{
    unsigned char opcode;
    union {
        unsigned char request[NLM_NONCE_LEN + NLM_PCR_LEN];
        unsigned char reply[NLM_QUOTE_LEN];
    };
} nlmsgt;

#define NUM_MSGS    100

struct sock *nl_sk = NULL;
static int num_msgs = 0;
static int num_connections = 10;

static void nltpmd_nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    int pkt_size;
    char *msg="hello_from_nltpmd_kernel_module";
    int res;
    unsigned char pcr[NLM_PCR_LEN];
    unsigned char nonce[NLM_NONCE_LEN];
    nlmsgt nlmsg;
    nlmsgt *msg_ptr;

    printk(KERN_INFO "nltpmd: entering: %s\n", __FUNCTION__);

    nlh = (struct nlmsghdr*)skb->data;
    pid = nlh->nlmsg_pid; /*pid of sending process */
    msg_ptr = nlmsg_data(nlh);

    printk(KERN_INFO "nltpmd: netlink received msg opcode: [%u], from pid: [%u]\n", msg_ptr->opcode, pid);

    if (msg_ptr->opcode == NLM_MSG_INIT)
    {
        printk(KERN_INFO "nltpmd: received init msg: [%s]\n", msg_ptr->reply);

        /* Send the msg from kernel to the user */
        printk(KERN_INFO "nltpmd: sending init msg: [%s]\n", msg);

        memset(&nlmsg, 0, sizeof(nlmsgt));
        nlmsg.opcode = NLM_MSG_INIT;
        memcpy(nlmsg.request, msg, strlen(msg)+1);

        pkt_size = sizeof(nlmsgt);
        skb_out = nlmsg_new(pkt_size, 0);
        if (!skb_out) 
        {
            printk(KERN_ERR "nltpmd: failed to allocate new skb\n");
            return;
        }
        nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, pkt_size, 0);  
        NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */

        /* Copy the nlmsg */
        memcpy(nlmsg_data(nlh), &nlmsg, pkt_size);

        /* Send back to user space */
        res = nlmsg_unicast(nl_sk, skb_out, pid);
        if (res)
            printk(KERN_INFO "nltpmd: error while sending back to user\n");
    }

    if (msg_ptr->opcode == NLM_MSG_AIK)
    {
        printk(KERN_INFO "nltpmd: received AIK Public Key:\n");
        print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 2, (u8 *)msg_ptr->reply, NLM_AIK_PKEY_LEN, 0);
        msleep(10);
    }

    if (msg_ptr->opcode == NLM_MSG_REP)
    {
        printk(KERN_INFO "nltpmd: received reply opcode: [%u]\n", msg_ptr->opcode);
        print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 2, (u8 *)msg_ptr->reply, NLM_QUOTE_LEN, 0);
    }

    if (num_msgs++ <= NUM_MSGS) 
    {
        /* Send a random nonce */
        get_random_bytes(pcr, NLM_PCR_LEN);
        get_random_bytes(nonce, NLM_NONCE_LEN);

        memset(&nlmsg, 0, sizeof(nlmsgt));
        if (num_msgs == NUM_MSGS+1)
            nlmsg.opcode = NLM_MSG_GOODBYE;
        else
        {
            nlmsg.opcode = NLM_MSG_REQ;
            memcpy(nlmsg.request, nonce, NLM_NONCE_LEN);
            memcpy(nlmsg.request+NLM_NONCE_LEN, pcr, NLM_PCR_LEN);
        }
        pkt_size = sizeof(nlmsgt);
        skb_out = nlmsg_new(pkt_size, 0);
        if (!skb_out) 
        {
            printk(KERN_ERR "nltpmd: failed to allocate new skb\n");
            return;
        }
        nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, pkt_size, 0);  
        NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */

        /* Copy the nlmsg */
        memcpy(nlmsg_data(nlh), &nlmsg, pkt_size);

        /* Send back to user space */
        printk(KERN_INFO "nltpmd: sending request opcode: [%u]\n", nlmsg.opcode);
        if (nlmsg.opcode == NLM_MSG_REQ)
        {
            printk(KERN_INFO "nltpmd: PCR mask:\n");
            print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 2, (u8 *)pcr, NLM_PCR_LEN, 0);
            printk(KERN_INFO "nltpmd: Nonce:\n");
            print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 2, (u8 *)nonce, NLM_NONCE_LEN, 0);
        }
        else if (nlmsg.opcode == NLM_MSG_GOODBYE)
            printk(KERN_INFO "nltpmd: GOODBYE!\n");

        res = nlmsg_unicast(nl_sk, skb_out, pid);
        if (res)
            printk(KERN_INFO "nltpmd: error while sending back to user\n");
    }
    else
    {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
        if (--num_connections > 0) 
        {
            struct netlink_kernel_cfg cfg = {
                .input = nltpmd_nl_recv_msg,
            };
            nl_sk = netlink_kernel_create(&init_net, NLTPMD_NETLINK, &cfg);
            num_msgs = 0;
        }
        else
        {
            struct task_struct *task;
            struct pid *pid_struct;
            pid_struct = find_get_pid(pid);
            task = pid_task(pid_struct,PIDTYPE_PID);
            send_sig_info(SIGTERM, (struct siginfo *)1, task);
        }
    }
}

static int __init nltpmd_kmod_init(void)
{
#ifndef LATEST_KERNEL
    nl_sk = netlink_kernel_create(&init_net, NLTPMD_NETLINK, 0, nltpmd_nl_recv_msg,
            NULL, THIS_MODULE);
#else
    struct netlink_kernel_cfg cfg = {
        .input = nltpmd_nl_recv_msg,
    };
    nl_sk = netlink_kernel_create(&init_net, NLTPMD_NETLINK, &cfg);
#endif

    printk("entering: %s\n", __FUNCTION__);

    if(!nl_sk) 
    {
        printk(KERN_ALERT "nltpmd: error creating socket.\n");
        return -10;
    }

    return 0;
}

static void __exit nltpmd_kmod_exit(void)
{
    printk(KERN_INFO "exiting nltpmd kernel module\n");
    if(nl_sk) 
        netlink_kernel_release(nl_sk);
}

module_init(nltpmd_kmod_init);
module_exit(nltpmd_kmod_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("nltpmd kernel module");
MODULE_AUTHOR("daveti");
