ifneq ($(KERNELRELEASE),)
	obj-m := scull.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

