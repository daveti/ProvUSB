/*
 * dior.c
 * direct IO read
 * for ProvUSB efficient provenance storage testing
 * Sep 13, 2015
 * Use the official Linux kernel 4.4 zip file as the testing file - 83MB
 * Jan 11, 2016
 * Generate a uniform distribution for R/W ratio
 * Jan 14, 2016
 * root@davejingtian.org
 * http://davejingtian.org
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DIST_RANGE	10

static char *filepath = "/media/daveti/386D-43B9/linux-4.4.tar.xz";
static char *testpath = "/home/daveti/linux-4.4.tar.xz";
static char *buf;
static int linux_size = 87295988;
static int size = 1024*1024*100; /* Buffer is 100MB */
static int loop = 100;
static int blk_size = 512;
static int w_portion = 5; /* I should have made it an arugment, shouldn't i */
static int r_total;
static int w_total;
static int sleep_time = 20; /* seconds */
static int gen_rand = 0;

/* Returns an integer in the range [0, n).
 * Uses rand(), and so is affected-by/affects the same seed.
 */
static int rand_int(int n)
{
	int r;

	if ((n - 1) == RAND_MAX)
		return rand();

	// Chop off all of the values that would cause skew...
	long end = RAND_MAX / n; // truncate skew
	assert(end > 0L);
	end *= n;
	// ... and ignore results from rand() that fall above that limit.
	// (Worst case the loop condition should succeed 50% of the time,
	// so we can expect to bail out of this loop pretty quickly.)
	while ((r = rand()) >= end);
	return r % n;
}

/* Return 1 if this operation should be read
 * otherwise 0 for write.
 */
static int launch_read()
{
	int r;

	if (w_portion == 0)
		return 1;
	if (w_portion == 10)
		return 0;

	/* Play the coin */
	r = rand_int(DIST_RANGE);
	if (r < w_portion)
		return 0;

	return 1; 
}

/* DIO read */
static int dio_read(int fd, char *buf, int size)
{
	int rtn;
	int total;
	char *ptr;

	/* Reset fp */
	rtn = lseek(fd, 0, SEEK_SET);
      	if (rtn < 0) {
		printf("lseek error: %s\n", strerror(errno));
		return -1;
	}

	/* Read */
	rtn = read(fd, buf, linux_size);
	if (rtn < 0) {
		printf("read error: %s\n", strerror(errno));
		return -1;
	}

	total = 0;
	total += rtn;
	while (total < linux_size) {
		printf("total=%d, read-cont\n", total);
		ptr = buf + total;
		rtn = read(fd, ptr, linux_size-total);
		if (rtn < 0) {
			printf("read error: %s\n", strerror(errno));
			return -1;
		}
		if (rtn == 0)
			break;
		total += rtn;
	}

	return total;
}

/* DIO write */
static int dio_write(int fd, char *buf, int size)
{
	int rtn;
	int total;
	char *ptr;

	/* Reset fp */
	rtn = lseek(fd, 0, SEEK_SET);
	if (rtn < 0) {
		printf("lseek error: %s\n", strerror(errno));
		return -1;
	}

	/* Write */
	rtn = write(fd, buf, linux_size);
	if (rtn < 0) {
		printf("write error: %s\n", strerror(errno));
		return -1;
	}

	total = 0;
	total += rtn;
	while (total < linux_size) {
		printf("total=%d, write-cont\n", total);
		ptr = buf + total;
		rtn = write(fd, ptr, linux_size-total);
		if (rtn < 0) {
			printf("write error: %s\n", strerror(errno));
			return -1;
		}
		if (rtn == 0)
			break;
		total += rtn;
	}

	return total;
}

int main(void)
{
	int fd;
	int i;
	int rtn;
	int total;

	/* Seed */
	srand(time(NULL));

	/* Only for rand generation */
	if (gen_rand) {
		fd = -1;
		printf("Random generation mode:\n");
		for (i = 0; i < loop; i++) {
			if (launch_read())
				r_total++;
			else
				w_total++;
		}
		goto DONE;
	}

	/* Align the buffer */
	rtn = posix_memalign((void **)&buf, blk_size, size);
	if (rtn) {
		printf("posix_memalign failed: %s\n", strerror(errno));
		return -1;
	}

	/* Align the read size */
	linux_size = linux_size/blk_size*blk_size;
	printf("aligned linux_size=%d\n", linux_size);

	fd = open(filepath, O_RDWR|O_DIRECT|O_SYNC);
	//fd = open(testpath, O_RDONLY);
	if (fd < 0) {
		printf("open failed [%s]\n", strerror(errno));
		return -1;
	}

	/* Hack - do a read now for following write */
	rtn = dio_read(fd, buf, linux_size);
	if (rtn != linux_size) {
		printf("dio_read error - abort\n");
		goto DONE;
	}
	printf("pre-read done, write-portion=%d\n", w_portion);

	/* DIO */
	for (i = 0; i < loop; i++) {
		/* Sleep? */
		printf("sleeping for %d seconds\n", sleep_time);
		sleep(sleep_time);

		if (launch_read()) {
			/* Read */
			r_total++;
			printf("loop=%d, read\n", i);
			rtn = dio_read(fd, buf, linux_size);
			if (rtn != linux_size) {
				printf("dio_read error - abort\n");
				break;
			}
		}
		else {
			/* Write */
			w_total++;
			printf("loop=%d, write\n", i);
			rtn = dio_write(fd, buf, linux_size);
			if (rtn != linux_size) {
				printf("dio_write error - abort\n");
				break;
			}
		}
	}
DONE:
	printf("Loop=%d, Wportion=%d, R=%d, W=%d\n",
		loop, w_portion, r_total, w_total);
	if (fd != -1)
		close(fd);
	return 0;
}

