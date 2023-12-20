SRCS +=\
	   pt_block_manager.c\

OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

TARGETOBJ:=\
	$(addprefix ../../object/,$(OBJS))\

all: $(TARGETOBJ)

.c.o :  ../../include/settings.h 
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

../../object/%.o: %.c ../../include/settings.h
	$(CC) $(CFLAGS) $(CFLAGS_ALGO) -c $< -o $@


clean : 
	@$(RM) *.o
