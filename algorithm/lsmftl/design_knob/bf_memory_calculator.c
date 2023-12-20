#include "bf_memory_calculator.h"
#include "../helper_algorithm/guard_bf_set.h"
extern float gbf_min_bit;
uint64_t bf_memory_calc(uint32_t entry_num, uint32_t error, bool wiskey){
	return gbf_min_bit*entry_num+(wiskey?entry_num*48:0);
}
