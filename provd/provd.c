/*
 * provd.c
 * Source file for provd
 * provd (provenance daemon) is a netlink server used to recv the
 * IP packet from the ProvUSB kernel, implementing:
 * TPM information management (TIM)
 * Block/Policy control (BIBA)
 * Provenance logging
 *
 * Aug 30, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include "nlm.h"
#include "provmem.h"

/* Global defs */
#define PROVD_NETLINK		31
#define PROVD_RECV_BUFF_LEN	1024*1024
#define PROVD_PROV_LOG_PATH	"./log/provenance.log"
#define PROVD_BLK_DATA_PATH	"./blk/block.dat"

/* Global variables */
extern char *optarg;
static struct sockaddr_nl provd_nl_addr;
static struct sockaddr_nl provd_nl_dest_addr;
static pid_t provd_pid;
static int provd_sock_fd;
static void *recv_buff;
static int debug_enabled;
static int save_storage;
static FILE *provd_log_fp;
static FILE *provd_blk_fp;

/* Signal term handler */
static void provd_signal_term(int signal)
{
    /* Close opened files */
    if (provd_log_fp) {
	fflush(provd_log_fp);
	fclose(provd_log_fp);
    }
    if (provd_blk_fp) {
	fflush(provd_blk_fp);
	fclose(provd_blk_fp);
    }

    /* Close the socket */
    if (provd_sock_fd != -1)    
        close(provd_sock_fd);

    /* Free netlink receive buffer */
    if (recv_buff != NULL)
        free(recv_buff);

    exit(EXIT_SUCCESS);
}

/* Setup signal handler */
static int signals_init(void)
{
    int rc;
    sigset_t sigmask;
    struct sigaction sa;

    sigemptyset(&sigmask);
    if ((rc = sigaddset(&sigmask, SIGTERM)) || (rc = sigaddset(&sigmask, SIGINT))) {
        printf("provd - Error: sigaddset [%s]\n", strerror(errno));
        return -1;
    }
    sa.sa_flags = 0;
    sa.sa_mask = sigmask;
    sa.sa_handler = provd_signal_term;
    if ((rc = sigaction(SIGTERM, &sa, NULL)) || (rc = sigaction(SIGINT, &sa, NULL))) {
        printf("provd - Error: signal SIGTERM or SIGINT not registered [%s]\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* Send the nlmsgt via the netlink socket */
static int provd_netlink_send(nlmsgt *msg_ptr)
{
    struct nlmsghdr *nlh;
    struct iovec iov;
    struct msghdr msg;
    int rtn;
    unsigned char *data;
    int data_len;

    // Convert the nlmsgt into binary data
    data_len = NLM_MSG_LEN;
    data = (unsigned char *)malloc(data_len);
    memcpy(data, msg_ptr, NLM_MSG_LEN);

    // Init the stack struct to avoid potential error
    memset(&iov, 0, sizeof(iov));
    memset(&msg, 0, sizeof(msg));

    // Create the netlink msg
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(data_len));
    memset(nlh, 0, NLMSG_SPACE(data_len));
    nlh->nlmsg_len = NLMSG_SPACE(data_len);
    nlh->nlmsg_pid = provd_pid;
    nlh->nlmsg_flags = 0;

    // Copy the binary data into the netlink message
    memcpy(NLMSG_DATA(nlh), data, data_len);

    // Nothing to do for test msg - it is already what it is
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&provd_nl_dest_addr;
    msg.msg_namelen = sizeof(provd_nl_dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // Send the msg to the kernel
    rtn = sendmsg(provd_sock_fd, &msg, 0);
    if (rtn == -1) {
        printf("provd_netlink_send: Error on sending netlink msg to the kernel [%s]\n",
                strerror(errno));
        free(nlh);
        free(data);
        return rtn;
    }
    if (debug_enabled)
        printf("provd_netlink_send: Info - send netlink msg to the kernel\n");

    free(nlh);
    free(data);
    return 0;
}

/* Init the netlink with initial nlmsg */
static int provd_init_netlink(void)
{
    int result;
    nlmsgt init_msg;

    // Add the opcode into the msg
    memset(&init_msg, 0, sizeof(nlmsgt));
    init_msg.opcode = PROVUSB_NETLINK_OP_INIT;

    // Send the msg to the kernel
    printf("provd_init_netlink: Info - send netlink init msg to the kernel\n");
    result = provd_netlink_send(&init_msg);
    if (result)
        printf("provd_init_netlink: Error on sending netlink init msg to the kernel [%s]\n",
                strerror(errno));

    return result;
}

/* Init the provenance/blk logging */
static int provd_init_log(void)
{
    /* Open the provenance log */
    provd_log_fp = fopen(PROVD_PROV_LOG_PATH, "a+");
    if (!provd_log_fp) {
	printf("%s: fopen failed for file %s with error %s\n",
		__func__, PROVD_PROV_LOG_PATH, strerror(errno));
	return -1;
    }

    /* Open the block file */
    provd_blk_fp = fopen(PROVD_BLK_DATA_PATH, "a+b");
    if (!provd_blk_fp) {
	printf("%s: fopen failed for file %s with error %s\n",
		__func__, PROVD_BLK_DATA_PATH, strerror(errno));
	return -1;
    }

    return 0;
}

/* Sync the block */
static int provd_sync_blk(void)
{
    int ret;
    nlmsgt msg;
    struct provusb_block blk;

    /* Set the opcode */
    msg.opcode = PROVUSB_NETLINK_OP_BLK;

    /* Read-in the saved blocks */
    while (!feof(provd_blk_fp)) {
	ret = fread(&blk, sizeof(blk), 1, provd_blk_fp);
	if (ret == 0) {
		/* Done */
		return 0;
	}
	else if (ret != 1) {
		printf("%s: fread failed with error %s\n",
			__func__, strerror(errno));
		return -1;
	}
	/* Sync this block with the kernel */
	memcpy(&msg.block, &blk, sizeof(blk));
	ret = provd_netlink_send(&msg);
	if (ret) {
		printf("%s: provd_netlink_send failed with error %s\n",
			__func__, strerror(errno));
		return -1;
	}
    }

    return 0;	
}

/* Log the block */
static int provd_log_blk(struct provusb_block *blk)
{
    int ret;

    if (!blk) {
	printf("%s: NULL block\n", __func__);
	return -1;
    }

    ret = fwrite(blk, sizeof(*blk), 1, provd_blk_fp);
    if (ret != 1) {
	printf("%s: fwrite failed with error %s\n",
		__func__, strerror(errno));
	return -1;
    }

    return 0;
}

/* Log the provenance */
static int provd_log_prov(struct provusb_report *rep)
{
    if (!rep) {
	printf("%s: NULL report\n", __func__);
	return -1;
    }

    /* To save some space, try to make the log as CSV
     * and use raw time rather than formated timestamp
     */
    fprintf(provd_log_fp, "%ld,%d,%d,%u,%llu,%u\n",
		time(NULL),
		rep->id, rep->act, rep->lba, rep->offset, rep->amount);

    return 0;
}

/* Process the netlink msg */
static void provd_netlink_process(nlmsgt *msg)
{
    if (!msg) {
	printf("%s: NULL msg\n", __func__);
	return;
    }

    switch (msg->opcode) {

    case PROVUSB_NETLINK_OP_REP:
	if (save_storage)
		provmem_update_prov(&msg->report);
	else
		provd_log_prov(&msg->report);
	break;

    case PROVUSB_NETLINK_OP_BLK:
	provd_log_blk(&msg->block);
	break;

    case PROVUSB_NETLINK_OP_POL:
    case PROVUSB_NETLINK_OP_TPM:
	printf("%s: to be supported\n", __func__);
	break;

    default:
	printf("%s: unknown opcode %d\n", __func__, msg->opcode);
	break;
    }
}


static void usage(void)
{
    fprintf(stderr, "\tusage: provd [-s] [-d] [-c <config file> [-h]\n\n");
    fprintf(stderr, "\t-s|--save\tsave the provenance logging storage (for self-powered devices)\n");
    fprintf(stderr, "\t-d|--debug\tenable debug mode\n");
    fprintf(stderr, "\t-c|--config\tpath to configuration file (TBD)\n");
    fprintf(stderr, "\t-h|--help\tdisplay this help message\n");
    fprintf(stderr, "\n");
}

int main(int argc, char **argv)
{
    int result;
    int c, option_index = 0;
    int recv_size;
    int num_of_msg;
    int i;
    nlmsgt *msg_ptr;
    struct option long_options[] = {
        {"help", 0, NULL, 'h'},
        {"save", 0, NULL, 's'},
        {"debug", 0, NULL, 'd'},
        {"config", 1, NULL, 'c'},
        {0, 0, 0, 0}
    };

    /* Process the arguments */
    while ((c = getopt_long(argc, argv, "shdc:", long_options, &option_index)) != -1) {
        switch (c) {
            case 's':
                printf("provd - Info: save the provenance storage\n");
                save_storage = 1;
                break;
            case 'd':
                printf("provd - Info: debug mode enabled\n");
                debug_enabled = 1;
                break;
            case 'c':
                printf("provd - Warning: may support in future\n");
                break;
            case 'h':
                /* fall through */
            default:
                usage();
                return -1;
        }
    }

    /* Set the signal handlers */
    if (signals_init() != 0) {
        printf("provd - Error: failed to set up the signal handlers\n");
        return -1;
    }

    /* Init the NLM queue */
    nlm_init_queue();

    /* Init the logging */
    result = provd_init_log();
    if (result) {
	printf("provd - Error: provd_init_log failed\n");
	return -1;
    }

    /* Init provmem */
    if (save_storage)
	provmem_init(provd_log_fp, provd_blk_fp);

    do{
        /* Create the netlink socket */
        printf("provd - Info: waiting for a new connection\n");
        while ((provd_sock_fd = socket(PF_NETLINK, SOCK_RAW, PROVD_NETLINK)) < 0);

        /* Bind the socket */
        memset(&provd_nl_addr, 0, sizeof(provd_nl_addr));
        provd_nl_addr.nl_family = AF_NETLINK;
        provd_pid = getpid();
        printf("provd - Info: pid [%u]\n", provd_pid);
        provd_nl_addr.nl_pid = provd_pid;
        if (bind(provd_sock_fd, (struct sockaddr*)&provd_nl_addr, sizeof(provd_nl_addr)) == -1) {
            printf("provd - Error: netlink bind failed [%s], aborting\n", strerror(errno));
            return -1;
        }

        /* Setup the netlink destination socket address */
        memset(&provd_nl_dest_addr, 0, sizeof(provd_nl_dest_addr));
        provd_nl_dest_addr.nl_family = AF_NETLINK;
        provd_nl_dest_addr.nl_pid = 0;
        provd_nl_dest_addr.nl_groups = 0;
        printf("provd - Info: provd netlink socket init done\n");

        /* Prepare the recv buffer */
        recv_buff = calloc(1, PROVD_RECV_BUFF_LEN);
        struct iovec iov = { recv_buff, PROVD_RECV_BUFF_LEN };
        struct nlmsghdr *nh;
        struct nlmsgerr *nlm_err_ptr;
        struct msghdr msg = { &provd_nl_dest_addr,
            sizeof(provd_nl_dest_addr),
            &iov, 1, NULL, 0, 0 };

        /* Send the initial testing nlmsgt to the kernel module */
        result = provd_init_netlink();
        if (result != 0) {
            printf("provd - Error: provd_init_netlink failed\n");
            return -1;
        }

        /* Retrive the data from the kernel */
        recv_size = recvmsg(provd_sock_fd, &msg, 0);
        printf("provd - Info: got netlink init msg response from the kernel:\n");
	nlm_display_msg((nlmsgt *)(NLMSG_DATA(recv_buff)));

	/* Sync all blocks */
	printf("provd - Info: sync'ing all the blocks\n");
	result = provd_sync_blk();
	if (result) {
	    printf("provd - Error: provd_sync_blk failed\n");
	    return -1;
	}

        printf("provd - Info: provd is up and running\n");
        do {
            /* Recv the msg from the kernel */
	    if (debug_enabled)
            	printf("provd - Info: waiting for a new request\n");
            recv_size = recvmsg(provd_sock_fd, &msg, 0);
            if (recv_size == -1) {
                printf("provd - Error: recv failed [%s]\n", strerror(errno));
                continue;
            }
            else if (recv_size == 0) {
                printf("provd - Warning: kernel netlink socket is closed\n");
                continue;
            }
	    if (debug_enabled)
            	printf("provd - Info: received a new msg\n");

            /* Pop nlmsgs into the NLM queue
             * Note that we do not allow multipart msg from the kernel.
             * So we do not have to call NLMSG_NEXT() and only one msg
             * would be recv'd for each recvmsg call. NLM queue seems
             * to be redundant if provd is single thread. But it is
             * needed if TPM pkt signing is the other thread.
             * Feb 24, 2014
             * daveti
             */
            nh = (struct nlmsghdr *)recv_buff;
            if (NLMSG_OK(nh, recv_size)) {
                /* Make sure the msg is alright */
                if (nh->nlmsg_type == NLMSG_ERROR) {
                    nlm_err_ptr = (struct nlmsgerr *)(NLMSG_DATA(nh));
                    printf("provd - Error: nlmsg error [%d]\n",
                            nlm_err_ptr->error);
                    continue;
                }

                /* Ignore the noop */
                if (nh->nlmsg_type == NLMSG_NOOP)
                    continue;

                /* Defensive checking - should always be non-multipart msg */
                if (nh->nlmsg_type != NLMSG_DONE) {
                    printf("provd - Error: nlmsg type [%d] is not supported\n",
                            nh->nlmsg_type);
                    continue;
                }

                /* Berak if received goodbye msg */
                if (((nlmsgt *)NLMSG_DATA(nh))->opcode == PROVUSB_NETLINK_OP_BYE) {
                    printf("provd - Info: got goodbye msg\n");
                    if (debug_enabled)
                        nlm_display_msg(NLMSG_DATA(nh));
                    break;
                }

                /* Pop the msg into the NLM queue */
                if (nlm_add_msg_queue(NLMSG_DATA(nh)) != 0) {
                    printf("provd - Error: nlm_add_raw_msg_queue failed\n");
                    continue;
                }
            } else {
                printf("provd - Error: netlink msg is corrupted\n");
                continue;
            }

            /* NOTE: even if nlm_add_raw_msg_queue may fail, there may be msgs in queue
             * Right now, provd is single thread - recving msgs from the kernel space
             * and then processing each msg upon this recving. However, the code below
             * could be separated into a worker thread which could run parallelly with
             * the main thread. This may be an option to improve the performance even
             * the mutex has to be added into NLM queue implementation...
             * Feb 24, 2014
             * daveti
             */

            /* Go thru the queue */
            num_of_msg = nlm_get_msg_num_queue();
            if (debug_enabled)
                printf("provd - Debug: got [%d] msgs(packets) in the queue\n", num_of_msg);

            for (i = 0; i < num_of_msg; i++) {
                /* Get the nlmsgt msg */
                msg_ptr = nlm_get_msg_queue(i);

                /* Debug */
                if (debug_enabled)
                    nlm_display_msg(msg_ptr);

                /* Process the request */
		provd_netlink_process(msg_ptr);
            }

            /* Clear the queue before receiving again */
            nlm_clear_all_msg_queue();

	    /* Flush */
	    fflush(provd_log_fp);
	    fflush(provd_blk_fp);

        } while (1);

        /* Clean up before next connection */
        printf("provd - Info: closing the current connection\n");
        nlm_clear_all_msg_queue();
        close(provd_sock_fd);
	fflush(provd_log_fp);
	fflush(provd_blk_fp);
	fclose(provd_log_fp);
	fclose(provd_blk_fp);
        provd_sock_fd = -1;
        free(recv_buff);
        recv_buff = NULL;

    } while (1);

    /* To close correctly, we must receive a SIGTERM */
    return 0;
}

