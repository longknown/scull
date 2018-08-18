# using EXTRA_CFLAGS can pass a C Macro while compile
EXTRA_CFLAGS=-D USE_SEQ


ifneq ($(KERNELRELEASE),)
	obj-m += scull.o
	scull-objs := sculldev.o scull_seq.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules


endif

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
