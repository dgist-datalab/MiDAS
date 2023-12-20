SRCS+=\
	  sftl_cache.c\
	  sftl_verify.c\

OBJS :=\
	   $(patsubst %.c, %.o, $(SRCS))\

all:$(OBJS)
	mv *.o ../../../../object/

.c.o:
	$(CC) $(CFLAGS_ALGO) -c $< -o $@
