
SRCS +=\
	   no_dev.c\

OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

TARGETOBJ:=\
	$(addprefix ../../object/,$(OBJS))\
#$(patsubst %.c,%.o,$(SRCS))\

DEBUG : all
all: $(TARGETOBJ)

libpos.a : $(TARGETOBJ)
	$(AR) r $(@) $(TARGETOBJ)
	

.c.o :
	$(CC) -laio $(CFLAGS_LOWER) -c $< -o $@

../../object/%.o: %.c
	$(CC) $(CFLAGS_LOWER) -c $< -o $@

clean : 
	@$(RM) libpos.a
	@$(RM) *.o
