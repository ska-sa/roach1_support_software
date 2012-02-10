#KERNEL_DIR=/usr/src/linux-headers-2.6.28-11-generic
KERNEL_DIR=/home/shanly/SVN/svnROACH/sw/borph_roach
PWD=$(shell pwd)
EXTRA_CFLAGS		+= -DDEBUG

obj-m		+= mmc_dummy.o

all:
	make CROSS_COMPILE=ppc_4xx- -C  $(KERNEL_DIR) M=$(PWD) modules

clean:
	make CROSS_COMPILE=ppc_4xx- -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f modules.order Module.markers tags
