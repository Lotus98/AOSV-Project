EXTRA_CFLAGS = -Wall -g
KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

# Comment line to disable debug prints
DEBUG = y

obj-m += ums.o

all:
	make -C $(KDIR) M=$(PWD) modules

.PHONY: clean install
clean:
	rm -rf *.o *~ core .depend *.mod.o .*.cmd *.ko *.mod.c *.mod \
		.tmp_versions *.markers *.symvers modules.order

tidy:
	rm -rf *.o *~ core .depend *.mod.o .*.cmd *.mod.c *.mod \
		.tmp_versions *.markers *.symvers modules.order

install:
	sudo insmod *.ko
