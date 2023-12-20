#include "hot.h"
#include "model.h"
#include "map.h"
#include "midas.h"

extern algorithm page_ftl;
extern STAT* midas_stat;
extern mini_model *mmodel;
extern HF_Q* hot_q; 
extern int jy_LBANUM;
extern HF* hotfilter;

void hf_init() {
	hotfilter = (HF*)malloc(sizeof(HF));
	hotfilter->max_val = 3;
	hotfilter->hot_val = 3;
	hotfilter->tw_ratio = 1.0;

	hotfilter->make_flag=1;
	hotfilter->use_flag=1;

	hotfilter->tw = (long)jy_LBANUM;
	hotfilter->left_tw = hotfilter->tw;
	hotfilter->cold_tw = (long)jy_LBANUM;

	hotfilter->G0_vr_sum=0.0;
	hotfilter->G0_vr_num=0.0;
	hotfilter->seg_age=0.0;
	hotfilter->seg_num=0.0;
	hotfilter->avg_seg_age=0.0;

	hotfilter->G0_traffic_ratio=0.0;
	hotfilter->tot_traffic=0.0;
	hotfilter->G0_traffic=0.0;

	hotfilter->hot_lba_num=0;

	hotfilter->err_cnt=0;
	hotfilter->tmp_err_cnt=0;

	hotfilter->cur_hf = (int*)malloc(sizeof(int)*jy_LBANUM);
	memset(hotfilter->cur_hf, 0, sizeof(int)*jy_LBANUM);

	hf_q_init();
}

void hf_q_init() {
	hot_q = (HF_Q *)malloc(sizeof(HF_Q));
	
	hot_q->queue_idx=0;

	hot_q->g0_traffic=0.0;
	hot_q->g0_size=0.0;
	hot_q->g0_valid=0.0;
	hot_q->queue_max=10;

	hot_q->extra_size=0;
	hot_q->extra_traffic=0.0;

	hot_q->calc_traffic=0.0;
	hot_q->calc_size=0;
	hot_q->calc_unit=0;

	hot_q->best_extra_size=0;
	hot_q->best_extra_traffic=0.0;
	hot_q->best_extra_unit=0.0;

	hot_q->g0_traffic_queue=(double*)malloc(sizeof(double)*hot_q->queue_max);
	hot_q->g0_size_queue=(double*)malloc(sizeof(double)*hot_q->queue_max);
	hot_q->g0_valid_queue=(double*)malloc(sizeof(double)*hot_q->queue_max);

	memset(hot_q->g0_traffic_queue, 0, sizeof(double)*hot_q->queue_max);
	memset(hot_q->g0_size_queue, 0, sizeof(double)*hot_q->queue_max);
	memset(hot_q->g0_valid_queue, 0, sizeof(double)*hot_q->queue_max);
}

void hf_q_reset(bool true_reset) {
	hot_q->g0_traffic=0;
	hot_q->g0_size=0;
	hot_q->g0_valid=0.0;
	hot_q->queue_idx=0;

	hot_q->extra_size=0;
	hot_q->extra_traffic=0.0;
	
	hot_q->best_extra_size=0;
	hot_q->best_extra_traffic=0.0;
	hot_q->best_extra_unit=0.0;
	
	hot_q->calc_traffic=0.0;
	hot_q->calc_size=0;
	hot_q->calc_unit=0;

	if (true_reset) {
		memset(hot_q->g0_traffic_queue, 0, sizeof(double)*hot_q->queue_max);
		memset(hot_q->g0_size_queue, 0, sizeof(double)*hot_q->queue_max);
		memset(hot_q->g0_valid_queue, 0, sizeof(double)*hot_q->queue_max);
	}
}

void hf_q_calculate() {
	hf_q_reset(false);

	double hfq_cnt=0.0;
	for (int i=0;i<hot_q->queue_max; i++) {
		if (hot_q->g0_traffic_queue[i] != 0.0) hfq_cnt++;
		hot_q->g0_traffic += hot_q->g0_traffic_queue[i];
		hot_q->g0_size += hot_q->g0_size_queue[i];
		hot_q->g0_valid += hot_q->g0_valid_queue[i];
	}
	if (hfq_cnt == 0.0) {
		printf("there is no information about hotfilter!!\n");
		abort();
	}
	hot_q->g0_traffic = hot_q->g0_traffic/hfq_cnt;
	hot_q->g0_size = hot_q->g0_size/hfq_cnt;
	hot_q->g0_valid = hot_q->g0_valid/hfq_cnt;

	hot_q->g0_size = floor(hot_q->g0_size+0.5);
	
	if ((hot_q->g0_valid > 0.2) || (hot_q->g0_traffic < 0.15)) {
		printf("hotfilter accuracy is low, so FIXED\n");
		hot_q->is_fix=true;
	}else hot_q->is_fix=false;
	
	printf("==============HOT FILTER INFO=============\n");
	printf("- avg. traffic: %.3f\n", hot_q->g0_traffic);
	printf("- avg. size   : %.3f\n", hot_q->g0_size);
	printf("- avg. valid  : %.3f\n", hot_q->g0_valid);
	printf("------------------------------------------\n");

	return;
}

void hf_destroy() {
	free(hotfilter->cur_hf);
	free(hotfilter);
}

void hf_metadata_reset() {
	hotfilter->make_flag=0;
	hotfilter->use_flag=0;
	hotfilter->G0_vr_num=0;
	hotfilter->G0_vr_sum=0;
	hotfilter->seg_age=0.0;
	hotfilter->seg_num=0.0;
	hotfilter->avg_seg_age=0.0;
	hotfilter->G0_traffic_ratio=0.0;
	hotfilter->tot_traffic=0.0;
	hotfilter->G0_traffic=0.0;
	hotfilter->hot_lba_num=0;
	hotfilter->left_tw = hotfilter->tw;
	hotfilter->err_cnt=0;
	hotfilter->tmp_err_cnt=0;
}

void hot_merge() {
	pm_body *p = (pm_body*)page_ftl.algo_body;

	printf("NAIVE_START is 1!!! HOT MERGE on\n");
	//0: move the whole queue to the heap
	int size = page_ftl.bm->jy_move_q2h(page_ftl.bm, p->group[0], 0);
	if ((size+1) != midas_stat->g->gsize[0]) {
		printf("size miss: stat->gsize and real queue size is different\n");
		printf("in hot merge function\n");
		abort();
	}
	midas_stat->g->gsize[p->n->naive_start] += midas_stat->g->gsize[0];
	midas_stat->g->gsize[0]=0;
	if (p->active[0] != NULL) {
		midas_stat->g->gsize[p->n->naive_start]--;
		midas_stat->g->gsize[0]=1;
	}
	q_free(p->group[0]);
	p->group[0] = NULL;
	//TODO p->m->config[i]?
}

void hf_reset(int flag) {
	memset(hotfilter->cur_hf, 0, sizeof(int)*jy_LBANUM);
	hotfilter->left_tw = hotfilter->tw;
	if (flag==1) hotfilter->make_flag=1;
	hotfilter->use_flag=0;
}

void hf_update_model(double traffic) {
	if (traffic==0.0) return;
	hot_q->g0_traffic_queue[hot_q->queue_idx] = traffic;
	hot_q->g0_size_queue[hot_q->queue_idx] = midas_stat->g->gsize[0];
	hot_q->g0_valid_queue[hot_q->queue_idx] = hotfilter->G0_vr_sum/hotfilter->G0_vr_num;
	hot_q->queue_idx++;
	if (hot_q->queue_idx == hot_q->queue_max) hot_q->queue_idx = 0;
	
	return;
}

void hf_update() {
	double prev_seg_age=0.0;
	double tr=0.0;
	tr = hotfilter->G0_traffic/hotfilter->tot_traffic;
	hotfilter->G0_traffic_ratio=tr;

	hf_update_model(tr);
	prev_seg_age = hotfilter->tw/hotfilter->tw_ratio;

	double avg_g0 = 0.0;
	hotfilter->avg_seg_age=(double)hotfilter->seg_age/(double)hotfilter->seg_num;
	avg_g0 = hotfilter->avg_seg_age/(double)_PPS/L2PGAP;

	hotfilter->avg_seg_age=(hotfilter->avg_seg_age+prev_seg_age)/2.0;
	avg_g0 = hotfilter->avg_seg_age/(double)_PPS/L2PGAP;
	long tw = (long)(hotfilter->avg_seg_age*hotfilter->tw_ratio);

	hotfilter->tw=tw;
	hotfilter->left_tw=tw-1;
	hotfilter->seg_age=0.0;
	hotfilter->seg_num=0.0;
	hotfilter->G0_traffic=0.0;
	hotfilter->tot_traffic=0.0;
	hotfilter->G0_vr_sum=0.0;
	hotfilter->G0_vr_num=0.0;

	return;
}

void hf_generate(uint32_t lba, int gnum, int hflag) {
	pm_body *p=(pm_body*)page_ftl.algo_body;
	if ((hotfilter->left_tw<=0) && (hotfilter->seg_num)) hf_update();
	
	if (hotfilter->cold_tw>0) {
		//printf("[HF-NOTICE] COLD start end\n");
		hotfilter->cold_tw--;
		return;
	}
	if (hflag) {
		//update the LBA to the hotfilter
		hotfilter->left_tw--;
		if (gnum==1) {
			double seg_age=page_ftl.bm->jy_get_timestamp(page_ftl.bm, p->mapping[lba]/_PPS/L2PGAP);
			double tmp_age=hotfilter->tw/hotfilter->tw_ratio;
			if (seg_age <= tmp_age) {
				//hot lba	
				if (hotfilter->cur_hf[lba] <= hotfilter->max_val-1) {
					hotfilter->cur_hf[lba]++;
				}
			} else {
				//not hot lba
				if (hotfilter->cur_hf[lba]>0) {
					hotfilter->cur_hf[lba]--;
				}
			}
		}
	} else {
		if (hotfilter->cur_hf[lba]>0) {
			hotfilter->cur_hf[lba]--;
			uint32_t gnum = seg_get_ginfo(p->mapping[lba]/_PPS/L2PGAP);
			if (gnum == 0) hotfilter->cur_hf[lba]--;
		}
	}	
	return;
}

void hf_reduce(uint32_t lba, int gnum) {
	pm_body *p=(pm_body*)page_ftl.algo_body;
	if (hotfilter->cur_hf[lba]>0) {
		hotfilter->cur_hf[lba]--;
		if (seg_get_ginfo(p->mapping[lba]/_PPS/L2PGAP)) hotfilter->cur_hf[lba]--;
	}
}


int hf_check(uint32_t lba) {
	hotfilter->tot_traffic++;
	if (hotfilter->cur_hf[lba] >= hotfilter->hot_val) {
		hotfilter->G0_traffic++;
		return 0;
	} else return 1;
}

