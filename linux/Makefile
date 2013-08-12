# $Id: Makefile,v 1.5 2007-01-30 11:48:33 andrew_belov Exp $
#
# Linux makefile (kbuild)

ifneq ($(KERNELRELEASE),)

obj-m := quietpc.o
quietpc-objs := regio.o hwmon.o winbond.o ite.o linuxdd.o
EXTRA_CFLAGS += -I$(PWD)/.. -DLINUX -Wno-strict-prototypes

#
# OK, this is not what kbuild was invented for. Any ideas on handling
# sources located one level above?
#

MODULE_OPTS = $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -c -o $@

%.o: ../%.c
	$(CC) $(MODULE_OPTS) $<

else
#
# Non-kbuild part
#

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

VPATH = .:..

all: quietpc.ko qctrl

quietpc.ko: 
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

qctrl: userland.o
	gcc -s -o $@ userland.o
	chmod +x $@

userland.o: userland.c
	gcc -c -Wall -DLINUX ../userland.c

distclean:
	rm -f *.o *.mod.c

clean: distclean
	rm -f *.ko qctrl

install:
	-mkdir /lib/modules/`uname -r`/misc
	cp quietpc.ko /lib/modules/`uname -r`/misc
	cp qctrl /usr/local/bin

endif
