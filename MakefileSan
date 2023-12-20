#override export CC=g++-9
#override export CXX=g++-9
#override export AR=gcc-ar
#override export NM=gcc-nm

export CC=g++
export CXX=g++

TARGET_INF=interface
export TARGET_LOWER=posix_memory
export TARGET_ALGO=lsmftl
export TARGET_BM=sequential
JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64

PPWD=$(pwd)

DEBUGFLAGS=\
			-rdynamic\
			-Wno-pointer-arith\
			-g\
-fsanitize=address\

export COMMONFLAGS=\
			-Wno-write-strings\
			-Wno-sign-compare\
			-Wno-unused-function\
			-DLARGEFILE64_SOURCE\
			-D_GNU_SOURCE\
			-DSLC\
			-D$(TARGET_BM)\
			-Wno-unused-but-set-variable\
			-DDEBUG\
#	-O3 -march=native -mtune=native -flto=20 \

COMMONFLAGS+=$(DEBUGFLAGS)\

export CFLAGS_ALGO=\
			 -fPIC\
			 -Wall\
			 -D$(TARGET_LOWER)\

export CFLAGS_LOWER=\
		     -fPIC\
			 -lpthread\
			 -Wall\
			 -D_FILE_OFFSET_BITS=64\
#			 -DLOWER_MEM_DEV\

export priority="false"
export ORIGINAL_PATH=$(PPWD)

export kv_flag="true"

#CFLAGS_ALGO+=-DCOMPACTIONLOG\
	
CFLAGS_ALGO+=$(COMMONFLAGS)\
			 -D$(TARGET_ALGO)\

CFLAGS_LOWER+=$(COMMONFLAGS)\
			  -D$(TARGET_ALGO)\

ifeq ($(TARGET_ALGO), lsmtree)
 CFLAGS_ALGO+=-DLSM_SKIP
endif

ifeq ($(CC), gcc)
 CFLAGS_ALGO+=-Wno-discarded-qualifiers -std=c99
 CFLAGS_LOWER+=-Wno-discarded-qualifiers -std=c99
else
 CFLAGS_ALGO+= -std=c++11
 CFLAGS_LOWER+= -std=c++11
endif

CFLAGS +=\
		 $(CFLAGS_ALGO)\
		 -D$(TARGET_LOWER)\
		 -D$(TARGET_ALGO)\
		 -D$(TARGET_INF)\
		 -D_DEFAULT_SOURCE\
		 -D_BSD_SOURCE\
-DBENCH\
-DCDF\

SRCS +=\
	./interface/queue.c\
	./interface/interface.c\
	./interface/vectored_interface.c\
	./include/FS.c\
	./include/slab.c\
	./include/utils/debug_tools.c\
	./include/utils/dl_sync.c\
	./include/utils/rwlock.c\
	./include/utils/cond_lock.c\
	./include/utils/data_checker.c\
	./include/data_struct/hash_kv.c\
	./include/data_struct/list.c\
	./include/data_struct/redblack.c\
	./include/data_struct/heap.c\
	./include/data_struct/lru_list.c\
	./bench/measurement.c\
	./bench/bench.c\
	./bench/bench_demand.c\
	./bench/bench_transaction.c\
	./bench/bench_parsing.c\
	./include/utils/thpool.c\
	./include/utils/kvssd.c\
	./include/utils/sha256.c\
	./include/utils/tag_q.c\
	./blockmanager/base_block_manager.c\
	./blockmanager/bb_checker.c\

TARGETOBJ =\
			$(patsubst %.c,%.o,$(SRCS))\

MEMORYOBJ =\
		   	$(patsubst %.c,%_mem.o,$(SRCS))\

DEBUGOBJ =\
		   	$(patsubst %.c,%_d.o,$(SRCS))\


ifeq ($(TARGET_LOWER),bdbm_drv)
	ARCH +=./object/libmemio.a
endif

ifeq ($(TARGET_LOWER),AMF)
	ARCH +=./object/libAmfManager.a
endif

LIBS +=\
		-lpthread\
		-lm\
		-ljemalloc\
#	-laio\

all: driver

DEBUG: debug_driver

duma_sim: duma_driver

debug_driver: ./interface/main.c libdriver_d.a
	$(CC) $(CFLAGS) -DDEBUG -o $@ $^ $(LIBS)

driver: ./interface/vectored_main.c libdriver.a
	$(CC) $(CFLAGS) -o $@ $^ $(ARCH) $(LIBS)

bd_testcase: ./interface/mainfiles/testcase.c libdriver.a
	$(CC) $(CFLAGS) -o $@ $^ $(ARCH) $(LIBS)

kv_driver: ./interface/mainfiles/NET_main.c libdriver.a libfdsock.a
	$(CC) $(CFLAGS) -o $@ $^ $(ARCH) $(LIBS)

tc_driver: ./interface/mainfiles/trace_collect_main.c libdriver.a libfdsock.a
	$(CC) $(CFLAGS) -o $@ $^ $(ARCH) $(LIBS)

tr_driver: ./interface/mainfiles/Ytest_main.c libdriver.a libfdsock.a
	$(CC) $(CFLAGS) -o $@ $^ $(ARCH) $(LIBS)

range_driver: ./interface/range_test_main.c libdriver.a
	$(CC) $(CFLAGS) -o $@ $^ $(ARCH) $(LIBS)

duma_driver: ./interface/main.c libdriver.a
	$(CC) $(CFLAGS) -o $@ $^ -lduma $(ARCH) $(LIBS)

jni: libdriver.a ./jni/DriverInterface.c
	$(CC) -fPIC -o libdriver.so -shared -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux ./object/* $(LIBS)
	
libfdsock.a:
	cd ./include/flash_sock/ && $(MAKE) libfdsock.a && mv ./libfdsock.a ../../ && cd ../../

libdriver.a: $(TARGETOBJ)
	mkdir -p object && mkdir -p data
	cd ./blockmanager/$(TARGET_BM) && $(MAKE) && cd ../../
	cd ./algorithm/$(TARGET_ALGO) && $(MAKE) clean && $(MAKE) && cd ../../
	cd ./lower/$(TARGET_LOWER) && $(MAKE) && cd ../../ 
	mv ./include/data_struct/*.o ./object/
	mv -f ./blockmanager/*.o ./object/
	mv ./include/utils/*.o ./object/
	mv ./interface/*.o ./object/ && mv ./bench/*.o ./object/ && mv ./include/*.o ./object/
	$(AR) r $(@) ./object/*

%_mem.o: %.c
	$(CC) $(CFLAGS) -DLEAKCHECK -c $< -o $@ $(LIBS)

%_d.o: %.c
	$(CC) $(CFLAGS) -DDEBUG -c $< -o $@ $(LIBS)

.c.o :
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

submodule:libdriver.a
	mv libdriver.a ../objects/

clean :
	cd ./algorithm/$(TARGET_ALGO) && $(MAKE) clean && cd ../../
	cd ./lower/$(TARGET_LOWER) && $(MAKE) clean && cd ../../
	@$(RM) ./data/*
	@$(RM) ./object/*.o
	@$(RM) ./object/*.a
	@$(RM) *.a
	@$(RM) driver_memory_check
	@$(RM) debug_driver
	@$(RM) duma_driver
	@$(RM) range_driver
	@$(RM) *driver
	@$(RM) bd_testcase
	@$(RM) libdriver.so

