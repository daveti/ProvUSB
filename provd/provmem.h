/*
 * provmem.h
 * Header file for provenance memory implementation
 * NOTE: this should only be used by self-powered devices
 * Aug 31, 2015
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include <stdio.h>
#include "nlm.h"

/* For block ctrl */
struct provmem_block {
	struct provusb_block	block;
	struct provusb_block	*next;
};

/* For provenance ctrl */
struct provmem_prov {
	unsigned int		lba;
	int			act;
	int			id;
	struct provmem_prov	*next;
};

/* APIs */
void provmem_init(FILE *log_fp, FILE *blk_fp);
void provmem_update_prov(struct provusb_report *rep);
void provmem_update_blk(struct provusb_block *blk);
