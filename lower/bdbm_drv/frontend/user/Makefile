# Makefile for a RISA Device Driver
#

#CC = g++
CC = arm-linux-gnueabihf-g++
FTL := ../../ftl
INCLUDE := ../../include
COMMON := ../../common
DM_COMMON := ../../devices/common

CFLAGS := -Wall -g -D_LARGEFILE64_SOURCE -D_GNU_SOURCE 
LIBS += -lm -lpthread -lrt

INCLUDES = -I$(PWD)/$(INCLUDE) \
		   -I$(PWD)/$(COMMON)/utils \
		   -I$(PWD)/$(COMMON)/3rd \
		   -I$(PWD)/$(FTL) \
		   -I$(PWD)/$(DM_COMMON) \

CFLAGS += -D HASH_BLOOM=20 \
		  -D CONFIG_ENABLE_MSG \
		  -D CONFIG_ENABLE_DEBUG \
		  -D USER_MODE \
		  -D USE_PMU \
	#	  -D USE_NEW_RMW \

DMLIB := ../../devices/libramdrive/libramdrive.a
#DMLIB := ../../devices/nohost/nohost.a
LIBFTL := libftl.a

SRCS := \
	main.c \

libftl: $(SRCS) $(DMLIB) $(LIBFTL)
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRCS) $(LIBS) $(LIBFTL) $(DMLIB)

clean:
	@$(RM) *.o core *~ libftl 
	@cd $(FTL); rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	@cd $(COMMON)/utils; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	@cd $(COMMON)/3rd; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	@cd $(COMMON); rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
