#ifndef __GLOBAL_RETURN_CODE_H__
#define __GLOBAL_RETURN_CODE_H__

typedef enum return_code{
	SUCCESSED=0,
	FAILED,
}return_code; 

static inline uint32_t decode_return_code_h(uint32_t return_code){return return_code>>16;}
static inline uint32_t decode_return_code_b(uint32_t return_code){return return_code&0xffff;}
static inline uint32_t encode_return_code(uint32_t global_code, uint32_t specific_code){
	return global_code<<16 | specific_code;
}
#endif
