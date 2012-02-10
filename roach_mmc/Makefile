CROSS_COMPILE=ppc_4xx-
#KERNEL_DIR=/usr/src/linux-2.6.29
KERNEL_DIR=/home/dave/work/casper_svn/roach/sw/linux
PWD=$(shell pwd)
EXTRA_CFLAGS		+= -DDEBUG

obj-m		+= roach_mmc.o

all:
	CROSS_COMPILE=$(CROSS_COMPILE) make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	CROSS_COMPILE=$(CROSS_COMPILE) make -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f modules.order Module.markers tags
