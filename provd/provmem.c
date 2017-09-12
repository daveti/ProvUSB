/*
 * provmem.c
 * Implementation for provenance memory management
 * Aug 31, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "provmem.h"

/* Global vars */
static FILE *blk_ctrl_fp;
static FILE *prov_ctrl_fp;
static struct provmem_block *blk_ctrl;
static struct provmem_prov *prov_ctrl;

/* Helpers */
static struct provmem_prov *get_prov_ctrl(unsigned int lba)
{
	struct provmem_prov *ctrl;

	ctrl = prov_ctrl;
	while (ctrl) {
		if (ctrl->act == -1)
			break;
		if (ctrl->lba == lba)
			return ctrl;
		ctrl = ctrl->next;
	}

	return NULL;
}

static struct provmem_prov *get_prov_ctrl_avail(void)
{
	struct provmem_prov *prev;
	struct provmem_prov *ctrl;

	/* Go and find the last */
	ctrl = prov_ctrl;
	while (ctrl) {
		if (ctrl->act == -1)
			return ctrl;
		prev = ctrl;
		ctrl = ctrl->next;
	}

	/* Alloc a new ctrl */
	prev->next = malloc(sizeof(*prev));
	if (!prev->next) {
		printf("provmem - Error: malloc failed in %s\n", __func__);
		return NULL;
	}
	memset(prev->next, 0x0, sizeof(*prev));

	return prev->next;
}

static void update_log_prov_ctrl(struct provmem_prov *ctrl, struct provusb_report *rep)
{
	/* Update the ctrl */
	ctrl->lba = rep->lba;	/* for new ctrl */
	ctrl->act = rep->act;
	ctrl->id = rep->id;	/* for new ctrl */
	ctrl->next = NULL;

	/* Log the event */
	fprintf(prov_ctrl_fp, "%ld,%d,%d,%u,%llu,%u\n",
		time(NULL),
		rep->id, rep->act, rep->lba, rep->offset, rep->amount);
}

/* Init provmem component */
void provmem_init(FILE *log_fp, FILE *blk_fp)
{
	/* Init fps */
	blk_ctrl_fp = blk_fp;
	prov_ctrl_fp = log_fp;

	/* Create the first ctrl */
	prov_ctrl = malloc(sizeof(*prov_ctrl));
	if (!prov_ctrl) {
		printf("provmem - Error: malloc failed in %s\n", __func__);
		return;
	}

	/* Invalid the ctrl */
	memset(prov_ctrl, 0x0, sizeof(*prov_ctrl));
	prov_ctrl->act = -1;
}

/* Update the provenance logging */
void provmem_update_prov(struct provusb_report *rep)
{
	struct provmem_prov *ctrl;

	/* Try to find the ctrl */
	ctrl = get_prov_ctrl(rep->lba);
	if (ctrl) {
		/* Handle read early */
		if (rep->act == PROVUSB_ACTION_READ) {
			if (rep->id == ctrl->id) {
				if (ctrl->act == PROVUSB_ACTION_READ)
					return;
				/* Fall thru to the write event if ctrl->act is write */
			} else {
				printf("provmem - Error: id mismatch with report [%d] and ctrl [%d]\n",
					rep->id, ctrl->id);
				return;
			}
			/* Unless we consider loading previous provenance
 			 * logging information before starting provd, we do
 			 * not need to worry about host id eventually.
 			 * The provenance memory is constructed everytime
 			 * when a new host machine is plugged into.
 			 * Aug 31, 2015
 			 * daveti
 			 */
		}
	} else {
		/* Get the available ctrl */
		ctrl = get_prov_ctrl_avail();
		if (!ctrl) {
			printf("provmem - Error: get_prov_ctrl_avail failed\n");
			return;
		}
		/* Fall thru to the write event for a new read */
	}

	/* Handle the write event */
	update_log_prov_ctrl(ctrl, rep);
}

/* Update the block policy control logging */
void provmem_update_blk(struct provusb_block *blk)
{
	/* Unless we are going to flush the disk
	 * at the end for self-powered devcies,
	 * there seems no optimization we could do by taking the
	 * advantage of memory construct.
	 * Aug 31, 2015
	 * daveti
	 */
	return;
}

