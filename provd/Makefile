# Makefile for provd
# using Yocto cross compile
# Sep 1, 2015
# root@davejingtian.org
# http://davejingtian.org

# NOTE:
# Do NOT change CC,CFLAGS,LDFLAGS

OBJS = provd.o nlm.o provmem.o

provd : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

