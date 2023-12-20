SRCS +=\
	   bf_set.c\
	   compressed_bloomfilter.c\
	   guard_bf_set.c\
	   plr/plr.c\
	   plr_helper.c\
	  
# bloomfilter.c\

	
OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

all: $(OBJS)
	mv ./plr/plr.o .


%.o :%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

plr/plr.o: plr/plr.cpp
	$(CC) $(CFLAGS_ALGO) -c $< -o $@
