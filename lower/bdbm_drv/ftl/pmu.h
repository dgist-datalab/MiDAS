#ifndef __BLUEDBM_PMU_H
#define __BLUEDBM_PMU_H

#include "utime.h"
#include "umemory.h"

/* performance monitor functions */
void pmu_create (bdbm_drv_info_t* bdi);
void pmu_destory (bdbm_drv_info_t* bdi);
void pmu_display (bdbm_drv_info_t* bdi);

void pmu_inc (bdbm_drv_info_t* bdi, bdbm_llm_req_t* llm_req);
void pmu_inc_read (bdbm_drv_info_t* bdi);
void pmu_inc_write (bdbm_drv_info_t* bdi);
void pmu_inc_rmw_read (bdbm_drv_info_t* bdi);
void pmu_inc_rmw_write (bdbm_drv_info_t* bdi);
void pmu_inc_gc (bdbm_drv_info_t* bdi);
void pmu_inc_gc_erase (bdbm_drv_info_t* bdi);
void pmu_inc_gc_read (bdbm_drv_info_t* bdi);
void pmu_inc_gc_write (bdbm_drv_info_t* bdi);
void pmu_inc_meta_read (bdbm_drv_info_t* bdi);
void pmu_inc_meta_write (bdbm_drv_info_t* bdi);
void pmu_inc_util_r (bdbm_drv_info_t* bdi, uint64_t pid);
void pmu_inc_util_w (bdbm_drv_info_t* bdi, uint64_t pid);

void pmu_update_sw (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
void pmu_update_r_sw (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* sw);
void pmu_update_w_sw (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* sw);
void pmu_update_rmw_sw (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* sw);
void pmu_update_gc_sw (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* sw);

void pmu_update_q (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
void pmu_update_r_q (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* req);
void pmu_update_w_q (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* req);
void pmu_update_rmw_q (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* req);

void pmu_update_tot (bdbm_drv_info_t* bdi, bdbm_llm_req_t* req);
void pmu_update_r_tot (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* req);
void pmu_update_w_tot (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* req);
void pmu_update_rmw_tot (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* req);
void pmu_update_gc_tot (bdbm_drv_info_t* bdi, bdbm_stopwatch_t* sw);

#endif
