CC=gcc

all: test

CFLAGS+=\
		-g\
#		-fsanitize=address\

SRCS +=\
	   heap.c\

TARGETOBJ=\
		  $(patsubst %.c,%.o,$(SRCS))\

test:$(TARGETOBJ) main.c
	$(CC) $(CFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@$(RM) *.o
	@$(RM) test
