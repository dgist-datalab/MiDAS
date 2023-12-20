#ifndef __H_UTIL__
#define __H_UTIL__

#include "../bench/measurement.h"

#ifndef NPRINTOPTION
#define MT(t) measure_stamp((t))
#define MS(t) measure_start((t))
#define ME(t,s) measure_end((t),(s))
#define MP(t) measure_pop((t))
#define MC(t) measure_calc((t))
#define MR(t) measure_res((t))
#define MA(t) measure_adding((t))
#define MCM(t) measure_calc_max((t))
#else
#define MS(t) donothing(t)
#define ME(t,s) donothing2((t),(s))
#define MP(t) donothing((t))
#define MC(t) donothing((t))
#endif

#define DEBUG_LOG(c) ({\
		printf("[%s]:",c);\
		printf("%s:%d\n",__FILE__,__LINE__);})

#endif
