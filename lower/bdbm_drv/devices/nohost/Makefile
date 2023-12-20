# Makefile for a RISA Device Driver
#
BD?=zcu102
ifeq ($(BD),zcu102)
CC = aarch64-linux-gnu-g++
AR = aarch64-linux-gnu-ar
BOARD = zcu102
CONNECTAL_DIR = connectal_zcu102
else
CC = arm-linux-gnueabihf-g++
AR = arm-linux-gnueabihf-ar
BOARD = zc706_ubuntu
CONNECTAL_DIR = connectal_zc706
endif

ROOTDIR = ../..
INCLUDE = $(ROOTDIR)/include
COMMON = $(ROOTDIR)/common
DM_COMMON = $(ROOTDIR)/devices/common
BOARDDIR = $(BOARD)/jni

OUT = nohost.a

CFLAGS := -g -static
CFLAGS += \
	-D HASH_BLOOM=20 \
	-D CONFIG_ENABLE_MSG \
	-D CONFIG_ENABLE_DEBUG \
	-D CONFIG_DEVICE_TYPE_USER_RAMDRIVE \
	-D USER_MODE \
	-D USE_PMU \
	-D USE_KTIMER \
	-D USE_NEW_RMW \
	#-D ZYNQ=1 \ # Already defined in ConnectalProjectConfig.h:20 

INCLUDES := \
	-I$(PWD) \
	-I$(PWD)/$(INCLUDE) \
	-I$(PWD)/$(COMMON)/utils \
	-I$(PWD)/$(COMMON)/3rd \
	-I$(PWD)/$(DM_COMMON) \
	-I$(PWD)/$(CONNECTAL_DIR) \
	-I$(PWD)/$(CONNECTAL_DIR)/cpp \
	-I$(PWD)/$(BOARDDIR) \
	-I$(PWD)/$(CONNECTAL_DIR)/drivers/zynqportal \
	-I$(PWD)/$(CONNECTAL_DIR)/drivers/portalmem \


LIBSRC := \
	$(DM_COMMON)/dev_main.c \
	$(DM_COMMON)/dev_params.c \
	$(PWD)/$(BOARDDIR)/FlashRequest.c \
	$(PWD)/$(BOARDDIR)/FlashIndication.c \
	$(PWD)/$(BOARDDIR)/GeneratedCppCallbacks.cpp \
	$(PWD)/$(BOARDDIR)/MMURequest.c \
	$(PWD)/$(BOARDDIR)/MMUIndication.c \
	$(PWD)/$(BOARDDIR)/MemServerRequest.c \
	$(PWD)/$(BOARDDIR)/MemServerIndication.c \
	$(PWD)/$(CONNECTAL_DIR)/cpp/portal.c \
	$(PWD)/$(CONNECTAL_DIR)/cpp/portalPrintf.c \
	$(PWD)/$(CONNECTAL_DIR)/cpp/transportHardware.c \
	$(PWD)/$(CONNECTAL_DIR)/cpp/poller.cpp \
	$(PWD)/$(CONNECTAL_DIR)/cpp/DmaBuffer.cpp \
	$(PWD)/$(CONNECTAL_DIR)/cpp/dmaManager.c \
	$(PWD)/$(CONNECTAL_DIR)/cpp/platformMemory.cpp \
	$(PWD)/$(CONNECTAL_DIR)/cpp/timer.c \
	dm_nohost.cpp \
	sw_poller.cpp \

LIBOBJ := \
	$(DM_COMMON)/dev_main.o \
	$(DM_COMMON)/dev_params.o \
	$(PWD)/$(BOARDDIR)/FlashRequest.o \
	$(PWD)/$(BOARDDIR)/FlashIndication.o \
	$(PWD)/$(BOARDDIR)/GeneratedCppCallbacks.o \
	$(PWD)/$(BOARDDIR)/MMURequest.o \
	$(PWD)/$(BOARDDIR)/MMUIndication.o \
	$(PWD)/$(BOARDDIR)/MemServerRequest.o \
	$(PWD)/$(BOARDDIR)/MemServerIndication.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/portal.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/portalPrintf.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/transportHardware.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/poller.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/DmaBuffer.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/dmaManager.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/platformMemory.o \
	$(PWD)/$(CONNECTAL_DIR)/cpp/timer.o \
	dm_nohost.o \
	sw_poller.o \

all: $(OUT)

$(OUT): $(LIBOBJ)
	$(AR) r $(OUT) $(LIBOBJ)

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

.cpp.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

clean:
	@$(RM) *.o core *~ nohost.a
	@$(RM) $(DM_COMMON)/*.o
	@$(RM) $(PWD)/$(BOARDDIR)/*.o
	@$(RM) $(PWD)/$(CONNECTAL_DIR)/cpp/*.o

