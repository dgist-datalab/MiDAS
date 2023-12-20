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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "libmemio.h"

double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}

int main (int argc, char** argv)
{
	// this test should be done after writing 256 MB from LPA 0 & using same table.dump.0
	uint8_t *readBuffer;
	memio_t* mio = NULL;

	timespec start, now;

	readBuffer = (uint8_t*)malloc(256*1024*1024);
	if (readBuffer == NULL) {
		printf("Could not allocate 256 MB buffer\n");
		return -1;
	}

	if ((mio = memio_open()) == NULL) {
		printf("could not open memio\n");
		return -1;
	}

	for (int size=2; size <=256; size=size*2) {
		clock_gettime(CLOCK_REALTIME, & start);
		memio_read(mio, 0, size*1024*1024, readBuffer);
		clock_gettime(CLOCK_REALTIME, & now);
		printf("Read Size: %d MB, SPEED: %f MB/s\n", size, 1.0*size/timespec_diff_sec(start, now));
		printf("time elapsed: %f s\n", timespec_diff_sec(start, now));
	}

	memio_close(mio);

	return 0;
}

int main2 (int argc, char** argv)
{
	int i = 0, j = 0, tmplba = 0;
	srand(time(NULL));

	memio_t* mio = NULL;
	int32_t wdata[2048], rdata[2048]; // uint8_t -> int32_t
	
	/* open the device */
	if ((mio = memio_open ()) == NULL)
		goto fail;

	/* perform some operations = test only 512 segments */
	for (i = 0; i < 20; i++) {
		memio_trim (mio, (i<<14), (1<<14)*8192);
		memio_wait (mio);
	}

	for (i = 0; i < 20*(1<<14); i++) {
		/* 4 bytes of LSB and MSB are LBA */
		wdata[0] = i;    
		wdata[2047] = i;

		memio_write (mio, i, 8192, (uint8_t*)wdata);

		if (i%100 == 0) {
			printf ("w");
			fflush (stdout);
		}
	}
	memio_wait (mio);

	/* test random LBAs and read */
	printf ("start reads...\n\n\n");
	for (i = 0; i < 64*(1<<14); i++) {   // reducing the test case
		tmplba = rand() % (20*(1<<14)); // try random lba

		memio_read (mio, tmplba, 8192, (uint8_t*)rdata);
		memio_wait (mio);
		if (rdata[0] != tmplba || rdata[2047] != tmplba) {
			bdbm_msg ("[%d] OOPS! LBA=%d rdata[0] = %d, rdata[2047] = %d", i, tmplba, rdata[0], rdata[2047]);
		}
		if (i%100 == 0) {
			printf ("r");
			fflush (stdout);
		}
	}

	/* close the device */
	memio_close (mio);
	return 0;

fail:
	return -1;
}

