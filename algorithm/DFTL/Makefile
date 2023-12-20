SRCS +=\
	   page.c\
	   gc.c\
	   gc_map.c\
	   demand_io.c\
	   demand_mapping.c\

DIRS := $(shell find ./caching/ -type d)

OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

CACHE_OBJS:=\
	$(patsubst %.c,%.o,$(CACHE_SRCS))\

TARGETOBJ:=\
	$(addprefix ../../object/,$(OBJS))\


LEAK: all
all: $(TARGETOBJ) traverse

libnom.a : $(TARGETOBJ)
	$(AR) r $(@) $(TARGETOBJ)

%.o :%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

../../object/%.o:%.c
	$(CC) $(CFLAGS_ALGO) -c $< -o $@

traverse: $(DIRS)
	for d in $+; do \
		$(MAKE) -i -C "$$d"; \
	done

clean : 
	@$(RM) libnom.a
	@$(RM) *.o
