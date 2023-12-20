#ifndef __H_FS__
#define __H_FS__
#define DMARBUF	1
#define DMAWBUF 2
int F_malloc(void **,int size, int rw);
void F_free(void *, int tag, int rw);
#endif
