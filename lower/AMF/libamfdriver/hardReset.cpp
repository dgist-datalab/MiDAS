#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AmfManager.h"
#include "time.h"

double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}

// void readCb(void *req) {
// 	// do nothing
// }
// 
// void writeCb(void *req) {
// 	// do nothing
// }
// 
// void eraseCb(void *req) {
// 	// do nothing
// }
// 
// void readErrorCb(void *req) {
// 	// do nothing
// }
// 
// void writeErrorCb(void *req) {
// 	// do nothing
// }
// 
// void eraseErrorCb(void *req) {
// 	// do nothing
// }

int main() {
	// Hard-reset of the devic3
	//   If AFTL is programmed or "aftl.bin" exists, PE counts will be honored
	//   Erase All blocks in the system & clean up the table
	AmfManager *am = AmfOpen(2); // mode == 2 (hard reset)

	// if you do not register a call back function or set it NULL,
	//  callback is ignored

	AmfClose(am); // close device and dump "aftl.bin"
}
