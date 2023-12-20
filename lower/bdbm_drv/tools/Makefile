CFLAGS=-g -Wall 
CC=gcc 
LIBS=-lm -lpthread 
INCLUDES= 
OBJS_FORMAT=bdbm_format.o 
SRCS_FORMAT=bdbm_format.c 

#The variable $@ has the value of the target. In this case $@ = psort 
bdbm_format: ${OBJS_FORMAT} 
		${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS_FORMAT} ${LIBS} 

.c.o: 
		${CC} ${CFLAGS} ${INCLUDES} -c $< 

clean: 
		rm *.o bdbm_format 
