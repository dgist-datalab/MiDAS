# Makefile for a RISA Device Driver
#

INCLUDE := ../../include
COMMON := ../../common

#KBUILD_EXTRA_SYMBOLS := $(PWD)/../../devices/ramdrive/Module.symvers
KBUILD_EXTRA_SYMBOLS := $(PWD)/../../devices/bluedbm/Module.symvers

EXTRA_CFLAGS := -I$(PWD)/../../include

EXTRA_CFLAGS += \
	-I$(PWD)/$(INCLUDE) \
	-I$(PWD)/$(COMMON)/utils \
	-I$(PWD)/$(COMMON)/3rd \

EXTRA_CFLAGS += -D KERNEL_MODE
EXTRA_CFLAGS += -D CONFIG_ENABLE_MSG
EXTRA_CFLAGS +=	-D CONFIG_ENABLE_DEBUG

module_main-y := \
	kmain.o \
	raw-flash.o \
	$(COMMON)/utils/utime.o \
	$(COMMON)/utils/umemory.o

obj-m := module_main.o

ccflags-y := -I$(src)

export KROOT=/lib/modules/$(shell uname -r)/build

.PHONY: default
default: modules

.PHONY: modules
modules:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules
	@chmod +x module_main.ko

.PHONY: modules_check
modules_check:
	@$(MAKE) -C $(KROOT) C=2 M=$(PWD) modules

.PHONY: modules_install
modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install

.PHONY: kernel_clean
kernel_clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean
 
.PHONY: clean
clean: kernel_clean
	@rm -rf Module.markers modules.order
	@rm -rf *.o .*.cmd; rm -rf *.o .*.cmd;

