#include "bdbm_drv.h"
#include "umemory.h" /* bdbm_malloc */
#include "uthread.h" /* bdbm_thread_nanosleep */
#include "devices.h" /* bdbm_dm_get_inf */
#include "debug.h" /* bdbm_msg */
#include <queue>

typedef struct memio {
	bdbm_drv_info_t bdi;
	bdbm_llm_req_t* rr;
	int nr_punits;
	int nr_tags;
	uint64_t io_size; /* bytes */
	uint64_t trim_size;
	uint64_t trim_lbas;
	std::queue<int>* tagQ;
	bdbm_mutex_t tagQMutex;
	bdbm_cond_t  tagQCond;
} memio_t;

memio_t* memio_open ();
void memio_wait (memio_t* mio);
int memio_read (memio_t* mio, uint64_t lba, uint64_t len, uint8_t* data);
int memio_write (memio_t* mio, uint64_t lba, uint64_t len, uint8_t* data);
int memio_trim (memio_t* mio, uint64_t lba, uint64_t len);
void memio_close (memio_t* mio);
