#ifndef _CL_HEAD
#define _CL_HEAD
#include <pthread.h>
#include "../settings.h"
typedef struct{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	volatile int cnt;
	volatile int now;
	bool zero_lock;
}cl_lock;

cl_lock *cl_init(int cnt,bool);
void cl_grap(cl_lock *);
void cl_cond_grap(cl_lock *,bool);
void cl_always_release(cl_lock*);
void cl_now_update(cl_lock*, int);
void cl_release(cl_lock *);
void cl_free(cl_lock *);

void cl_grep_with_f(cl_lock *, int s, int d, bool (*cmp)(int,int));
void cl_release_with_f(cl_lock *, int s, int d, bool (*cmp)(int,int));

#endif
