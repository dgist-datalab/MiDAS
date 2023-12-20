SRCS +=\
	   page.c\
	   map.c\
	   gc.c\
	   model.c\
	   midas.c\
	   hot.c\

OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

TARGETOBJ:=\
	$(addprefix ../../object/,$(OBJS))\

LEAK: all
all: $(TARGETOBJ)

libnom.a : $(TARGETOBJ)
	$(AR) r $(@) $(TARGETOBJ)

%.o :%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

../../object/%.o:%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

clean : 
	@$(RM) libnom.a
	@$(RM) *.o
