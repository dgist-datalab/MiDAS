# Makefile for a RISA Device Driver
#
BD?=zcu102
ifeq ($(BD),zcu102)
CC = aarch64-linux-gnu-g++
AR = aarch64-linux-gnu-ar
BOARD = zcu102
CONNECTAL_DIR = connectal_zcu102
else
#CC = arm-linux-gnueabihf-g++
#AR = arm-linux-gnueabihf-ar
#CC=g++
BOARD = zc706_ubuntu
CONNECTAL_DIR = connectal_zc706
endif

ROOTDIR = ../..
INCLUDE = $(ROOTDIR)/include
COMMON = $(ROOTDIR)/common
DM_COMMON = $(ROOTDIR)/devices/common
DM_NOHOST = $(ROOTDIR)/devices/nohost
BOARDDIR = $(BOARD)/jni_4.0

CFLAGS += \
	-g \
	-Wall -Wsign-compare -Wwrite-strings \
	-D_LARGEFILE64_SOURCE \
	-D_GNU_SOURCE  \
	-D HASH_BLOOM=20 \
	-D CONFIG_ENABLE_MSG \
	-D CONFIG_ENABLE_DEBUG \
	-D CONFIG_DEVICE_TYPE_USER_RAMDRIVE \
	-D USER_MODE \
	-D USE_PMU \
	-D USE_KTIMER \
	-D USE_NEW_RMW \
	-D NOHOST \
	-D LIBLSM \
	-D JNI=4\
	-std=c++11 \
	#-D USE_ACP \ # only valid for zc706. mkTop8k_ACP.exe should be used.
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
	-I../../devices/nohost/ \

LIBS := \
	-lm -lpthread -lrt

SRCS := \
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
	$(COMMON)/utils/umemory.c \
	$(COMMON)/utils/uthread.c \
	$(DM_COMMON)/dev_main.c \
	$(DM_COMMON)/dev_params.c \
	$(DM_NOHOST)/dm_nohost.cpp \
	$(DM_NOHOST)/sw_poller.cpp \
	../../../../include/data_struct/hash.c\

ifeq ($(priority), "true")
 SRCS+=$(PWD)/libmemio_priority.cpp\

else

  ifeq ($(non_test), "true")
   SRCS+=$(PWD)/libmemio.cpp\

  else
   SRCS+=$(PWD)/libmemio.cpp\

  endif

endif



OBJS := \
	$(SRCS:.c=.o) $(SRCS:.cpp=.o)


all: nohost.exe

nohost.exe: $(SRCS) libmemio.a main.cpp
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ main.cpp libmemio.a $(LIBS) 

libmemio.a: $(OBJS)
	$(AR) r $(@) $(OBJS)

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

.cpp.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

clean:
	@$(RM) *.o core *~ libmemio.a nohost.exe
	@$(RM) $(DM_COMMON)/*.o
	@$(RM) $(DM_NOHOST)/*.o
	@$(RM) $(PWD)/$(BOARDDIR)/*.o
	@$(RM) $(PWD)/$(CONNECTAL_DIR)/cpp/*.o
	@$(RM) $(COMMON)/utils/*.o
