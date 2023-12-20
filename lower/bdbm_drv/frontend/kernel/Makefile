# Makefile for a RISA Device Driver
#

FTL := ../../ftl
INCLUDE := ../../include
COMMON := ../../common
DM_COMMON := ../../devices/common

#KBUILD_EXTRA_SYMBOLS := $(PWD)/../../devices/ramdrive/Module.symvers
#KBUILD_EXTRA_SYMBOLS := $(PWD)/../../devices/ramdrive_timing/Module.symvers
KBUILD_EXTRA_SYMBOLS := $(PWD)/../../devices/bluedbm/Module.symvers

EXTRA_CFLAGS := \
	-I$(PWD)/$(INCLUDE) \
	-I$(PWD)/$(COMMON)/utils \
	-I$(PWD)/$(COMMON)/3rd \
	-I$(PWD)/$(FTL) \
	-I$(PWD)/$(DM_COMMON) \

EXTRA_CFLAGS += -D HASH_BLOOM=20 	# for HASH (8KB)
EXTRA_CFLAGS += -D CONFIG_ENABLE_MSG
EXTRA_CFLAGS += -D CONFIG_ENABLE_DEBUG
EXTRA_CFLAGS += -D USE_PMU
EXTRA_CFLAGS += -D KERNEL_MODE
EXTRA_CFLAGS += -D USE_NEW_RMW

#EXTRA_CFLAGS += -D USE_KTIMER
#EXTRA_CFLAGS += -D EMULATE_BAD_BLOCKS
#EXTRA_CFLAGS += -D USE_COMPLETION

#bdbm_drv-y := \
	kmain.o \
	blkdev.o \
	blkio.o \
	$(FTL)/ftl_params.o \
	$(FTL)/pmu.o \
	$(FTL)/hlm_nobuf.o \
	$(FTL)/hlm_buf.o \
	$(FTL)/hlm_dftl.o \
	$(FTL)/hlm_rsd.o \
	$(FTL)/llm_noq.o \
	$(FTL)/llm_mq.o \
	$(FTL)/algo/abm.o \
	$(FTL)/algo/no_ftl.o \
	$(FTL)/algo/block_ftl.o \
	$(FTL)/algo/page_ftl.o \
	$(FTL)/algo/dftl.o \
	$(FTL)/algo/dftl_map.o \
	$(FTL)/queue/queue.o \
	$(FTL)/queue/prior_queue.o \
	$(FTL)/queue/rd_prior_queue.o \
	$(DM_COMMON)/dev_params.o \
	$(COMMON)/utils/utime.o \
	$(COMMON)/utils/ufile.o \
	$(COMMON)/utils/uthread.o \
	$(COMMON)/bdbm_main.o \

bdbm_drv-y := \
	kmain.o \
	blkdev.o \
	blkio.o \
	$(FTL)/ftl_params.o \
	$(FTL)/pmu.o \
	$(FTL)/hlm_nobuf.o \
	$(FTL)/llm_mq.o \
	$(FTL)/algo/abm.o \
	$(FTL)/algo/page_ftl.o \
	$(FTL)/algo/block_ftl.o \
	$(FTL)/queue/queue.o \
	$(FTL)/queue/prior_queue.o \
	$(FTL)/queue/rd_prior_queue.o \
	$(FTL)/hlm_reqs_pool.o \
	$(DM_COMMON)/dev_params.o \
	$(COMMON)/utils/utime.o \
	$(COMMON)/utils/ufile.o \
	$(COMMON)/utils/uthread.o \
	$(COMMON)/utils/umemory.o \
	$(COMMON)/bdbm_main.o \


obj-m := bdbm_drv.o

ccflags-y := -I$(src)

export KROOT=/lib/modules/$(shell uname -r)/build

.PHONY: default
default: modules

.PHONY: modules
modules:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules

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
	@cd ../../ftl; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd; cd -;
	@cd $(FTL); rm *.o; cd -;
	@cd $(COMMON)/utils; rm *.o; cd -;
	@cd $(COMMON)/3rd; rm *.o; cd -;
	@cd $(DM_COMMON); rm *.o; cd -;

