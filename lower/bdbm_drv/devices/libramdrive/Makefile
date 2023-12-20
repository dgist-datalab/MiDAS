# Makefile for a RISA Device Driver
#
CC = g++
#CC = arm-linux-gnueabihf-g++
OUT = libramdrive.a
INCLUDE = ../../include
COMMON = ../../common
DM_COMMON = ../common

INCLUDES := \
	-I$(PWD)/$(INCLUDE) \
	-I$(PWD)/$(COMMON)/utils \
	-I$(PWD)/$(COMMON)/3rd \
	-I$(PWD)/$(DM_COMMON)

CFLAGS := \
	-g \
	-static \

CFLAGS += \
	-D HASH_BLOOM=20 \
	-D CONFIG_ENABLE_MSG \
	-D CONFIG_ENABLE_DEBUG \
	-D CONFIG_DEVICE_TYPE_USER_RAMDRIVE \
	-D USE_PMU \
	-D USE_KTIMER \
	-D USER_MODE \
	-D USE_NEW_RMW \

LIBSRC := \
	$(DM_COMMON)/dev_main.c \
	$(DM_COMMON)/dev_params.c \
	../ramdrive/dev_ramssd.c \
	../ramdrive/dm_ramdrive.c \

LIBOBJ=$(LIBSRC:.c=.o)

$(OUT): $(LIBOBJ)
	$(AR) r $(OUT) $(LIBOBJ)

.c.o:
	$(CC) $(INCLUDES) $(LIBS) $(CFLAGS) -c $< -o $@

clean:
	@$(RM) *.o core *~ libramdrive.a
	@cd $(DM_COMMON); $(RM) *.o
	@cd ../ramdrive; $(RM) *.o
