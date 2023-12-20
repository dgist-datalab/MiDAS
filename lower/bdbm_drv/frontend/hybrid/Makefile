# Makefile for a RISA Device Driver
#

FTL := ../../ftl
INCLUDE := ../../include
KERN := ../kernel
COMMON := ../../common
DM_COMMON := ../../devices/common

EXTRA_CFLAGS := \
	-I$(PWD)/$(INCLUDE) \
	-I$(PWD)/$(COMMON)/utils \
	-I$(PWD)/$(COMMON)/3rd \
	-I$(PWD)/$(FTL) \
	-I$(PWD)/$(KERN) \
	-I$(PWD)/$(DM_COMMON) \

EXTRA_CFLAGS += -D HASH_BLOOM=20 	# for HASH (8KB)
EXTRA_CFLAGS += -D CONFIG_ENABLE_MSG
EXTRA_CFLAGS += -D CONFIG_ENABLE_DEBUG
EXTRA_CFLAGS += -D KERNEL_MODE
EXTRA_CFLAGS += -D USE_PMU
EXTRA_CFLAGS += -D USE_BLOCKIO_PROXY
EXTRA_CFLAGS += -D USE_NEW_RMW

bdbm_drv-y := \
	kmain.o \
	blkio_proxy.o \
	blkio_proxy_reqs_pool.o \
	$(FTL)/ftl_params.o \
	$(FTL)/pmu.o \
	$(DM_COMMON)/dev_params.o \
	$(COMMON)/utils/utime.o \
	$(COMMON)/utils/ufile.o \
	$(COMMON)/utils/uthread.o \
	$(COMMON)/utils/umemory.o \
	$(COMMON)/bdbm_main.o \
	$(KERN)/blkdev.o \

obj-m := bdbm_drv.o

ccflags-y := -I$(src)

export KROOT=/lib/modules/$(shell uname -r)/build

.PHONY: default
#default: kernel user	
default: user	

kernel: 
	@$(MAKE) -f Makefile.kernel

user:
	@cd ../../devices/libramdrive; $(MAKE) clean; $(MAKE) 
	@$(MAKE) -f Makefile.user # build user

.PHONY: clean 
clean: clean-kernel clean-user 

clean-kernel:
	@$(MAKE) -f Makefile.kernel clean

clean-user:
	@$(MAKE) -f Makefile.user clean

mount:
	sudo insmod bdbm_drv.ko

umount:
	sudo rmmod bdbm_drv
