#ifndef __H_SETTING__
#define __H_SETTING__
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#include<stdint.h>
#include <stdlib.h>
#include<stdio.h>
#include <string.h>
#include <execinfo.h>

/*
#define free(a) \
	do{\
		printf("%s %d:%p\n",__FILE__,__LINE__,a);\
		free(a)\
	}while(0)
*/
#define PROGRESS
//#define LOWER_FILE_NAME "../iotest/simulator.data"
#define LOWER_FILE_NAME "./data/simulator.data"
//#define LOWER_FILE_NAME "/dev/sdb1"

//#define LOWER_FILE_NAME "/dev/robusta"
#define BENCH_LOG "./result/"
#define CACHING_RATIO 1

#ifdef DEBUG
#define DPRINTF(fmt, ...) \
	do{\
		printf(fmt, __VA_ARGS__); \
	}while(0)
#else
#define DPRINTF(fmt, ...)\
	do{}while(0)
#endif

#define K (1024)
#define M (1024*K)
#define G (1024*M)
#define T (1024L*G)
#define P (1024L*T)
#define MILI (1000000)

#ifdef MLC
#define TOTALSIZE (300L*G)
#define REALSIZE (512L*G)
#define PAGESIZE (16*K)
#define _PPB (256)
#define BPS (64)
#define _PPS (_PPB*BPS)

#elif defined(SLC)

//#define GIGAUNIT 128L
#define OP 100
#define SHOWINGSIZE (GIGAUNIT * G)
#define TOTALSIZE (SHOWINGSIZE + (SHOWINGSIZE/100*(100-OP)))
#define REALSIZE (512L*G)
#define PAGE_TARGET_KILO (16)
#define PAGESIZE (PAGE_TARGET_KILO*K)
#define _PPB (256*8/PAGE_TARGET_KILO)

//#ifdef AMF
	#define NOC 2
//	#define BPS (128)
//	#define _PPS (_PPB*BPS)
	#define BPS (_PPS/_PPB)
//#else
//	#define BPS (64)
//	#define _PPS (_PPB*BPS)
//#endif

#define PUNIT (64)
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)<(b)?(b):(a))


#define interface_vector
#define LPAGESIZE 4096
#define L2PGAP (PAGESIZE/LPAGESIZE)
#define BLOCKSIZE (_PPB*PAGESIZE)
#define _NOS (TOTALSIZE/(_PPS*PAGESIZE))
#define _NOP (_NOS*_PPS)
#define _NOB (BPS*_NOS)
#define _RNOS (REALSIZE/(_PPS*PAGESIZE))//real number of segment
#define LBANUM (SHOWINGSIZE/LPAGESIZE)

#define RANGE (SHOWINGSIZE/LPAGESIZE)
#define DEVFULL ((uint32_t)TOTALSIZE/LPAGESIZE)
#define TOTALLPN ((uint32_t)RANGE)

#define PARTNUM 2
#define MAPPING_TOTAL_NUM (RANGE/(PAGESIZE/sizeof(uint32_t)))
#define MAPPING_TOTAL_SEGS (((MAPPING_TOTAL_NUM/_PPS)+(MAPPING_TOTAL_NUM%_PPS?1:0)))
#define MAPPART_SEGS (MAPPING_TOTAL_SEGS+MAPPING_TOTAL_SEGS/100*(100-OP)+2)
#define DATAPART_SEGS (_NOS-MAPPART_SEGS)
enum{
	MAP_S,DATA_S
};

#ifdef DVALUE
	#define MAXKEYNUMBER (TOTALSIZE/PIECE)
#endif

#define SIMULATION 0

#define PFTLMEMORY (TOTALSIZE/K)

#define FSTYPE uint8_t
#define ppa_t uint32_t
#ifdef KVSSD
#define KEYFORMAT(input) input.len>DEFKEYLENGTH?DEFKEYLENGTH:input.len,input.key
#include<string.h>
typedef struct str_key{
	uint8_t len;
	char *key;
}str_key;

#define KEYT str_key
static inline int KEYCMP(KEYT a,KEYT b){
	if(!a.len && !b.len) return 0;
	else if(a.len==0) return -1;
	else if(b.len==0) return 1;

	int r=memcmp(a.key,b.key,a.len>b.len?b.len:a.len);
	if(r!=0 || a.len==b.len){
		return r;
	}
	return a.len<b.len?-1:1;
}

static inline int KEYCONSTCOMP(KEYT a, char *s){
	int len=strlen(s);
	if(!a.len && !len) return 0;
	else if(a.len==0) return -1;
	else if(len==0) return 1;

	int r=memcmp(a.key,s,a.len>len?len:a.len);
	if(r!=0 || a.len==len){
		return r;
	}
	return a.len<len?-1:1;
}

static inline char KEYTEST(KEYT a, KEYT b){
	if(a.len != b.len) return 0;
	int alen=a.len, blen=b.len;
	return memcmp(a.key,b.key,alen>blen?blen:alen)?0:1;
}

static inline bool KEYVALCHECK(KEYT a){
	if(a.len<=0)
		return false;
	if(a.key[0]<0)
		return false;
	return true;
}
#else
	#define KEYT uint32_t
#endif
#define BLOCKT uint32_t
#define V_PTR char * const
#define PTR char*
#define ASYNC 1
#define QSIZE (512)
#define LOWQDEPTH (128)
#define QDEPTH (512)

#define THPOOL
#define NUM_THREAD 4

#define TCP 1
//#define IP "10.42.0.2"
#define IP "127.0.0.1"
//#define IP "10.42.0.1"
//#define IP "192.168.0.7"
#define PORT 7777
#define NETWORKSET
#define DATATRANS

//#define KEYGEN
#define SPINSYNC
//#define BUSE_MEASURE
//#define BUSE_ASYNC 0

#define EPRINT(error, isabort)\
	do{\
		printf("[%s:%d]-%s\n", __FILE__,__LINE__, (error));\
		if((isabort)){abort();}\
	}while(0)


static inline void print_stacktrace()
{
	int size = 16;
	void * array[16];
	int stack_num = backtrace(array, size);
	char ** stacktrace = backtrace_symbols(array, stack_num);
	for (int i = 0; i < stack_num; ++i)
	{
		printf("%s\n", stacktrace[i]);
	}
	free(stacktrace);
}

#ifndef __GNUG__
typedef enum{false,true} bool;
#endif

#endif
