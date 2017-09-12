/*
 * dior.c
 * direct IO read
 * for ProvUSB efficient provenance storage testing
 * Sep 13, 2015
 * Use the official Linux kernel 4.4 zip file as the testing file - 83MB
 * Jan 11, 2016
 * root@davejingtian.org
 * http://davejingtian.org
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static char *filepath = "/media/daveti/5983-30AF/linux-4.4.tar.xz";
static char *testpath = "/home/daveti/linux-4.4.tar.xz";
static char *buf;
static int linux_size = 87295988;
static int size = 1024*1024*100; /* Buffer is 100MB */
static int loop = 100;
static int blk_size = 512;

int main(void)
{
	int fd;
	int i;
	int rtn;
	int total;

	/*
	 * daveti: not-aligned buffer would not work!
	buf = malloc(size);
	if (!buf) {
		printf("malloc failed [%s]\n", strerror(errno));
		return -1;
	}
	 */
	
	/* Align the buffer */
	rtn = posix_memalign((void **)&buf, blk_size, size);
	if (rtn) {
		printf("posix_memalign failed: %s\n", strerror(errno));
		return -1;
	}

	/* Align the read size */
	linux_size = linux_size/blk_size*blk_size;
	printf("aligned linux_size=%d\n", linux_size);

	fd = open(filepath, O_RDONLY|O_DIRECT|O_SYNC);
	//fd = open(testpath, O_RDONLY);
	if (fd < 0) {
		printf("open failed [%s]\n", strerror(errno));
		return -1;
	}

	/* DIO */
	for (i = 0; i < loop; i++) {
		total = 0;
		printf("read %d\n", i);

		rtn = lseek(fd, 0, SEEK_SET);
		if (rtn < 0) {
			printf("lseek error: %s\n", strerror(errno));
			break;
		}
	
		rtn = read(fd, buf, linux_size);
		if (rtn < 0) {
			printf("read error: %s\n", strerror(errno));
			break;
		}

		total += rtn;
		while (total < linux_size) {
			printf("total=%d, read-cont\n", total);
			rtn = read(fd, buf, linux_size-total);
			if (rtn < 0) {
				printf("read error: %s\n", strerror(errno));
				break;
			}
			if (rtn == 0)
				break;
			total += rtn;
		}
	}

	close(fd);

	return 0;
}

