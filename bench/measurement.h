#ifndef __MEASURE_H__
#define __MEASURE_H__
#include<sys/time.h>
#include "../include/settings.h"
typedef struct linktime{
	struct timeval start,end;
	struct linktime * next;
}linktime;

typedef struct MeasureTime{
	bool isadding;
	linktime *header;
	struct timeval adding;
	uint64_t micro_time;
	uint64_t max;
	bool isused;
	int call;
	int cnt;
}MeasureTime;


void measure_init(MeasureTime *);
void measure_start(MeasureTime *);
void measure_pop(MeasureTime *);
void measure_stamp(MeasureTime *);
void measure_adding(MeasureTime *);
void measure_calc_max(MeasureTime *);
void measure_adding_print(MeasureTime *m);
struct timeval measure_res(MeasureTime *);
static inline void print_adding_result(const char* front,MeasureTime *m, const char *behind){
	if(!m->isadding){
		printf("should time is a adding type\n");
		return;
	}
	if(front){
		printf("%s",front);
	}
	uint64_t total_micro_time=m->adding.tv_sec*1000000+m->adding.tv_usec;
	printf("avg:%.2f total:%lu cnt:%u",(double)total_micro_time/m->cnt,
			total_micro_time, m->cnt);
	if(behind){
		printf("%s",behind);
	}
}
#ifdef DCPP
#include<string>
void measure_end(MeasureTime *,std::string);
#else
void measure_end(MeasureTime *,const char *);
#endif
void measure_calc(MeasureTime *);
void donothing(MeasureTime *t);
void donothing2(MeasureTime *t,char *a);
#endif
