#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "AmfManager.h"
#include "time.h"

double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}

void readCb(void *req) {
	// do nothing
}

void writeCb(void *req) {
	// do nothing
}

void eraseCb(void *req) {
	// do nothing
}

void readErrorCb(void *req) {
	unsigned lpa = (uintptr_t)req;
	fprintf(stderr, "lpa %u aftl read translation error\n", lpa);
}

void writeErrorCb(void *req) {
	unsigned lpa = (uintptr_t)req;
	fprintf(stderr, "lpa %u aftl write translation error\n", lpa);
}

void eraseErrorCb(void *req) {
	// do nothing
}

int main() {
	AmfManager *am = AmfOpen(1); // Erase only mapped blocks (written blocks) so that device is clean state

	// register callbacks (erase has no callback for now)
	// SetReadCb(am, readCb, readErrorCb);
	SetReadCb(am, NULL, readErrorCb); // you can register NULL as a callback (ignored)
	SetWriteCb(am, writeCb, writeErrorCb);
	SetEraseCb(am, eraseCb, eraseErrorCb);

	char buf[8192];

	timespec start, now;
	int elapsed;

	for (unsigned int i=0; i< 10; i++) {
		// will all fail because not mapped yet (10 error messages)
		AmfRead(am, i, buf, (void*)(uintptr_t)i);
	}

	elapsed = 10000;
	while (true) {
		usleep(100);
		if (elapsed == 0) {
			elapsed = 10000;
		} else {
			elapsed--;
		}
		if (!IsAmfBusy(am)) break;
	}

	clock_gettime(CLOCK_REALTIME, &start);
	for (unsigned int i=0; i< 1024*1024; i++) {
		AmfWrite(am, i, buf, (void*)(uintptr_t)i);
	}

	elapsed = 10000;
	while (true) {
		usleep(100);
		if (elapsed == 0) {
			elapsed = 10000;
		} else {
			elapsed--;
		}
		if (!IsAmfBusy(am)) break;
	}
	clock_gettime(CLOCK_REALTIME, &now);

	fprintf(stderr, "WRITE SPEED: %f MB/s\n", ((8192.0*1024*1024)/1000000)/timespec_diff_sec(start,now));

	AmfClose(am); // close device and dump "aftl.bin"

	am = AmfOpen(0); // mode = 0; Open the device as it is; AFTL must be programmed or aftl.bin must be provided

	SetReadCb(am, readCb, readErrorCb);
	SetWriteCb(am, writeCb, writeErrorCb);
	SetEraseCb(am, eraseCb, eraseErrorCb);

	clock_gettime(CLOCK_REALTIME, &start);
	for (unsigned int i=0; i< 1024*1024 + 5; i++) {
		// the last 5 items will fail..
		AmfRead(am, i, buf, (void*)(uintptr_t)i);
	}

	elapsed = 10000;
	while (true) {
		usleep(100);
		if (elapsed == 0) {
			elapsed = 10000;
		} else {
			elapsed--;
		}
		if (!IsAmfBusy(am)) break;
	}

	clock_gettime(CLOCK_REALTIME, &now);
	fprintf(stderr, "READ SPEED: %f MB/s\n", ((8192.0*1024*1024)/1000000)/timespec_diff_sec(start,now));

	AmfClose(am); // close device and dump "aftl.bin"
}
