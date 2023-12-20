SRCS +=\
		bdbm_inf.c\

OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

TARGETOBJ:=\
	$(addprefix ../../object/,$(OBJS))\
#$(patsubst %.c,%.o,$(SRCS))\

CFLAGS_LOWER += \
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
	-std=c++11 \
	#-D USE_ACP \ # only valid for zc706. mkTop8k_ACP.exe should be used.
	#-D ZYNQ=1 \ # Already defined in ConnectalProjectConfig.h:20 

INCLUDE = ./include
COMMON = ./common
DM_COMMON = ./devices/common
DM_NOHOST = ./devices/nohost
BOARDDIR = ./jni


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

DEBUG : all
all: $(TARGETOBJ) libmemio.a

libmemio.a:
	cd ./frontend/libmemio && $(MAKE) libmemio.a
	mv ./frontend/libmemio/libmemio.a ../../object/libmemio.a

libbdbm_drv.a : $(TARGETOBJ)
	$(AR) r $(@) $(TARGETOBJ)

bdbm_test: libmemio.a test_main.c
	g++ $(INCLUDES) $(CFLAGS_LOWER) -o $@ test_main.c ../../include/FS.c ../../bench/measurement.c ../../object/libmemio.a

.c.o :
	$(CC) $(INCLUDES) $(CFLAGS_LOWER) -c $< -o $@

../../object/%.o: %.c
	$(CC) $(INCLUDES) $(CFLAGS_LOWER) -c $< -o $@

clean : 
	cd ./frontend/libmemio && $(MAKE) clean
	@$(RM) libpos.a
	@$(RM) *.o
