#ifndef __H_THREADING__
#define __H_THREADING__
#include"interface.h"
#include"queue.h"
#include"../include/data_struct/redblack.h"
#include<pthread.h>
typedef struct master_processor master_processor;
typedef struct processor{
	pthread_t t_id;
	pthread_mutex_t flag;
	master_processor *master;
	queue *req_q; //for write req in priority
	queue *retry_q;
#ifdef interface_pq
	queue *req_rq; //for read req
	Redblack qmanager;
	pthread_mutex_t qm_lock;
#endif
}processor;

struct master_processor{
	bool _data_check_flag;
	processor *processors;
	pthread_mutex_t flag;
	lower_info *li;
	algorithm *algo;
	blockmanager *bm;
	bool stopflag;
};
#endif
