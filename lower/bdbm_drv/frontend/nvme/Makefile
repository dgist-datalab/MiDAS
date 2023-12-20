# Makefile for a RISA Device Driver
#

CC = gcc
FTL := ../../ftl
INCLUDE := ../../include
COMMON := ../../common
DM_COMMON_DF := ../../devices/dragonfire

CFLAGS := -Wall -g -D_LARGEFILE64_SOURCE -D_GNU_SOURCE 
LIBS += -lm -lpthread -lrt

INCLUDES = -I$(PWD)/$(INCLUDE) \
		   -I$(PWD)/$(COMMON)/utils \
		   -I$(PWD)/$(COMMON)/3rd \
		   -I$(PWD)/$(FTL) \
		   -I$(PWD)/$(DM_COMMON_DF) \

CFLAGS += -D HASH_BLOOM=20 \
		  -D CONFIG_ENABLE_MSG \
		  -D CONFIG_ENABLE_DEBUG \
		  -D USER_MODE \
		  -D USE_PMU \
		  -D USE_NEW_RMW \

DMPROXY := dev_proxy.c	# a device manager proxy
DMLIB := ../../devices/dragonfire/libdf.a	# a device driver library for DragonFire cards (written by VVDN)

SRCS := \
	userio.c \
	main.c \
	df_umemory.c \
	$(FTL)/pmu.c \
	$(FTL)/hlm_nobuf.c \
	$(FTL)/llm_mq.c \
	$(FTL)/llm_noq.c \
	$(FTL)/hlm_reqs_pool.c \
	$(FTL)/ftl_params.c \
	$(FTL)/algo/abm.c \
	$(FTL)/algo/page_ftl.c \
	$(FTL)/algo/block_ftl.c \
	$(FTL)/queue/queue.c \
	$(FTL)/queue/prior_queue.c \
	$(FTL)/queue/rd_prior_queue.c \
	$(COMMON)/utils/utime.c \
	$(COMMON)/utils/ufile.c \
	$(COMMON)/utils/uthread.c \
	$(COMMON)/utils/upage.c \
	$(COMMON)/3rd/uilog.c \
	$(COMMON)/bdbm_main.c \
	$(DMPROXY) \
	$(DMLIB) \

SRCS2 := \
	main.c \

libftl: $(SRCS) $(DMLIB)
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRCS) $(LIBS) 

clean:
	@$(RM) *.o core *~ libftl 
	@cd $(FTL); rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	@cd $(COMMON)/utils; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	@cd $(COMMON)/3rd; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	@cd $(COMMON); rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
