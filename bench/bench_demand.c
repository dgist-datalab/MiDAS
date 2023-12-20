#include "bench.h"
#include "../include/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern int32_t write_stop;
extern master *_master;
extern bool last_ack;
extern int KEYLENGTH;
extern int VALUESIZE;

bench_value *bench_make_ondemand();
bench_value *get_bench_ondemand(){
	bench_value *res=NULL;
	monitor * _m=&_master->m[_master->n_num];
	if(_m->n_num && _m->n_num==_m->m_num){
		while(!bench_is_finish_n(_master->n_num)){
			write_stop = false;
        }
		printf("\rtesting...... [100%%] done!\n");
		printf("\n");
        //sleep(5);

		free(_m->dbody);
		_master->n_num++;
		if(_master->n_num==_master->m_num)
			return NULL;
		_m=&_master->m[_master->n_num];
	}


	if(_m->m_num<100){
		float body=_m->m_num;
		float head=_m->n_num;
		printf("\r testing.....[%f%%]",head/body*100);
	}
	else if(_m->n_num%(_m->m_num<100?_m->m_num:PRINTPER*(_m->m_num/10000))==0){
#ifdef PROGRESS
		printf("\r testing...... [%.2lf%%]",(double)(_m->n_num)/(_m->m_num/100));
		fflush(stdout);
#endif
	}

    if (_m->n_num == _m->m_num -1) {
        last_ack = true;
    }

	res=bench_make_ondemand();
	_m->n_num++;
	return res;
}

bench_value *bench_make_ondemand(){
	int idx=_master->n_num;
	bench_meta *_meta=&_master->meta[idx];
	monitor * _m=&_master->m[idx];
	if(!_m->m_num){
		_m->n_num=0;
		_m->r_num=0;
		_m->empty=false;
		_m->m_num=_meta->number;
		_m->ondemand=true;
		_m->type=_meta->type;
		_m->dbody=(bench_value**)malloc(sizeof(bench_value*)*_meta->number);
		measure_init(&_m->benchTime);
		MS(&_m->benchTime);
	}
	
	bench_value *res=(bench_value*)malloc(sizeof(bench_value));
	_m->dbody[_m->n_num]=res;
	uint32_t start=_meta->start;
	uint32_t end=_meta->end;
	uint32_t t_k=0;
	switch(_meta->type){
		case FILLRAND:
			abort();
			break;
		case SEQSET:
				res->type=FS_SET_T;
				res->length=LPAGESIZE;
				t_k=start+(_m->n_num%(end-start));
				break;
		case SEQGET:
				res->type=FS_GET_T;
				res->length=LPAGESIZE;
				t_k=start+(_m->n_num%(end-start));
				break;
		case RANDSET:
				res->type=FS_SET_T;
				res->length=LPAGESIZE;
				t_k=start+rand()%(end-start);
				break;
		case RANDGET:
				res->type=FS_GET_T;
				res->length=LPAGESIZE;
				t_k=start+rand()%(end-start);
				break;

		default:
				break;
	}
	res->mark=idx;
#ifdef KVSSD
	res->key.len=my_itoa(t_k,&res->key.key);
#else
	res->key=t_k;
#endif
	return res;
}
