
CFLAGS=\
	   -g\
	
SRCS+=\
	  fd_sock.c\
	  buffer_manager.c\


TARGETOBJ =\
	$(patsubst %.c,%.o,$(SRCS))\

socktest: main.c libfdsock.a
	$(CC) $(CFLAGS) -o $@ $^

libfdsock.a:$(TARGETOBJ)
	$(AR) r $(@) *.o; rm *.o


.c.o :
	$(CC) $(COMMONFLAGS) -c $< -o $@

clean:
	@$(RM) *.o
	@$(RM) libfdsock.a
	@$(RM) socktest

