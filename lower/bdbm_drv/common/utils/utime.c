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

#if defined(KERNEL_MODE)
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/slab.h>

#include "bdbm_drv.h"
#include "umemory.h"
#include "debug.h"
#include "utime.h"

#elif defined(USER_MODE)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "utime.h"

#define do_gettimeofday(a) gettimeofday (a, NULL)

#else
	#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif


#if defined(KERNEL_MODE) && \
    defined(USE_KTIMER)
ktime_t ktime_get (void);
#endif

static uint32_t _time_startup_timestamp = 0;


uint32_t time_get_timestamp_in_us (void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

uint32_t time_get_timestamp_in_sec (void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_sec - _time_startup_timestamp;
}

void time_init (void)
{
	_time_startup_timestamp = time_get_timestamp_in_sec ();
}


/* stopwatch functions */
int timeval_subtract (
	struct timeval *result, 
	struct timeval *x, 
	struct timeval *y)
{
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	return x->tv_sec < y->tv_sec;
}

void bdbm_stopwatch_start (bdbm_stopwatch_t* sw)
{
	if (sw) {
#if defined(KERNEL_MODE) && \
	defined(USE_KTIMER)
		sw->start = ktime_get ();
#else
		do_gettimeofday(&sw->start);
#endif
	}
}

int64_t bdbm_stopwatch_get_elapsed_time_ms (bdbm_stopwatch_t* sw)
{
	if (sw) {
#if defined(KERNEL_MODE) && \
	defined(USE_KTIMER)
		ktime_t end = ktime_get ();
		return ktime_to_ms (ktime_sub (end, sw->start));

#else
		struct timeval diff, end;
		do_gettimeofday (&end);
		if (timeval_subtract (&diff, &end, &sw->start) == 0) {
			return (diff.tv_sec * 1000000 + diff.tv_usec)/1000;
		}
#endif
	}
	return 0;
}

int64_t bdbm_stopwatch_get_elapsed_time_us (bdbm_stopwatch_t* sw)
{
	if (sw) {
#if defined(KERNEL_MODE) && \
	defined(USE_KTIMER)
		ktime_t end = ktime_get ();
		return ktime_to_us (ktime_sub (end, sw->start));

#else
		struct timeval diff, end;
		do_gettimeofday (&end);
		if (timeval_subtract (&diff, &end, &sw->start) == 0) {
			return diff.tv_sec * 1000000 + diff.tv_usec;
		}
#endif
	}
	return 0;
}

struct timeval bdbm_stopwatch_get_elapsed_time (bdbm_stopwatch_t* sw)
{
	struct timeval diff;

	if (sw) {
#if defined(KERNEL_MODE) && \
	defined(USE_KTIMER)
		ktime_t end = ktime_get ();
		diff.tv_usec = ktime_to_us (ktime_sub (end, sw->start));
#else
		struct timeval end;
		do_gettimeofday (&end);
		timeval_subtract (&diff, &end, &sw->start);
#endif
	}
	return diff;
}

