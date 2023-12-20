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

#ifndef _BLUEDBM_SYNC_H
#define _BLUEDBM_SYNC_H

#if defined(KERNEL_MODE)

/* completion lock */
#include <linux/version.h>
#include <linux/completion.h>
#define bdbm_completion_t struct completion
#define	bdbm_init_completion(a)	init_completion(&a)
#define bdbm_wait_for_completion(a)	wait_for_completion(&a)
#define bdbm_try_wait_for_completion(a) try_wait_for_completion(&a)
#define bdbm_complete(a) complete(&a)
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,12,0)
#define bdbm_reinit_completion(a) INIT_COMPLETION(a)
#else
#define	bdbm_reinit_completion(a) reinit_completion(&a)
#endif

/* synchronization (semaphore) */
#include <linux/semaphore.h>
#define bdbm_sema_t struct semaphore
#define bdbm_sema_init(a) sema_init (a, 1)
#define bdbm_sema_lock(a) down (a)
#define bdbm_sema_lock_interruptible(a) down_interruptible(a)
#define bdbm_sema_unlock(a) up (a)
#define bdbm_sema_try_lock(a) ({  /* 0: busy, 1: idle */ \
	int z = down_trylock(a); int ret; \
	if (z == 0) ret = 1; \
	else ret = 0; \
	ret; })
#define bdbm_sema_free(a)

/* synchronization (mutex) 
 * NOTE: simply reuse semaphore; it should be replaced with kernel mutex later */
#define bdbm_mutex_t bdbm_sema_t
#define bdbm_mutex_init(a) bdbm_sema_init(a)
#define bdbm_mutex_lock(a) bdbm_sema_lock(a)
#define bdbm_mutex_lock_interruptible(a) bdbm_sema_lock_interruptible(a)
#define bdbm_mutex_unlock(a) bdbm_sema_unlock(a)
#define bdbm_mutex_try_lock(a) bdbm_sema_try_lock(a)
#define bdbm_mutex_free(a) bdbm_sema_free(a)

/* spinlock */
#define bdbm_spinlock_t spinlock_t
#define bdbm_spin_lock_init(a) spin_lock_init(a)
#define bdbm_spin_lock(a) spin_lock(a)
#define bdbm_spin_lock_irqsave(a,flag) spin_lock_irqsave(a,flag)
#define bdbm_spin_unlock(a) spin_unlock(a)
#define bdbm_spin_unlock_irqrestore(a,flag) spin_unlock_irqrestore(a,flag)
#define bdbm_spin_lock_destory(a)


#elif defined(USER_MODE) 


/* spinlock */
#include <pthread.h>
#define bdbm_spinlock_t pthread_spinlock_t
#define bdbm_spin_lock_init(a) pthread_spin_init(a,0)
#define bdbm_spin_lock(a) pthread_spin_lock(a)
#define bdbm_spin_lock_irqsave(a,flag) pthread_spin_lock(a);
#define bdbm_spin_unlock(a) pthread_spin_unlock(a)
#define bdbm_spin_unlock_irqrestore(a,flag) pthread_spin_unlock(a)
#define bdbm_spin_lock_destory(a) pthread_spin_destroy(a)

/* synchronization (semaphore) */
#include <semaphore.h>  /* Semaphore */
#define bdbm_sema_t sem_t 
#define bdbm_sema_init(a) sem_init(a, 0, 1)
#define bdbm_sema_init2(a,b) sem_init(a, 0, b)
#define bdbm_sema_wait(a) sem_wait(a)
#define bdbm_sema_post(a) sem_post(a)
#define bdbm_sema_lock(a) sem_wait(a)
#define bdbm_sema_lock_interruptible(a) sem_wait(a)
#define bdbm_sema_unlock(a) sem_post(a)
#define bdbm_sema_try_lock(a) ({ /* 0: busy, 1: idle */ \
	int z = sem_trywait(a); int ret; \
	if (z == 0) ret = 1; \
	else ret = 0; \
	ret; })
#define bdbm_sema_free(a) sem_destroy(a)

/* synchronization (mutex) */
#define bdbm_mutex_t pthread_mutex_t 
#define bdbm_mutex_init(a) pthread_mutex_init(a, NULL)
#define bdbm_mutex_lock(a) pthread_mutex_lock(a)
#define bdbm_mutex_lock_interruptible(a) pthread_mutex_lock(a)
#define bdbm_mutex_unlock(a) pthread_mutex_unlock(a)
#define bdbm_mutex_try_lock(a) ({ /* 0: busy, 1: idle */ \
	int z = pthread_mutex_trylock(a); int ret; \
	if (z == 0) ret = 1; \
	else ret = 0; \
	ret; })
#define bdbm_mutex_free(a) pthread_mutex_destroy(a)

/* condition */
#define bdbm_cond_t pthread_cond_t
#define bdbm_cond_init(a) pthread_cond_init(a,0)
#define bdbm_cond_signal(a) pthread_cond_signal(a)
#define bdbm_cond_broadcast(a) pthread_cond_broadcast(a)
#define bdbm_cond_wait(a,b) pthread_cond_wait(a,b)
#define bdbm_cond_free(a) pthread_cond_destroy(a)

#else
/* ERROR CASE */
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#endif /* _BLUEDBM_SYNC_H */ 
