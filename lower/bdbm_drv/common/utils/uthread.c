/*
The MIT License (MIT)

Copyright (c) 2014-2015 CSAIL, MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "debug.h"
#include "usync.h"
#include "umemory.h"
#include "uthread.h"

#if defined(KERNEL_MODE)
#include <linux/init.h>
#include <linux/module.h>

int bdbm_thread_fn (void *data) 
{
	bdbm_thread_t* k = (bdbm_thread_t*)data;

	/* initialize a kernel thread */
	DECLARE_WAITQUEUE (wait, current);
	k->wait = &wait;

	bdbm_daemonize ("bdbm_thread_fn");
	allow_signal (SIGKILL); 

	bdbm_mutex_lock (&k->thread_done);
	
	/* invoke a user call-back function */
	k->user_threadfn (k->user_data);


	/* a thread stops */
	k->wait = NULL;
	bdbm_mutex_unlock (&k->thread_done);

	return 0;
};

bdbm_thread_t* bdbm_thread_create (
	int (*user_threadfn)(void *data), 
	void* user_data, 
	const char* name)
{
	bdbm_thread_t* k = NULL;

	/* create bdbm_thread_t */
	if ((k = (bdbm_thread_t*)bdbm_malloc_atomic (
			sizeof (bdbm_thread_t))) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		return NULL;
	}

	/* initialize bdbm_thread_t */
	k->user_threadfn = user_threadfn;
	k->user_data = (void*)user_data;
	k->wait = NULL;
	bdbm_mutex_init (&k->thread_done);
	init_waitqueue_head (&k->wq);
	if ((k->thread = kthread_create (
			bdbm_thread_fn, (void*)k, name)) == NULL) {
		bdbm_error ("kthread_create failed");
		bdbm_free_atomic (k);
		return NULL;
	} 

	return k;
}

int bdbm_thread_run (bdbm_thread_t* k)
{
	/* wake up thread! */
	wake_up_process (k->thread);

	return 0;
}

int bdbm_thread_schedule (bdbm_thread_t* k)
{
	if (k == NULL || k->wait == NULL) {
		bdbm_error ("oops! k or k->wait is NULL (%p, %p)", k, k->wait);
		return 0;
	}

	add_wait_queue (&k->wq, k->wait);
	set_current_state (TASK_INTERRUPTIBLE);
	schedule (); /* go to sleep */
	remove_wait_queue (&k->wq, k->wait);

	if (signal_pending (current)) {
		/* get a kill signal */
		return SIGKILL;
	}

	return 0;
}

void bdbm_thread_schedule_setup (bdbm_thread_t* k)
{	
	add_wait_queue (&k->wq, k->wait);
	set_current_state (TASK_INTERRUPTIBLE);
}

void bdbm_thread_schedule_cancel (bdbm_thread_t* k)
{
	/* FIXME: I'm not sure wether or not to remove wait from queue */
	remove_wait_queue (&k->wq, k->wait);
}

int bdbm_thread_schedule_sleep (bdbm_thread_t* k)
{
	schedule (); /* go to sleep */
	remove_wait_queue (&k->wq, k->wait);

	if (signal_pending (current)) {
		/* get a kill signal */
		return SIGKILL;
	}

	return 0;
}

void bdbm_thread_wakeup (bdbm_thread_t* k)
{
	if (k == NULL) {
		bdbm_error ("oops! k is NULL");
		return;
	}

	wake_up_interruptible (&k->wq);
}

void bdbm_thread_stop (bdbm_thread_t* k)
{
	if (k == NULL) {
		bdbm_error ("oops! k is NULL");
		return;
	}

	/* send a KILL signal to the thread */
	send_sig (SIGKILL, k->thread, 0);
	bdbm_mutex_lock (&k->thread_done);

	/* free bdbm_thread_t */
	bdbm_free_atomic (k);
}

void bdbm_thread_msleep (uint32_t ms) 
{
	msleep (ms);
}

void bdbm_thread_yield ()
{
	yield ();
}

#endif /* KERNEL_MODE */


#if defined(USER_MODE)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>

#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

void* bdbm_thread_fn (void *data) 
{
	bdbm_thread_t* k = (bdbm_thread_t*)data;

	/* initialize a kernel thread */
	bdbm_mutex_lock (&k->thread_done);
	
	/* invoke a user call-back function */
	k->user_threadfn (k->user_data);

	/* a thread stops */
	bdbm_mutex_unlock (&k->thread_done);

	pthread_exit (0);
	return NULL;
};

bdbm_thread_t* bdbm_thread_create (
	int (*user_threadfn)(void *data), 
	void* user_data, 
	const char* name)
{
	bdbm_thread_t* k = NULL;

	/* create bdbm_thread_t */
	if ((k = (bdbm_thread_t*)bdbm_malloc_atomic (
			sizeof (bdbm_thread_t))) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		return NULL;
	}

	/* initialize bdbm_thread_t */
	k->user_threadfn = user_threadfn;
	k->user_data = (void*)user_data;
	bdbm_mutex_init (&k->thread_done);
	bdbm_mutex_init (&k->thread_sleep);
	pthread_cond_init (&k->thread_con, NULL);

	bdbm_msg ("new thread created: %p", k);

	return k;
}

int bdbm_thread_run (bdbm_thread_t* k)
{
	return pthread_create (&k->thread, NULL, bdbm_thread_fn, (void*)k);
}

int bdbm_thread_schedule (bdbm_thread_t* k)
{
	int ret = 0;
#ifdef PTHREAD_TIMEOUT
	struct timespec ts;

	/* setup waiting time */
	clock_gettime (CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;
#endif

	if (k == NULL) {
		bdbm_warning ("k is NULL");
		return 0;
	}

	/* sleep until wake-up signal */
	if ((ret = bdbm_mutex_lock (&k->thread_sleep)) == 0) {
		/* FIXME: need to fix a time-out bug that occasionally occurs in an exceptional case */
#ifdef PTHREAD_TIMEOUT
		if ((ret = pthread_cond_timedwait 
				(&k->thread_con, &k->thread_sleep, &ts)) != 0) {
			bdbm_warning ("pthread timeout: %u %s", ret, strerror (ret));
		}
#else
		if ((ret = pthread_cond_wait
				(&k->thread_con, &k->thread_sleep)) != 0) {
			bdbm_warning ("pthread timeout: %u %s", ret, strerror (ret));
		}
#endif
		bdbm_mutex_unlock (&k->thread_sleep);
	} else {
		bdbm_warning ("pthread lock failed: %u %s", ret, strerror (ret));
	}

	return 0;
}

void bdbm_thread_schedule_setup (bdbm_thread_t* k)
{	
	int ret = 0;
	if ((ret = bdbm_mutex_lock (&k->thread_sleep)) != 0) {
		bdbm_warning ("pthread lock failed: %u %s", ret, strerror (ret));
	}
}

void bdbm_thread_schedule_cancel (bdbm_thread_t* k)
{
	bdbm_mutex_unlock (&k->thread_sleep);
}

int bdbm_thread_schedule_sleep (bdbm_thread_t* k)
{
	int ret = 0;
#ifdef PTHREAD_TIMEOUT
	struct timespec ts;

	/* setup waiting time */
	clock_gettime (CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;

	if ((ret = pthread_cond_timedwait 
			(&k->thread_con, &k->thread_sleep, &ts)) != 0) {
		bdbm_warning ("pthread timeout: %u %s", ret, strerror (ret));
	}
#else
	if ((ret = pthread_cond_wait 
			(&k->thread_con, &k->thread_sleep)) != 0) {
		bdbm_warning ("pthread timeout: %u %s", ret, strerror (ret));
	}
#endif

	bdbm_mutex_unlock (&k->thread_sleep);

	return ret;
}

void bdbm_thread_wakeup (bdbm_thread_t* k)
{
	int ret = 0;

	if (k == NULL) {
		bdbm_warning ("k is NULL");
		return;
	}

	/* send a wake-up signal */
	if ((ret = bdbm_mutex_try_lock (&k->thread_sleep)) == 1) {
		pthread_cond_signal (&k->thread_con);
		bdbm_mutex_unlock (&k->thread_sleep);
	} else {
		/*bdbm_warning ("pthread lock failed: %u %s", ret, strerror (ret));*/
	}
}

void bdbm_thread_stop (bdbm_thread_t* k)
{
	int ret;

	if (k == NULL) {
		bdbm_warning ("k is NULL");
		return;
	}

	bdbm_msg ("thread destroyed: %p", k);

	/* send a kill signal */
	if ((ret = pthread_cancel (k->thread)) != 0)
		bdbm_msg ("pthread_cancel is %d", ret);

	if ((ret = pthread_join (k->thread, NULL)) != 0)
		bdbm_msg ("pthread_join is %d", ret);

	/* clean up */
	bdbm_mutex_free (&k->thread_done);
	bdbm_mutex_free (&k->thread_sleep);
	pthread_cond_destroy (&k->thread_con);
	bdbm_free_atomic (k);
}

void bdbm_thread_msleep (uint32_t ms) 
{
	int microsecs;
	struct timeval tv;
	microsecs = ms * 1000;
	tv.tv_sec  = microsecs / 1000000;
	tv.tv_usec = microsecs % 1000000;
	select (0, NULL, NULL, NULL, &tv);  
}

void bdbm_thread_nanosleep (uint32_t ns) 
{
	struct timespec tv;
	tv.tv_sec  = 0;
	tv.tv_nsec = ns;
//	pselect (0, NULL, NULL, NULL, &tv, NULL);
	nanosleep (&tv, NULL);
}

void bdbm_thread_yield (void)
{
	pthread_yield ();
}

#endif

