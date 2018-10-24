ifneq ($(KERNELRELEASE),)
	obj-m += scullpipe.o
	scullpipe-objs := scull_pipe.o scull_fops.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules


endif

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
