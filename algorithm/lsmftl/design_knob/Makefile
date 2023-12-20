SRCS +=\
	   lsmtree_param_module.c\
	   bf_memory_calculator.c\
	   plr_memory_calculator.c\
	   design_knob.c\

	
OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

all: $(OBJS)


%.o :%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

plr/plr.o: plr/plr.cpp
	$(CC) $(CFLAGS_ALGO) -c $< -o $@
