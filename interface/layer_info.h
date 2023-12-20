#ifndef __H_LINFO__
#define __H_LINFO__
#include "../include/container.h"
#include "threading.h"
//alogrithm layer
extern struct algorithm __normal;
extern struct algorithm __badblock;
extern struct algorithm demand_ftl;
extern struct algorithm page_ftl;
extern struct algorithm algo_lsm;
extern struct algorithm demand_ftl;
extern struct algorithm lsm_ftl;

//device layer
extern struct lower_info memio_info;
extern struct lower_info aio_info;
extern struct lower_info net_info;
extern struct lower_info my_posix; //posix, posix_memory,posix_async
extern struct lower_info no_info;
extern struct lower_info amf_info;

//block manager
extern struct blockmanager base_bm;
extern struct blockmanager pt_bm;
extern struct blockmanager seq_bm;

static void layer_info_mapping(master_processor *mp,int argc, char **argv){
#if defined(posix) || defined(posix_async) || defined(posix_memory)
	mp->li=&my_posix;
#elif defined(bdbm_drv)
	mp->li=&memio_info;
#elif defined(network)
	mp->li=&net_info;
#elif defined(linux_aio)
	mp->li=&aio_info;
#elif defined(no_dev)
	mp->li=&no_info;
#elif defined(AMF)
	mp->li=&amf_info;
#endif


#ifdef normal
	mp->algo=&__normal;
#elif defined(MiDAS)
	mp->algo=&page_ftl;
#elif defined(DFTL)
	mp->algo=&demand_ftl;
#elif defined(demand)
	mp->algo=&__demand;
#elif defined(lsmftl)
	mp->algo=&lsm_ftl;
#elif defined(badblock)
	mp->algo=&__badblock;
#endif
	

#if defined(partition) && !defined(Page_ftl)
	mp->bm=&pt_bm;
#elif defined(sequential)
	mp->bm=&seq_bm;
#else
	mp->bm=&base_bm;
#endif

	mp->li->create(mp->li,mp->bm);
#if (defined(partition) && !defined(Page_ftl))
	printf("PARTNUM: %u, MAP: %lu, DATA: %lu\n", PARTNUM, MAPPART_SEGS, DATAPART_SEGS);
	int temp[PARTNUM];
	temp[MAP_S]=MAPPART_SEGS;
	temp[DATA_S]=DATAPART_SEGS;
	mp->bm->pt_create(mp->bm,PARTNUM,temp,mp->li);
#else
	mp->bm->create(mp->bm,mp->li);
#endif
	if(mp->algo->argument_set){
		mp->algo->argument_set(argc,argv);
	}
	mp->algo->create(mp->li,mp->bm,mp->algo);
}
#endif
