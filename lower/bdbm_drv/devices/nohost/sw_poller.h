#ifndef _SW_POLLER_HEADER_
#define _SW_POLLER_HEADER_

void sw_poller_init();
void sw_poller_enqueue(void *req);
void* sw_poller_dequeue();
void sw_poller_destroy();

#endif
