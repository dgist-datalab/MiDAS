SRCS +=\
		amf_info.c\

OBJS :=\
	$(patsubst %.c,%.o,$(SRCS))\

TARGETOBJ:=\
	$(addprefix ../../object/,$(OBJS))\

all: $(TARGETOBJ) libAmfManager.a

libAmfManager.a:
	cd ./libamfdriver/ && make 
	mv ./libamfdriver/libAmfManager.a ../../object/

.c.o :
	$(CC) $(INCLUDES) $(CFLAGS_LOWER) -c $< -o $@

../../object/%.o: %.c
	$(CC) $(INCLUDES) $(CFLAGS_LOWER) -c $< -o $@

clean:
	cd ./libamfdriver && make clean


