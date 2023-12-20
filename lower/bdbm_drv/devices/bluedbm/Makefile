# Makefile for BlueDBM
#

BOARD = vc707
HARDWARE_FLAGS = -D VC707
EXTRA_CFLAGS = -D BOARD_VC707
BOARDDIR = $(BOARD)/jni
CPPDIR = connectal/cpp

INCLUDE = ../../include
COMMON = ../../common
DEV_COMMON = ../common

KBUILD_EXTRA_SYMBOLS := \
	$(PWD)/connectal/drivers/pcieportal/Module.symvers \
	$(PWD)/connectal/drivers/portalmem/Module.symvers

EXTRA_CFLAGS := \
	-D NO_CPP_PORTAL_CODE \
	-D __KERNEL__ \
	-D MMAP_HW \
	-I$(PWD) \
	-I$(PWD)/connectal \
	-I$(PWD)/connectal/cpp \
	-I$(PWD)/connectal/drivers/pcieportal \
	-I$(PWD)/connectal/drivers/portalmem \
	-I$(PWD)/$(INCLUDE) \
	-I$(PWD)/$(COMMON)/utils \
	-I$(PWD)/$(COMMON)/3rd \
	-I$(PWD)/$(DEV_COMMON) \

EXTRA_CFLAGS += \
	-D CONFIG_ENABLE_MSG \
	-D CONFIG_ENABLE_DEBUG \
	-D CONFIG_DEVICE_TYPE_BLUEDBM \
	-D KERNEL_MODE \

risa_dev_bluedbm-y := \
	dm_bluedbm.o \
	$(COMMON)/utils/umemory.o \
	$(DEV_COMMON)/dev_main.o \
	$(DEV_COMMON)/dev_params.o \
	$(DEV_COMMON)/dev_stub.o \
	$(BOARDDIR)/MMURequest.o \
	$(BOARDDIR)/MMUIndication.o \
	$(BOARDDIR)/FlashIndication.o \
	$(BOARDDIR)/FlashRequest.o \
	$(BOARDDIR)/MemServerRequest.o \
	$(CPPDIR)/portal.o \
	$(CPPDIR)/dmaManager.o \

obj-m := risa_dev_bluedbm.o

ccflags-y := -I$(src)/$(CPPDIR) -I$(src)/$(BOARDDIR) $(HARDWARE_FLAGS) -DBOARD_$(BOARD)

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
	rm -rf Module.markers modules.order
	@cd ../common; $(RM) *.o

