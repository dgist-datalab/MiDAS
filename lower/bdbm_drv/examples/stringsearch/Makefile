CC = gcc
CXX = g++
CFLAGS = -Wall -g -O2 -D_LARGEFILE64_SOURCE -D_GNU_SOURCE
CXXFLAGS = -Wall -g -O2 -D_LARGEFILE64_SOURCE -D_GNU_SOURCE
LIBS =  -lm -lblkid
INCLUDES =
OBJS = ss.o ss_impl_sw.o ss_impl_hw.o
DEPS = ss_impl.h

%.o: %.c %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ss: $(OBJS)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm *.o core *~ ss_sw ss_hw
