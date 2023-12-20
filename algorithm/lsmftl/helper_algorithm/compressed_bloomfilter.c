#include "compressed_bloomfilter.h"

static inline uint32_t hashfunction(uint32_t key){
	key ^= key >> 15;
	key *= UINT32_C(0x2c1b3c6d);
	key ^= key >> 12;
	key *= UINT32_C(0x297a2d39);
	key ^= key >> 15;
	
	key = ~key + (key << 15); // key = (key << 15) - key - 1;
	key = key ^ (key >> 12);
	key = key + (key << 2);
	key = key ^ (key >> 4);
	key = key * 2057; // key = (key + (key << 3)) + (key << 11);
	key = key ^ (key >> 16);
	return key;
}

c_bf *cbf_init(uint32_t bits){
	c_bf *res=(c_bf*)calloc(1,sizeof(c_bf));
	res->bits=bits;
	return res;
}

void cbf_init_mem(c_bf *cbf, uint32_t bits){
	cbf->bits=bits;
}

void cbf_put_lba(c_bf *cbf, uint32_t lba){
	cbf->compressed_data =(hashfunction(lba)&((1<<(cbf->bits+1))-1));
}

bool cbf_check_lba(c_bf *cbf, uint32_t lba){
	return cbf->compressed_data == (hashfunction(lba)&((1<<(cbf->bits+1))-1));
}
