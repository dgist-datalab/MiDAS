#include "../../include/container.h"
#include "../../interface/interface.h"

#ifndef __grouping__
#define __grouping__
#define GNUMBER 5
#define BENCH_JY 1
#define FREENUM 4
#define MAX_G 19
//0 is zip08, 1 is zip11, 2 is random
#define TIME_WINDOW GIGAUNIT*4
#endif

typedef struct naive_mida {
        bool naive_on;
        uint32_t naive_start;
	queue* naive_q;
}naive;

typedef struct MiDAS_system {
	uint32_t *config; //size of each group. if (gsize[i] > config[i]), need to gc group i
	double *vr;
	double WAF;
	bool status;
	uint32_t errcheck_time;
	uint32_t time_window;
}midas;

typedef struct page_map_body{
	uint32_t *mapping;
	bool isfull;
	uint32_t assign_page;
	uint32_t gcur;

	uint32_t *ginfo; //segment's group number info
	queue** group; 
	uint32_t gnum; //# of groups
	naive *n;
	midas *m;

	/*segment is a kind of Physical Block*/
	__segment **reserve; //for gc
	__segment **active; //for gc

	queue* active_q; //unused active seg by merge group
}pm_body;


void page_map_create();
uint32_t page_map_assign(KEYT *lba, uint32_t max_idx, int gnum);
uint32_t page_map_pick(uint32_t lba);
uint32_t page_map_trim(uint32_t lba);
uint32_t page_map_gc_update(KEYT* lba, uint32_t idx, uint32_t mig_count);
void page_map_free();

uint32_t seg_assign_ginfo(uint32_t seg_idx, uint32_t group_number);
uint32_t seg_get_ginfo(uint32_t seg_idx);
