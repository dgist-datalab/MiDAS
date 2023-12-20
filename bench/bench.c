#include "bench.h"
#include "../include/types.h"
#include "../include/settings.h"
#include "../include/utils/kvssd.h"
#include "../interface/interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
int32_t LOCALITY;
float TARGETRATIO;
float OVERLAP;
int KEYLENGTH;
int VALUESIZE;
bool last_ack;
int seq_padding_opt;
extern int32_t write_stop;

master *_master;
#ifndef KVSSD
void latency(uint32_t,uint32_t,monitor*);

void rand_latency(uint32_t start, uint32_t end,int percentage, monitor *m);
void seq_latency(uint32_t start, uint32_t end,int percentage, monitor *m);
#endif

uint32_t keygenerator(uint32_t range);
uint32_t keygenerator_type(uint32_t range, int type);
pthread_mutex_t bench_lock;
uint8_t *bitmap;
static void bitmap_set(uint32_t key){
	uint32_t block=key/8;
	uint8_t offset=key%8;

	bitmap[block]|=(1<<offset);
}
static bool bitmap_get(uint32_t key){
	uint32_t block=key/8;
	uint8_t offset=key%8;

	return bitmap[block]&(1<<offset);
}
void bench_init(){
	OVERLAP=0.0;
	_master=(master*)malloc(sizeof(master));
	_master->m=(monitor*)malloc(sizeof(monitor)*BENCHNUM);
	memset(_master->m,0,sizeof(monitor)*BENCHNUM);

	_master->meta=(bench_meta*)malloc(sizeof(bench_meta) * BENCHNUM);
	memset(_master->meta,0,sizeof(bench_meta)*BENCHNUM);

	_master->datas=(bench_data*)malloc(sizeof(bench_data) * BENCHNUM);
	memset(_master->datas,0,sizeof(bench_data)*BENCHNUM);

	_master->li=(lower_info*)malloc(sizeof(lower_info)*BENCHNUM);
	memset(_master->li,0,sizeof(lower_info)*BENCHNUM);

	_master->n_num=0; _master->m_num=0;
	pthread_mutex_init(&bench_lock,NULL);

	for(int i=0; i<BENCHNUM; i++){
		_master->m[i].empty=true;
		_master->m[i].type=NOR;
	}
	
	bitmap=(uint8_t*)malloc(sizeof(uint8_t)*(RANGE));
	_master->error_cnt=0;
}
void bench_make_data(){
	int idx=_master->n_num;
	bench_meta *_meta=&_master->meta[idx];
	bench_data *_d=&_master->datas[idx];
	monitor * _m=&_master->m[idx];
	_m->mark=idx;
	_m->bech=_meta->number/(BENCHSETSIZE-1)+(_meta->number%(BENCHSETSIZE-1)?1:0);
	_m->benchsetsize=(BENCHSETSIZE-1);
	if(_meta->type==NOR){
		printf("not fixed test!\n");
		return;
	}
	printf("%d X %d = %d, answer=%lu\n",_m->bech,_m->benchsetsize,_m->bech*_m->benchsetsize,_meta->number);
	if(_meta->type < VECTOREDRSET){
		for(uint32_t i=0; i<_m->benchsetsize; i++){
			_m->body[i]=(bench_value*)malloc(sizeof(bench_value)*_m->bech);
		}
	}

	_m->n_num=0;
	_m->r_num=0;
	_m->empty=false;
	_m->m_num=_meta->number;
	_m->type=_meta->type;
	uint32_t start=_meta->start;
	uint32_t end=_meta->end;
	switch(_meta->type){
		case SEQGET:
			seqget(start,end,_m);
			break;
		case SEQSET:
			seqset(start,end,_m);
			break;
		case RANDGET:
			randget(start,end,_m);
			break;
		case RANDRW:
			randrw(start,end,_m);
			break;
		case SEQRW:
			seqrw(start,end,_m);
			break;
		case RANDSET:
			randset(start,end,_m);
			break;
		case MIXED:
			mixed(start,end,50,_m);
			break;
		case FILLRAND:
			fillrand(start,end,_m);
			break;
		case VECTOREDUNIQRSET:
			vectored_unique_rset(start,end, _m);
			break;
		case VECTOREDSSET:
			vectored_set(start,end, _m, true);
			break;
		case VECTOREDRSET:
			vectored_set(start,end, _m, false);
			break;
		case VECTOREDRGET:
			vectored_get(start,end, _m, false);
			break;
		case VECTOREDSGET:
			vectored_get(start,end, _m, true);
			break;
		case VECTOREDRW:
			vectored_rw(start,end, _m, false);
			break;
#ifndef KVSSD
		case SEQLATENCY:
			seq_latency(start,end,50,_m);
			break;
		case RANDLATENCY:
			rand_latency(start,end,50,_m);
			break;
#endif
		default:
			printf("making data failed\n");
			break;
	}
	_d->read_cnt=_m->read_cnt;
	_d->write_cnt=_m->write_cnt;
	_m->m_num=_m->read_cnt+_m->write_cnt;
	measure_init(&_m->benchTime);
	MS(&_m->benchTime);
}

void bench_add(bench_type type, uint32_t start, uint32_t end, uint64_t number){
	static int idx=0;
	_master->meta[idx].start=start;
	_master->meta[idx].end=end;
	_master->meta[idx].type=type;
	_master->meta[idx].number=number%2?(number/2)*2:number;

	for(int j=0;j<ALGOTYPE;j++){
		for(int k=0;k<LOWERTYPE;k++){
			_master->datas[idx].ftl_poll[j][k].min = UINT64_MAX;
			//_master->datas[i].ftl_npoll[j][k].min = UINT64_MAX;
		}
	}
	if(type==NOR){
		bench_meta *_meta=&_master->meta[idx];
		monitor *_m=&_master->m[idx];
		_m->n_num=0;
		_m->r_num=0;
		_m->m_num=_meta->number;
		_m->type=_meta->type;
	}
	_master->m_num++;
#ifndef KVSSD
	//printf("bench range:%u ~ %u\n",start,end);
#endif
	idx++;
}

void bench_free(){
	free(_master->m);
	free(_master->meta);
	free(_master->datas);
	free(_master->li);
	free(_master);
}


bench_value* __get_bench(){
	monitor * _m=&_master->m[_master->n_num];
	if(_m->n_num==0){
		bench_make_data();

	}
	if(_m->n_num==_m->m_num){
		while(!bench_is_finish_n(_master->n_num)){
			write_stop = false;
        }
		printf("\rtesting...... [100%%] done!\n");
		printf("\n");
        //sleep(5);
		
		for(int i=0; i<BENCHSETSIZE; i++){
			free(_m->body[i]);
		}
		_master->n_num++;
		if(_master->n_num==_master->m_num)
			return NULL;
	
		int type=_master->meta[_master->n_num].type;
		switch(type){
			case SEQGET:
			case SEQSET:
			case RANDSET:
			case RANDGET:
				return get_bench_ondemand();
				break;
		}

		bench_make_data();
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
	
	
	bench_value *res=&_m->body[_m->n_num/_m->bech][_m->n_num%_m->bech];
	if(_m->n_num%_m->bech==0 && _m->n_num/_m->bech>1){
	//	printf("%d %p org:%p\n",_m->n_num/_m->bech-1,&_m->body[_m->n_num/_m->bech-1],_m->body);
		free(_m->body[_m->n_num/_m->bech-1]);
		_m->body[_m->n_num/_m->bech-1]=NULL;
	}
	_m->n_num++;
	return res;
}
bench_value* get_bench(){
	int idx=_master->n_num;
	bench_meta *_meta=&_master->meta[idx];
	switch(_meta->type){
		case SEQGET:
		case SEQSET:
		case RANDSET:
		case RANDGET:
			return get_bench_ondemand();
		default:
			return __get_bench();
	}
}
extern bool force_write_start;
bool bench_is_finish_n(volatile int n){

	if(_master->m[n].command_num){
		if(_master->m[n].command_num <= _master->m[n].command_return_num+2){
			if(_master->m[n].r_num==_master->m[n].m_num){
				return true;
			}
			else{
				return false;
			}
		}
		else{
			return false;
		}
	}

	if(_master->m[n].r_num==_master->m[n].m_num){
		_master->m[n].finish=true;
		return true;
	}
	if(n+1==_master->m_num){
	//	force_write_start=true;	
		write_stop=0;
	}
	return false;
}
bool bench_is_finish(){
	for(int i=0; i<_master->m_num; i++){
		if(!bench_is_finish_n(i)){
			return false;
		}
	}
	return true;
}
void bench_print(){
	bench_data *bdata=NULL;
	monitor *_m=NULL;
	for(int i=0; i<_master->m_num; i++){
		printf("\n\n");
		_m=&_master->m[i];
		bdata=&_master->datas[i];
#ifdef CDF
		bench_cdf_print(_m->m_num,_m->type,bdata);
#endif
		bench_type_cdf_print(bdata);
		
		//printf("--------------------------------------------\n");
		//printf("|            bench type:                   |\n");
		//printf("--------------------------------------------\n");
#ifdef BENCH

		//printf("----algorithm----\n");
		for(int j=0; j<10; j++){
		//	printf("[%d~%d(usec)]: %ld\n",j*100,(j+1)*100,bdata->algo_mic_per_u100[j]);
		}
		//printf("[over_ms]: %ld\n",bdata->algo_mic_per_u100[10]);
		//printf("[over__s]: %ld\n",bdata->algo_mic_per_u100[11]);


		//printf("----lower----\n");
		for(int j=0; j<10; j++){
		//	printf("[%d~%d(usec)]: %ld\n",j*100,(j+1)*100,bdata->lower_mic_per_u100[j]);
		}
		//printf("[over_ms]: %ld\n",bdata->lower_mic_per_u100[10]);
		//printf("[over__s]: %ld\n",bdata->lower_mic_per_u100[11]);

		//printf("----average----\n");
		uint64_t total=0,avg_sec=0, avg_usec=0;
		total=(bdata->algo_sec*1000000+bdata->algo_usec)/_m->m_num;
		avg_sec=total/1000000;
		avg_usec=total%1000000;
		//printf("[avg_algo]: %ld.%ld\n",avg_sec,avg_usec);

		total=(bdata->lower_sec*1000000+bdata->lower_usec)/_m->m_num;
		avg_sec=total/1000000;
		avg_usec=total%1000000;
		//printf("[avg_low]: %ld.%ld\n",avg_sec,avg_usec);
		//bench_li_print(&_master->li[i],_m);
#endif
		printf("\n----summary----\n");
		if(_m->type==RANDRW || _m->type==SEQRW || _m->type==VECTOREDRW){
			uint64_t total_data=(LPAGESIZE * _m->m_num/2)/1024;
			double total_time2=_m->benchTime.adding.tv_sec+(double)_m->benchTime.adding.tv_usec/1000000;
			double total_time1=_m->benchTime2.adding.tv_sec+(double)_m->benchTime2.adding.tv_usec/1000000;
			double throughput1=(double)total_data/total_time1;
			double throughput2=(double)total_data/total_time2;
			double sr=1-((double)_m->notfound/_m->m_num);
			throughput2*=sr;

			printf("[all_time1]: %ld.%ld\n",_m->benchTime.adding.tv_sec,_m->benchTime.adding.tv_usec);
			printf("[all_time2]: %ld.%ld\n",_m->benchTime2.adding.tv_sec,_m->benchTime2.adding.tv_usec);
			printf("[size]: %lf(mb)\n",(double)total_data/1024);

			printf("[FAIL NUM] %ld\n",_m->notfound);
			fprintf(stderr,"[SUCCESS RATIO] %lf\n",sr);
			fprintf(stderr,"[throughput1] %lf(kb/s)\n",throughput1);
			fprintf(stderr,"             %lf(mb/s)\n",throughput1/1024);
			fprintf(stderr,"[IOPS 1] %lf\n",_m->m_num/total_time1/2);
			fprintf(stderr,"[throughput2] %lf(kb/s)\n",throughput2);
			fprintf(stderr,"             %lf(mb/s)\n",throughput2/1024);
			fprintf(stderr,"[IOPS 2] %lf\n",_m->m_num/total_time2/2);
			fprintf(stderr,"[cache hit cnt,ratio] %ld, %lf\n",_m->cache_hit,(double)_m->cache_hit/(_m->m_num/2));
			printf("[READ WRITE CNT] %ld %ld\n",_m->read_cnt,_m->write_cnt);
		}
		else{
			_m->benchTime.adding.tv_sec+=_m->benchTime.adding.tv_usec/1000000;
			_m->benchTime.adding.tv_usec%=1000000;
			printf("[all_time]: %ld.%ld\n",_m->benchTime.adding.tv_sec,_m->benchTime.adding.tv_usec);
			uint64_t total_data=(LPAGESIZE * _m->m_num)/1024;
			printf("[size]: %lf(mb)\n",(double)total_data/1024);
			double total_time=_m->benchTime.adding.tv_sec+(double)_m->benchTime.adding.tv_usec/1000000;
			double throughput=(double)total_data/total_time;
			double sr=1-((double)_m->notfound/_m->m_num);
			throughput*=sr;
			printf("[FAIL NUM] %ld\n",_m->notfound);
			fprintf(stderr,"[SUCCESS RATIO] %lf\n",sr);
			fprintf(stderr,"[throughput] %lf(kb/s)\n",throughput);
			fprintf(stderr,"             %lf(mb/s)\n",throughput/1024);
			printf("[IOPS] %lf\n",_m->m_num/total_time);
			//if(_m->read_cnt){
			printf("[cache hit cnt,ratio] %ld, %lf\n",_m->cache_hit,(double)_m->cache_hit/(_m->read_cnt));
			printf("[cache hit cnt,ratio dftl] %ld, %lf\n",_m->cache_hit,(double)_m->cache_hit/(_m->read_cnt+_m->write_cnt));
			//}
			printf("[READ WRITE CNT] %ld %ld\n",_m->read_cnt,_m->write_cnt);
		}
		printf("error cnt:%d\n",_master->error_cnt);
	}
}

void bench_update_typetime(bench_data *_d, uint8_t a_type,uint8_t l_type, uint64_t time){
	bench_ftl_time *temp;
	temp = &_d->ftl_poll[a_type][l_type];
	temp->total_micro += time;
	temp->max = temp->max < time ? time : temp->max;
	temp->min = temp->min > time ? time : temp->min;
	temp->cnt++;
}

void bench_type_cdf_print(bench_data *_d){
	fprintf(stderr,"a_type\tl_type\tmax\tmin\tavg\tcnt\tpercentage\n");
	for(int i = 0; i < ALGOTYPE; i++){
		for(int j = 0; j < LOWERTYPE; j++){
			if(!_d->ftl_poll[i][j].cnt)
				continue;
			fprintf(stderr,"%d\t%d\t%lu\t%lu\t%.3f\t%lu\t%.5f%%\n",i,j,_d->ftl_poll[i][j].max,_d->ftl_poll[i][j].min,(float)_d->ftl_poll[i][j].total_micro/_d->ftl_poll[i][j].cnt,_d->ftl_poll[i][j].cnt,(float)_d->ftl_poll[i][j].cnt/_d->read_cnt*100);
		}
	}
}

void __bench_time_maker(MeasureTime mt, bench_data *datas,bool isalgo){
	uint64_t *target=NULL;
	if(isalgo){
		target=datas->algo_mic_per_u100;
		datas->algo_sec+=mt.adding.tv_sec;
		datas->algo_usec+=mt.adding.tv_usec;
	}
	else{
		target=datas->lower_mic_per_u100;
		datas->lower_sec+=mt.adding.tv_sec;
		datas->lower_usec+=mt.adding.tv_usec;
	}

	if(mt.adding.tv_sec!=0){
		target[11]++;
		return;
	}

	int idx=mt.adding.tv_usec/1000;
	if(idx>=10){
		target[10]++;
		return;
	}
	idx=mt.adding.tv_usec/1000;
	if(target){
		target[idx]++;
	}
	return;
}

#ifdef CDF
void bench_cdf_print(uint64_t nor, uint8_t type, bench_data *_d){//number of reqeusts
	uint64_t cumulate_number=0;
	if(type>RANDSET)
		nor/=2;
	/*
	if((type>RANDSET || type%2==1) || type==NOR){
		printf("\n[cdf]write---\n");
		for(int i=0; i<1000000/TIMESLOT+1; i++){
			cumulate_number+=_d->write_cdf[i];
			if(_d->write_cdf[i]==0) continue;
			printf("%d\t%ld\t%f\n",i * 10,_d->write_cdf[i],(float)cumulate_number/_d->write_cnt);
			if(nor==cumulate_number)
				break;
		}	
	} 
	
	static int cnt=0;
	cumulate_number=0;
	if((type>RANDSET || type%2==0)|| type==RANDGET || type==NOR || type==FILLRAND){
		printf("\n(%d)[cdf]read---\n",cnt++);
		for(int i=0; i<1000000/TIMESLOT+1; i++){
			cumulate_number+=_d->read_cdf[i];
			if(_d->read_cdf[i]==0) continue;
			//fprintf(stderr,"%d,%ld,%f\n",i * 10,_d->read_cdf[i],(float)cumulate_number/_d->read_cnt);	
			printf("%d,%ld,%f\n",i * 10,_d->read_cdf[i],(float)cumulate_number/_d->read_cnt);
			if(nor==cumulate_number)
				break;
		}
	}
	*/
	
}
#endif
void bench_reap_nostart(request *const req){
	pthread_mutex_lock(&bench_lock);
	int idx=req->mark;
	monitor *_m=&_master->m[idx];
	_m->r_num++;
	//static int cnt=0;
	pthread_mutex_unlock(&bench_lock);
}

void bench_reap_data(request *const req,lower_info *li){
	//for cdf
	measure_calc(&req->latency_checker);

	pthread_mutex_lock(&bench_lock);
	if(!req){ 
		pthread_mutex_unlock(&bench_lock);
		return;
	}
	int idx=req->mark;
	if(idx==-1){return;}
	monitor *_m=&_master->m[idx];
	bench_data *_data=&_master->datas[idx];

	if(req->type==FS_GET_T || req->type==FS_NOTFOUND_T){
		bench_update_typetime(_data, req->type_ftl, req->type_lower,req->latency_checker.micro_time);
	}
	
	if(req->type==FS_NOTFOUND_T){
		_master->error_cnt++;
	}
#ifdef CDF
	int slot_num=req->latency_checker.micro_time/TIMESLOT;
	if(req->type==FS_GET_T || req->type==FS_RANGEGET_T){
		if(slot_num>=1000000/TIMESLOT){
			_data->read_cdf[1000000/TIMESLOT]++;
		}
		else{
			_data->read_cdf[slot_num]++;
		}
		if(_m->r_num%1000000==0){
		//	bench_cdf_print(_m->r_num,_m->type,_data);
		}
	}
	else if(req->type==FS_SET_T){
		if (req->is_board) {
			if(slot_num>=1000000/TIMESLOT){
				_data->write_cdf[1000000/TIMESLOT]++;
			}
			else{
				_data->write_cdf[slot_num]++;
			}
		}
	}
#endif
	if(_m->empty){
		if(req->type==FS_GET_T || req->type==FS_RANGEGET_T){
			_m->read_cnt++;
			_data->read_cnt++;
		}
		else if(req->type==FS_SET_T){
			if (req->is_board) {
				_m->write_cnt++;
				_data->write_cnt++;
			}
		}
		_m->r_num++;
		_m->m_num++;
		pthread_mutex_unlock(&bench_lock);
		return;
	}

	if(_m->m_num==_m->r_num+1){
		_data->bench=_m->benchTime;
	}
	if(_m->r_num+1==_m->m_num/2 && (_m->type==SEQRW || _m->type==RANDRW || _m->type==VECTOREDRW)){
		//inf_wait_background();
		MA(&_m->benchTime);
		_m->benchTime2=_m->benchTime;
		measure_init(&_m->benchTime);
		MS(&_m->benchTime);
	}

	if(req->type==FS_NOTFOUND_T){
		_m->notfound++;
	}
	if(_m->m_num==_m->r_num+1){
#ifdef BENCH
		memcpy(&_master->li[idx],li,sizeof(lower_info));
		li->refresh(li);
#endif
		MA(&_m->benchTime);
	}
	if(_m->ondemand){
		free(_m->dbody[_m->r_num]);
	}
	_m->r_num++;
	pthread_mutex_unlock(&bench_lock);
}
void bench_li_print(lower_info* li,monitor *m){
	printf("-----lower_info----\n");
	printf("[write_op]: %ld\n",li->write_op);
	printf("[read_op]: %ld\n",li->read_op);
	printf("[trim_op]:%ld\n",li->trim_op);
	printf("[WAF, RAF]: %lf, %lf\n",(float)li->write_op/m->m_num,(float)li->read_op/m->m_num);
	printf("[if rw test]: %lf(WAF), %lf(RAF)\n",(float)li->write_op/(m->m_num/2),(float)li->read_op/(m->m_num/2));
}

uint32_t keygenerator(uint32_t range){
	if(rand()%100<LOCALITY){
		return rand()%(uint32_t)(range*TARGETRATIO);
	}
	else{
		return (rand()%(uint32_t)(range*(1-TARGETRATIO)))+(uint32_t)RANGE*TARGETRATIO;
	}
}

uint32_t keygenerator_type(uint32_t range,int type){
	uint32_t write_shift=(uint32_t)(range*TARGETRATIO*(1.0f-OVERLAP));
	uint32_t res=0;
	if(rand()%100<LOCALITY){
		res=rand()%(uint32_t)(range*TARGETRATIO)+(type==FS_SET_T?write_shift:0);
	}else{
		res=(rand()%(uint32_t)(range*(1-TARGETRATIO)))+(uint32_t)RANGE*TARGETRATIO;
	}
	return res;
}
#ifdef KVSSD

int my_itoa(uint32_t key, char **_target){
	int cnt=1;
	int standard=10;
	int t_key=key;
	while(t_key/10){
		cnt++;
		t_key/=10;
		standard*=10;
	}
	int result=KEYLENGTH==-1?rand()%16+1:KEYLENGTH;
	result*=16;
	result-=sizeof(ppa_t);
	*_target=(char*)malloc(result);
	char *target=*_target;
	t_key=key;

	target[0]='u';
	target[1]='s';
	target[2]='e';
	target[3]='r';
	/*
	for(int i=cnt-1+4; i>=4; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	for(int i=cnt+4;i<result; i++){
		target[i]='0';
	}*/
	

	
	for(int i=4; i<result-cnt; i++){
		target[i]='0';
	}
	for(int i=result-1; i>=result-cnt; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	//printf("origin:%d\n",key);
	//printf("%d %s(%d)\n",result,target,strlen(target));
	return result;
}
/*
int my_itoa(uint32_t key, char **_target){
	int cnt=1;
	int standard=10;
	int t_key=key;
	while(t_key/10){
		cnt++;
		t_key/=10;
		standard*=10;
	}
	*_target=(char*)malloc(cnt+1);
	char *target=*_target;
	t_key=key;
	for(int i=cnt-1; i>=0; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	target[cnt]='\0';
	return cnt;
}*/

int my_itoa_padding(uint32_t key, char **_target,int digit){
	int cnt=1;
	int standard=10;
	int t_key=key;
	while(t_key/10){
		cnt++;
		t_key/=10;
		standard*=10;
	}

	*_target=(char*)malloc(digit+1);
	char *target=*_target;
	t_key=key;
	for(int i=0; i<digit-cnt-1; i++){
		target[i]='0';
	}
	for(int i=digit-1; i>=digit-cnt-1; i--){
		target[i]=t_key%10+'0';
		t_key/=10;
	}
	target[digit]='\0';
	return digit;
}
#endif
void seqget(uint32_t start, uint32_t end,monitor *m){
	printf("making seq Get bench!\n");
	for(uint32_t i=0; i<m->m_num; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
#else
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
#endif
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
		m->body[i/m->bech][i%m->bech].type=FS_GET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->read_cnt++;
	}
}

void seqset(uint32_t start, uint32_t end,monitor *m){
	printf("making seq Set bench!\n");
	for(uint32_t i=0; i<m->m_num; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
		bitmap_set(start+(i%(end-start)));
#else
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;
	}
}

void seqrw(uint32_t start, uint32_t end, monitor *m){
	printf("making seq Set and Get bench!\n");
	uint32_t i=0;
	for(i=0; i<m->m_num/2; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
		bitmap_set(start+(i%(end-start)));
#else
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else	
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;

#ifdef KVSSD
		t=&m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].key;
		if(seq_padding_opt)t->len=my_itoa_padding(start+(i%(end-start)),&t->key,10);
		else t->len=my_itoa(start+(i%(end-start)),&t->key);
#else
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].key=start+(i%(end-start));
#endif
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].type=FS_GET_T;
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].length=PAGESIZE;
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].mark=m->mark;
		m->read_cnt++;
	}
}

void randget(uint32_t start, uint32_t end,monitor *m){
	printf("making rand Get bench!\n");
	srand(1);
	for(uint32_t i=0; i<m->m_num; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		t->len=my_itoa(start+rand()%(end-start),&t->key);
#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
		m->body[i/m->bech][i%m->bech].type=FS_GET_T;
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->read_cnt++;
	}
}

void randset(uint32_t start, uint32_t end, monitor *m){
	printf("making rand Set bench!\n");
	for(uint32_t i=0; i<m->m_num; i++){
#ifdef KVSSD
		uint32_t t_k;
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
#ifdef KEYGEN
		t_k=keygenerator(end);
#else
		t_k=start+rand()%(end-start);
#endif
		t->len=my_itoa(t_k,&t->key);
		bitmap_set(t_k);
#else
#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif

		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->write_cnt++;
	}
}

void randrw(uint32_t start, uint32_t end, monitor *m){
	printf("making rand Set and Get bench!\n");
	for(uint32_t i=0; i<m->m_num/2; i++){
#ifdef KVSSD
		uint32_t t_k;
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
	#ifdef KEYGEN
		t_k=keygenerator(end);	
	#else
		t_k=start+rand()%(end-start);
	#endif
		bitmap_set(t_k);
		t->len=my_itoa(t_k,&t->key);
#else
	#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
	#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
	#endif
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif

		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
		if(m->body[i/m->bech][i%m->bech].length>PAGESIZE){
			abort();
		}

//		m->body[i/m->bech][i%m->bech].length=0;
//		m->body[i/m->bech][i%m->bech].length=PAGESIZE-PIECE;
#else	
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;

#ifdef KVSSD
		t=&m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].key;
		t->len=my_itoa(t_k,&t->key);
#else
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].key=m->body[i/m->bech][i%m->bech].key;
#endif
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].type=FS_GET_T;
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].length=PAGESIZE;
		m->body[(i+m->m_num/2)/m->bech][(i+m->m_num/2)%m->bech].mark=m->mark;
		m->read_cnt++;
	}
	printf("last set:%lu\n",(m->m_num-1)/m->bech);
}
void mixed(uint32_t start, uint32_t end,int percentage, monitor *m){
	printf("making mixed bench!\n");
	for(uint32_t i=0; i<m->m_num; i++){
#ifdef KVSSD
		uint32_t t_k;
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
	#ifdef KEYGEN
		t_k=keygenerator(end);	
	#else
		t_k=start+rand()%(end-start);
	#endif
		t->len=my_itoa(t_k,&t->key);
#else
	#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
	#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
	#endif

#endif

		if(rand()%100<percentage){
			m->body[i/m->bech][i%m->bech].type=FS_SET_T;
#ifdef KVSSD
			bitmap_set(t_k);
#else
			bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif
#ifdef DVALUE
			m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
			m->write_cnt++;
		}
		else{
			m->body[i/m->bech][i%m->bech].type=FS_GET_T;
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
			m->read_cnt++;
		}
		m->body[i/m->bech][i%m->bech].mark=m->mark;
	}
}

#ifndef KVSSD
void seq_latency(uint32_t start, uint32_t end,int percentage, monitor *m){
	printf("making latency bench!\n");
	//seqset process
	for(uint32_t i=0; i<m->m_num/2; i++){
		m->body[i/m->bech][i%m->bech].key=start+(i%(end-start));
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;
	}

	for(uint32_t i=m->m_num/2; i<m->m_num; i++){
#ifdef KEYGEN
		m->body[i/m->bech][i%m->bech].key=keygenerator(end);
#else
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
		if(rand()%100<percentage){
			m->body[i/m->bech][i%m->bech].type=FS_SET_T;
			bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
			m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
			m->write_cnt++;
		}
		else{
			while(!bitmap_get(m->body[i/m->bech][i%m->bech].key)){
#ifdef KEYGEN
				m->body[i/m->bech][i%m->bech].key=keygenerator(end);
#else
				m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			}
			m->body[i/m->bech][i%m->bech].type=FS_GET_T;
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
			m->read_cnt++;
		}
		m->body[i/m->bech][i%m->bech].mark=m->mark;
	}
}


void rand_latency(uint32_t start, uint32_t end,int percentage, monitor *m){
	printf("making latency bench!\n");
	for(uint32_t i=0; i<0; i++){
		m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->write_cnt++;
	}

	for(uint32_t i=0; i<m->m_num; i++){
		if(rand()%100<percentage){
#ifdef KEYGEN
			m->body[i/m->bech][i%m->bech].key=keygenerator_type(end,FS_SET_T);
#else
			m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			m->body[i/m->bech][i%m->bech].type=FS_SET_T;
			bitmap_set(m->body[i/m->bech][i%m->bech].key);
#ifdef DVALUE
			m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
			m->write_cnt++;
		}
		else{
#ifdef KEYGEN
			m->body[i/m->bech][i%m->bech].key=keygenerator_type(end,FS_GET_T);
#else
			m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			while(!bitmap_get(m->body[i/m->bech][i%m->bech].key)){
#ifdef KEYGEN
				m->body[i/m->bech][i%m->bech].key=keygenerator_type(end,FS_GET_T);
#else
				m->body[i/m->bech][i%m->bech].key=start+rand()%(end-start);
#endif
			}
			m->body[i/m->bech][i%m->bech].type=FS_GET_T;
			m->body[i/m->bech][i%m->bech].length=PAGESIZE;
			m->read_cnt++;
		}
		m->body[i/m->bech][i%m->bech].mark=m->mark;
	}
}
#endif

void fillrand(uint32_t start, uint32_t end, monitor *m){
	printf("making fillrand Set bench!\n");
	uint32_t* unique_array=(uint32_t*)malloc(sizeof(uint32_t)*(end-start+2));
	for(uint32_t i=start; i<=end; i++ ){
		unique_array[i]=i;
	}
	
	uint32_t range=end-start+1;
	for(uint32_t i=start; i<=end; i++){
		int a=rand()%range;
		int b=rand()%range;
		uint32_t temp=unique_array[a];
		unique_array[a]=unique_array[b];
		unique_array[b]=temp;
	}

	for(uint32_t i=0; i<m->m_num; i++){
#ifdef KVSSD
		KEYT *t=&m->body[i/m->bech][i%m->bech].key;
		t->len=my_itoa(unique_array[i%range],&t->key);
		bitmap_set(unique_array[i%range]);
#else
		m->body[i/m->bech][i%m->bech].key=unique_array[i%range];
		bitmap_set(m->body[i/m->bech][i%m->bech].key);
#endif

#ifdef DVALUE
		m->body[i/m->bech][i%m->bech].length=GET_VALUE_SIZE;
#else	
		m->body[i/m->bech][i%m->bech].length=PAGESIZE;
#endif
		m->body[i/m->bech][i%m->bech].mark=m->mark;
		m->body[i/m->bech][i%m->bech].type=FS_SET_T;
		m->write_cnt++;
	}
	free(unique_array);
}

void bench_cache_hit(int mark){
	monitor *_m=&_master->m[mark];
	_m->cache_hit++;
}


char *bench_lower_type(int a){
	switch(a){
		case 0:return "TRIM";
		case 1:return "MAPPINGR";
		case 2:return "MAPPINGW";
		case 3:return "GCMR";
		case 4:return "GCMW";
		case 5:return "DATAR";
		case 6:return "DATAW";
		case 7:return "GCDR";
		case 8:return "GCDW";
		case 9:return "GCMR_DGC";
		case 10:return "GCMW_DGC";
		case 11:return "COMPACTIONDATA-R";
		case 12:return "COMPACTIONDATA-W";
		case 13:return "MISSDATAR";
		case 14:return "TESTIO";
	}
	return NULL;
}

void bench_custom_init(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	for(int i=0; i<idx; i++){
		measure_init(&mt[i]);
	}
#endif
}

void bench_custom_start(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	MS(&mt[idx]);
#endif
}

void bench_custom_A(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	MA(&mt[idx]);
#endif
}

void bench_custom_print(MeasureTime *mt,int idx){
#ifdef CHECKINGTIME
	for(int i=0; i<idx; i++){
		printf("%d:",i);
		measure_adding_print(&mt[i]);
	}
#endif
}
