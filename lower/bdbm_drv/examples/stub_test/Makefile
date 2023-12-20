# Makefile for a RISA Device Driver
#

CC = gcc
FTL := ../../ftl
#CFLAGS := -Wall -g -O2 -D_LARGEFILE64_SOURCE -D_GNU_SOURCE 
CFLAGS := -Wall -g -D_LARGEFILE64_SOURCE -D_GNU_SOURCE 
LIBS += -lm -lpthread -lrt

INCLUDES = \
		  -I$(PWD)/../../include \
		  -I$(PWD)/../../include/utils \
		  -I$(PWD)/$(FTL) \
		  -I$(PWD)/../../devices/cpp \
		  
CFLAGS += -D HASH_BLOOM=20 \
		  -D CONFIG_ENABLE_MSG \
		  -D CONFIG_ENABLE_DEBUG \
		  -D USE_PMU \
		  -D USE_KTIMER \
		  -D USER_MODE

SRCS := \
	main.c \

stub_test: $(SRCS) 
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRCS) $(LIBS) 

clean:
	@$(RM) *.o core *~ userapp
	#@cd ../../ftl; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	#@cd ../../include; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
	#@cd ../../include/utils; rm -rf *.o .*.cmd; rm -rf */*.o */.*.cmd;
