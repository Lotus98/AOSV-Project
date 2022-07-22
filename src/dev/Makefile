EXTRA_CFLAGS = -Wall
KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

# Comment line to disable debug prints
DEBUG = y

ifeq ($(DEBUG), y)
	DBG=EXTRA_CFLAGS=-DDEBUG
endif

obj-m += umsdev.o
umsdev-objs := ums.o ioctl.o

all:
	make $(DBG) -C $(KDIR) M=$(PWD) $(EXTRA_CFLAGS) modules

.PHONY: clean tidy install uninstall
clean:
	rm -rf *.o *~ core .depend *.mod.o .*.cmd *.ko *.mod.c *.mod \
		.tmp_versions *.markers *.symvers modules.order

tidy:
	rm -rf *.o *~ core .depend *.mod.o .*.cmd *.mod.c *.mod \
		.tmp_versions *.markers *.symvers modules.order

install:
	sudo insmod umsdev.ko

uninstall:
	sudo rmmod umsdev
