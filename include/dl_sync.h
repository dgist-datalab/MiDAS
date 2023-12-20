#ifndef __H_DL_SYNC
#define __H_DL_SYNC
#include <stdint.h>
#include <pthread.h>
#include "settings.h"
typedef struct dl_syncronizer{
#ifdef SPINSYNC
	volatile uint64_t target_cnt;
	volatile uint64_t now_cnt;
#else
	pthread_mutex_t mutex_sync;
#endif
}dl_sync;

typedef struct dl_syncornizer_multi{
	uint64_t target_cnt;
	uint64_t now_cnt;
	pthread_mutex_t mutex_sync;
}dl_sync_m;

void dl_sync_init(dl_sync *, uint64_t cnt);
void dl_sync_wait(dl_sync*);
void dl_sync_arrive(dl_sync*);

void dl_syncM_init(dl_sync_m *, uint64_t cnt);
void dl_syncM_wait(dl_sync_m*);
void dl_syncM_arrive(dl_sync_m*);
void dl_syncM_cnt_update(dl_sync_m *, uint64_t cnt);
#endif
