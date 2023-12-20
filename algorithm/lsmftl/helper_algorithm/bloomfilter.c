#include"bloomfilter.h"
#include"../../../bench/bench.h"
//#include "../../include/sha256-arm.h"
#include<math.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#ifdef __GNUC__
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

extern int save_fd;

void BITSET(char *input, char offset){
	char test=1;
	test<<=offset;
	(*input)|=test;
}
bool BITGET(char input, char offset){
	char test=1;
	test<<=offset;
	return input&test;
}

static FORCE_INLINE uint32_t rotl32 ( uint32_t x, int8_t r )
{
	return (x << r) | (x >> (32 - r));
}

static FORCE_INLINE uint64_t rotl64 ( uint64_t x, int8_t r )
{
	return (x << r) | (x >> (64 - r));
}

#define	ROTL32(x,y)	rotl32(x,y)
#define ROTL64(x,y)	rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

//-----------------------------------------------------------------------------
//// Block read - if your platform needs to do endian-swapping or can only
//// handle aligned reads, do the conversion here
//
#define getblock(p, i) (p[i])
//

static FORCE_INLINE uint32_t fmix32 ( uint32_t h )
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}
static FORCE_INLINE uint64_t fmix64 ( uint64_t k )
{
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xff51afd7ed558ccd);
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
	k ^= k >> 33;

	return k;
}

void MurmurHash3_x86_32( const void * key, int len,uint32_t seed, void * out )
{
	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 4;
	int i;

	uint32_t h1 = seed;

	uint32_t c1 = 0xcc9e2d51;
	uint32_t c2 = 0x1b873593;
	const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

	for(i = -nblocks; i; i++)
	{
		uint32_t k1 = getblock(blocks,i);

		k1 *= c1;
		k1 = ROTL32(k1,15);
		k1 *= c2;

		h1 ^= k1;
		h1 = ROTL32(h1,13); 
		h1 = h1*5+0xe6546b64;
	}
	const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

	uint32_t k1 = 0;

	switch(len & 3)
	{
		case 3: k1 ^= tail[2] << 16;
		case 2: k1 ^= tail[1] << 8;
		case 1: k1 ^= tail[0];
				k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
	}; h1 ^= len;

	h1 = fmix32(h1);

	*(uint32_t*)out = h1;
} 

//djb2
static inline uint32_t simple_hash(char *key, uint8_t len){
	uint32_t hash=5361;
	for(int i=0; i<len ;i++){
		hash = ((hash << 5) + hash) + key[i];
	}
	return hash;
}

static inline uint32_t hashfunction(uint32_t key){
	key ^= key >> 15;
	key *= UINT32_C(0x2c1b3c6d);
	key ^= key >> 12;
	key *= UINT32_C(0x297a2d39);
	key ^= key >> 15;
	/*
	key = ~key + (key << 15); // key = (key << 15) - key - 1;
	key = key ^ (key >> 12);
	key = key + (key << 2);
	key = key ^ (key >> 4);
	key = key * 2057; // key = (key + (key << 3)) + (key << 11);
	key = key ^ (key >> 16);*/
	return key;
}

BF* bf_init(int entry, float fpr){
	if(fpr>1 || fpr==0)
		return NULL;
	BF *res=(BF*)malloc(sizeof(BF));
	res->n=entry;
	res->m=ceil((res->n * log(fpr)) / log(1.0 / (pow(2.0, log(2.0)))));
	res->k=round(log(2.0) * (float)res->m / res->n);
	int targetsize=res->m/8;
	if(res->m%8)
		targetsize++;
	res->body=(char*)malloc(targetsize);
	memset(res->body,0,targetsize);
	res->p=fpr;
	res->targetsize=targetsize;
	return res;
}

float bf_fpr_from_memory(int entry, uint32_t memory){
	int n=entry;
	int m=memory*8;
	int k=round(log(2.0)*(double)m/n);
	float p=pow(1 - exp((double)-k / (m / n)), k);
	return p;
}

BF* bf_cpy(BF *src){
	if(src==NULL) return NULL;
	BF* res=(BF*)malloc(sizeof(BF));
	memcpy(res,src,sizeof(BF));
	res->body=(char *)malloc(res->targetsize);
	memcpy(res->body,src->body,res->targetsize);
	return res;
}

uint64_t bf_bits(int entry, float fpr){
	if(fpr>1) return 0;
	uint64_t n=entry;
	uint64_t m=ceil((n * log(fpr)) / log(1.0 / (pow(2.0, log(2.0)))));
	int targetsize=m/8;
	if(m%8)
		targetsize++;
	return targetsize;
}

extern MeasureTime write_opt_time[10];
void bf_put(BF *input, uint32_t key){
	if(input==NULL){
		abort();
	}
	uint32_t h,th;
	int block;
	int offset;
	//printf("%u:",key);
#if defined(KVSSD)
	//th=simple_hash(key.key,key.len);
	MurmurHash3_x86_32(key.key,key.len,2,&th);
	//th=sha256_calculate(key.key);
#endif

	for(uint32_t i=0; i<input->k; i++){
#if defined(KVSSD) && defined(Lsmtree)
		h=hashfunction(th | (i<<7));
#else
		h=hashfunction((key<<19) | (i<<7));
#endif
		h%=input->m;
		block=h/8;
		offset=h%8;

		BITSET(&input->body[block],offset);
	}
}

bool bf_check(BF* input, uint32_t key){
	uint32_t h, th;
	int block,offset;
	if(input==NULL) return true;
	bench_custom_start(write_opt_time,6);
#if defined(KVSSD)
	 MurmurHash3_x86_32(key.key,key.len,2,&th);
#endif

	for(uint32_t i=0; i<input->k; i++){
#if defined(KVSSD) && defined(Lsmtree)
		h=hashfunction(th | (i<<7));
#else
		h=hashfunction((key<<19) | (i<<7));
#endif
		h%=input->m;
		block=h/8;
		offset=h%8;

		if(!BITGET(input->body[block],offset)){
			bench_custom_A(write_opt_time,6);
			return false;
		}
	}
	bench_custom_A(write_opt_time,6);
	return true;
}
void bf_free(BF *input){
	if(!input) return;
	free(input->body);
	free(input);
}

float bf_fpr_from_memory_monkey(int entry, uint32_t memory,uint32_t level, float size_factor, float normal_fpr){
	int i;
	uint32_t calc_memory=0;
	float ffpr;
	uint32_t header_num,before;
	uint32_t tt=pow(size_factor,4);
	printf("##############%u %u %u\n",bf_bits(entry,normal_fpr)*tt,memory,tt);
retry:
	header_num=ceil(size_factor);
	ffpr=normal_fpr;
	calc_memory=bf_bits(entry,ffpr)*header_num;
	for(i=1; i<level; i++){
		ffpr*=size_factor;
		header_num=ceil(header_num*size_factor);
		if(ffpr>1) break;
		before=calc_memory;
		calc_memory+=bf_bits(entry,ffpr)*header_num;
		if(before>calc_memory){
			printf("over flow!\n");
		}
		if(calc_memory>memory) break;
	}

	if(memory<calc_memory){
		normal_fpr+=0.0000001;
		goto retry;
	}
	if(ffpr>1){
		printf("[%f, %f, %d] calc_memory :%u, memory: %u normal:%f\n",ffpr,ffpr/size_factor,i,calc_memory,memory,normal_fpr);
		normal_fpr/=size_factor*100;
		goto retry;
	}
	if(memory/100*95> calc_memory){
		normal_fpr+=0.0000001;
		goto retry;
	}
	
	printf("################## %u %u", calc_memory,memory);
	return normal_fpr;
}

