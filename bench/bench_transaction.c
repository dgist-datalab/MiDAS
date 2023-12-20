#include "bench.h"
#include "../include/settings.h"
extern master *_master;

#define REQSIZE (sizeof(uint8_t)+sizeof(uint8_t)+sizeof(uint32_t)+sizeof(uint32_t))
#define TXNHEADERSIZE (sizeof(uint32_t)+sizeof(uint32_t))
#define REQPERCOMMAND QSIZE
#if REQPERCOMMAND!=0
	#define MAXBUFSIZE (REQPERCOMMAND*REQSIZE+TXNHEADERSIZE)
#else
	#define MAXBUFSIZE (4*K)
#endif

extern int KEYLENGTH;

void bench_vectored_configure(){
	_master->trans_configure.request_size=REQSIZE;
	_master->trans_configure.request_num_per_command=(MAXBUFSIZE-TXNHEADERSIZE)/_master->trans_configure.request_size;
}

char *get_vectored_bench(uint32_t *mark){
	static int transaction_id=0;
	static int now_transaction_req_cnt=0;
	static uint64_t real_req_num=0;

	monitor *m=&_master->m[_master->n_num];

	if(m->command_num!=0 && m->command_issue_num==m->command_num){
		_master->n_num++;
		free(m->tbody);
		while(!bench_is_finish_n(_master->n_num)){}
		printf("\rtesting...... [100%%] done!\n");
		printf("\n");

		if(_master->n_num==_master->m_num) return NULL;

		m=&_master->m[_master->n_num];
	}

	if(m->command_issue_num==0){ //start bench mark
		bench_make_data();
		real_req_num=0;
	}

	*mark=_master->n_num;

#ifdef PROGRESS
	if(m->command_issue_num % (m->command_num/100)==0){
		printf("\r testing...... [%.2lf%%]",(double)(m->command_issue_num)/(m->command_num/100));
		fflush(stdout);
	}
#endif


	transaction_bench_value *res;
	res=&m->tbody[real_req_num++];
	if(!res->buf) return NULL;
	*(uint32_t*)&res->buf[0]=transaction_id;

	now_transaction_req_cnt++;

	m->command_issue_num++;
	m->n_num++;
	return res->buf;
}

void vectored_set(uint32_t start, uint32_t end, monitor* m, bool isseq){
	uint32_t request_per_command=_master->trans_configure.request_num_per_command;
	uint32_t number_of_command=(m->m_num)/request_per_command;
	m->m_num=number_of_command*request_per_command;
	m->tbody=(transaction_bench_value*)malloc(number_of_command * sizeof(transaction_bench_value));

	uint32_t request_buf_size=_master->trans_configure.request_size * request_per_command;

	m->command_num=number_of_command;
	m->command_issue_num=0;
	printf("total command : %lu\n", m->command_num);
	for(uint32_t i=0; i<number_of_command; i++){
		uint32_t idx=0;
		m->tbody[i].buf=(char*)malloc(request_buf_size + TXNHEADERSIZE);
		char *buf=m->tbody[i].buf;

		idx+=sizeof(uint32_t);//tid
		(*(uint32_t*)&buf[idx])=request_per_command;
		idx+=sizeof(uint32_t);

		for(uint32_t j=0; j<request_per_command; j++){
			(*(uint8_t*)&buf[idx])=FS_SET_T;
			idx+=sizeof(uint8_t);

			if(isseq){
				(*(uint32_t*)&buf[idx])=start+i*request_per_command+j;
				idx+=sizeof(uint32_t);
			}
			else{
				(*(uint32_t*)&buf[idx])=start+rand()%(end-start);
				idx+=sizeof(uint32_t);
			}
			(*(uint32_t*)&buf[idx])=0; //offset
			idx+=sizeof(uint32_t);
			m->write_cnt++;
		}
	}
}

void vectored_unique_rset(uint32_t start, uint32_t end, monitor* m){
	uint32_t request_per_command=_master->trans_configure.request_num_per_command;
	uint32_t number_of_command=(m->m_num)/request_per_command;
	m->m_num=number_of_command*request_per_command;
	m->tbody=(transaction_bench_value*)malloc(number_of_command * sizeof(transaction_bench_value));

	uint32_t request_buf_size=_master->trans_configure.request_size * request_per_command;

	m->command_num=number_of_command;
	m->command_issue_num=0;
	printf("total command : %lu\n", m->command_num);
	uint32_t max_num_of_req=number_of_command*request_per_command;
	uint32_t *key_buf=(uint32_t *)malloc(sizeof(uint32_t)*max_num_of_req);

	for(uint32_t i=0; i<max_num_of_req; i++){
		key_buf[i]=max_num_of_req-1-i;
	}
	/*
	for(uint32_t i=0; i<max_num_of_req; i++){
		uint32_t temp_idx=rand()%max_num_of_req;
		uint32_t temp_idx2=rand()%max_num_of_req;

		uint32_t temp=key_buf[temp_idx];
		key_buf[temp_idx]=key_buf[temp_idx2];
		key_buf[temp_idx2]=temp;
	}*/

	
	uint32_t key_idx=0;
	for(uint32_t i=0; i<number_of_command; i++){
		uint32_t idx=0;
		m->tbody[i].buf=(char*)malloc(request_buf_size + TXNHEADERSIZE);
		char *buf=m->tbody[i].buf;

		idx+=sizeof(uint32_t);//tid
		(*(uint32_t*)&buf[idx])=request_per_command;
		idx+=sizeof(uint32_t);

		for(uint32_t j=0; j<request_per_command; j++){
			(*(uint8_t*)&buf[idx])=FS_SET_T;
			idx+=sizeof(uint8_t);

			(*(uint32_t*)&buf[idx])=key_buf[key_idx++];
			idx+=sizeof(uint32_t);

			(*(uint32_t*)&buf[idx])=0; //offset
			idx+=sizeof(uint32_t);
			m->write_cnt++;
		}
	}
	free(key_buf);
}

void vectored_get(uint32_t start, uint32_t end, monitor* m, bool isseq){
	uint32_t request_per_command=_master->trans_configure.request_num_per_command;
	uint32_t number_of_command=(m->m_num)/request_per_command;
	m->m_num=number_of_command*request_per_command;
	m->tbody=(transaction_bench_value*)malloc(number_of_command * sizeof(transaction_bench_value));

	uint32_t request_buf_size=_master->trans_configure.request_size * request_per_command;
	m->command_num=number_of_command;
	m->command_issue_num=0;
	for(uint32_t i=0; i<number_of_command; i++){
		uint32_t idx=0;
		m->tbody[i].buf=(char*)malloc(request_buf_size + TXNHEADERSIZE);
		char *buf=m->tbody[i].buf;

		idx+=sizeof(uint32_t);//tid
		(*(uint32_t*)&buf[idx])=request_per_command;
		idx+=sizeof(uint32_t);

		for(uint32_t j=0; j<request_per_command; j++){
			(*(uint8_t*)&buf[idx])=FS_GET_T;
			idx+=sizeof(uint8_t);

			if(isseq){
				(*(uint32_t*)&buf[idx])=start+i*request_per_command+j;
				idx+=sizeof(uint32_t);
			}
			else{
				(*(uint32_t*)&buf[idx])=start+rand()%(end-start);
				idx+=sizeof(uint32_t);
			}
			(*(uint32_t*)&buf[idx])=0; //offset
			idx+=sizeof(uint32_t);
			m->read_cnt++;
		}
	}
}

void vectored_rw(uint32_t start, uint32_t end, monitor* m, bool isseq){
	uint32_t request_per_command=_master->trans_configure.request_num_per_command;
	uint32_t number_of_command=(m->m_num)/request_per_command;
	m->m_num=number_of_command*request_per_command;
	m->tbody=(transaction_bench_value*)malloc(number_of_command * sizeof(transaction_bench_value));

	uint32_t request_buf_size=_master->trans_configure.request_size * request_per_command;
	int *key_buf=(int*)malloc(sizeof(int) * request_per_command);
	m->command_num=number_of_command;
	m->command_issue_num=0;


	for(uint32_t i=0; i<number_of_command/2; i++){
		uint32_t idx=0;
		m->tbody[i].buf=(char*)malloc(request_buf_size + TXNHEADERSIZE);
		char *buf=m->tbody[i].buf;

		idx+=sizeof(uint32_t);//tid
		(*(uint32_t*)&buf[idx])=request_per_command;
		idx+=sizeof(uint32_t);

		for(uint32_t j=0; j<request_per_command; j++){
			(*(uint8_t*)&buf[idx])=FS_SET_T;
			idx+=sizeof(uint8_t);

			if(isseq){
				key_buf[j]=start+i*request_per_command+j;
				(*(uint32_t*)&buf[idx])=key_buf[j];
				idx+=sizeof(uint32_t);
			}
			else{
				key_buf[j]=start+rand()%(end-start);
				(*(uint32_t*)&buf[idx])=key_buf[j];
				idx+=sizeof(uint32_t);
			}

			(*(uint32_t*)&buf[idx])=0;//offset
			idx+=sizeof(uint32_t);
			m->write_cnt++;
		}

		idx=0;
		m->tbody[i+number_of_command/2].buf=(char*)malloc(request_buf_size + TXNHEADERSIZE);
		buf=m->tbody[i+number_of_command/2].buf;


		idx+=sizeof(uint32_t);//tid
		(*(uint32_t*)&buf[idx])=request_per_command;
		idx+=sizeof(uint32_t);

		for(uint32_t j=0; j<request_per_command; j++){
			(*(uint8_t*)&buf[idx])=FS_GET_T;
			idx+=sizeof(uint8_t);

			if(isseq){
				(*(uint32_t*)&buf[idx])=key_buf[j];
				idx+=sizeof(uint32_t);
			}
			else{
				(*(uint32_t*)&buf[idx])=key_buf[j];
				idx+=sizeof(uint32_t);
			}
			(*(uint32_t*)&buf[idx])=0;//offset
			idx+=sizeof(uint32_t);
			m->read_cnt++;
		}	
	}
	m->m_num=m->read_cnt+m->write_cnt;
	free(key_buf);
}


void *bench_transaction_end_req(void *_req){
	vec_request *vec=(vec_request*)_req;
	monitor *m=&_master->m[vec->mark];
	
	m->command_return_num++;

	free(vec->req_array);
	free(vec->buf);
	free(vec);
	return NULL;
}
