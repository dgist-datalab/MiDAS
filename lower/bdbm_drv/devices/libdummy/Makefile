# Makefile for a RISA Device Driver
#
CC = gcc 
OUT = libdummy.a
INCLUDE = ../../include
COMMON = ../../common
DEV_COMMON = ../common

INCLUDES := \
	-I$(PWD)/$(INCLUDE) \
	-I$(PWD)/$(COMMON)/utils \
	-I$(PWD)/$(COMMON)/3rd \
	-I$(PWD)/$(DEV_COMMON)

CFLAGS := \
	-g \
	-static \

CFLAGS += \
	-D HASH_BLOOM=20 \
	-D CONFIG_ENABLE_MSG \
	-D CONFIG_ENABLE_DEBUG \
	-D CONFIG_DEVICE_TYPE_USER_DUMMY \
	-D USE_PMU \
	-D USE_KTIMER \
	-D USER_MODE

LIBSRC := \
	dm_dummy.c \
	$(DEV_COMMON)/dev_main.c \
	$(DEV_COMMON)/dev_params.c \

LIBOBJ=$(LIBSRC:.c=.o)

$(OUT): $(LIBOBJ) $(LIBSRC)
	$(AR) r $(OUT) $(LIBOBJ)

.c.o:
	$(CC) $(INCLUDES) $(LIBS) $(CFLAGS) -c $< -o $@

clean:
	@$(RM) *.o core *~ libdummy.a
	@cd $(DEV_COMMON); $(RM) *.o
