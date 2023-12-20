#include "midas.h"
#include "map.h"
#include "model.h"
#include "page.h"
#include "hot.h"

extern algorithm page_ftl;
extern STAT* midas_stat;
extern G_INFO *G_info;

void naive_mida_on() {
	printf("=> NAIVE ON!!!!\n");
	pm_body *p = (pm_body*)page_ftl.algo_body;
	p->n->naive_on=true;
	p->gnum = p->gnum+GNUMBER-1;

	p->gcur = p->n->naive_start;
	//use active segment
	for (int i=0;i<p->active_q->size;i++) {
		if (i == GNUMBER) break;
		if (p->active[p->n->naive_start+1+i] != NULL) {
			printf("why there is active block? \n");
			printf("in midas.c file\n");
			abort();
		}
		p->active[p->n->naive_start+1+i] = (__segment*)q_dequeue(p->active_q);
		midas_stat->g->gsize[p->n->naive_start+1+i]++;
		uint32_t res = seg_assign_ginfo(p->active[p->n->naive_start+1+i]->seg_idx, p->n->naive_start+1+i);
		p->gcur++;
		if (res != UINT_MAX) {
			printf("there is ginfo in segment in queue\n");
			abort();
		}
		if (i == 2) break;
	}
}


void naive_mida_off() {
	printf("=> NAIVE OFF!!!!\n");
	pm_body *p = (pm_body*)page_ftl.algo_body;
	queue* q = p->n->naive_q;
	
	p->n->naive_on=false;

	//change group info	
	node *cur = q->head;
	uint32_t seg_idx;
	uint32_t tsize = 0;
	for (int i=p->n->naive_start;i<p->gnum;i++) {
		tsize += midas_stat->g->gsize[i];
		if (p->active[i] != NULL) tsize -= 1;
	}
	if (p->n->naive_start == 1) {
		tsize += midas_stat->g->gsize[0];
		if (p->active[0] != NULL) tsize--;
	}
	
	if (q->size != tsize) {
		printf("queue size err in naive_mida_off()\n");
		abort();
	}
	//assign ginfo to naive_start
	for (int i=0;i<q->size;i++) {
		seg_idx = page_ftl.bm->jy_get_block_idx(page_ftl.bm, cur->d.req);
		seg_assign_ginfo(seg_idx, p->n->naive_start);
		cur = cur->next;
	}

	//add active to queue
	for (int i=p->n->naive_start+1;i<p->n->naive_start+GNUMBER;i++) {
		midas_stat->g->gsize[p->n->naive_start] += midas_stat->g->gsize[i];
		midas_stat->g->gsize[i]=0;
		if (p->active[i] == NULL) continue;
		q_enqueue((void*)p->active[i], p->active_q);
		seg_assign_ginfo(p->active[i]->seg_idx, UINT_MAX);
		p->active[i]=NULL;
		midas_stat->g->gsize[p->n->naive_start]--; //active block
	}

	//move G0 size to G0 queue
	if (p->n->naive_start == 1) {
		midas_stat->g->gsize[p->n->naive_start] += midas_stat->g->gsize[0];
		midas_stat->g->gsize[0]=0;
		if (p->active[0] != NULL) {
			midas_stat->g->gsize[0]++;
			midas_stat->g->gsize[p->n->naive_start]--;
		}
		q_init(&(p->group[0]), _NOS);
	}	

	if (p->gcur > p->n->naive_start) p->gcur = p->n->naive_start;
	p->gnum = p->gnum-(GNUMBER-1);
}

void stat_init() {
	midas_stat = (STAT*)calloc(sizeof(STAT), 1);
	midas_stat->tmp_waf=0.0;

	midas_stat->g = (G_VAL*)malloc(sizeof(G_VAL));
	midas_stat->g->gsize = (uint32_t*)calloc(sizeof(uint32_t), MAX_G);
	midas_stat->g->tmp_vr = (double*)calloc(sizeof(double), MAX_G);
	midas_stat->g->tmp_erase = (double*)calloc(sizeof(double), MAX_G);
	midas_stat->g->cur_vr = (double*)calloc(sizeof(double), MAX_G);
	
	midas_stat->e = (ERR*)malloc(sizeof(ERR));
	midas_stat->e->errcheck=false;
	midas_stat->e->collect=false;

	midas_stat->e->errcheck_time=0;
	midas_stat->e->err_start=TIME_WINDOW/3;
	midas_stat->e->err_window=TIME_WINDOW/2;
	midas_stat->e->vr = (double*)calloc(sizeof(double), MAX_G);
	midas_stat->e->erase = (double*)calloc(sizeof(double), MAX_G);
}

void stat_clear() {
	midas_stat->tmp_write=0;
	midas_stat->tmp_copy=0;
	for (int i=0;i<MAX_G;i++) {
		midas_stat->g->tmp_vr[i]=0.0;
		midas_stat->g->tmp_erase[i]=0.0;
	}
}

void errstat_clear() {
	midas_stat->e->errcheck=false;
	midas_stat->e->collect=false;
	midas_stat->e->errcheck_time=0;
	for (int i=0;i<MAX_G;i++) {
		midas_stat->e->vr[i]=0.0;
		midas_stat->e->erase[i]=0.0;
	}
}

void print_stat() {
	pm_body *p = (pm_body*)page_ftl.algo_body;
	printf("===============================\n");
	printf("[progress: %dGB]\n", midas_stat->write_gb);
	midas_stat->tmp_waf = (double)(midas_stat->tmp_write+midas_stat->tmp_copy)/(double)(midas_stat->tmp_write);
	printf("TOTAL WAF:\t%.3f, TMP WAF:\t%.3f\n", (double)(midas_stat->write+midas_stat->copy)/(double)midas_stat->write, midas_stat->tmp_waf);
	for (int i=0;i<p->gnum; i++) {
		midas_stat->g->cur_vr[i] = midas_stat->g->tmp_vr[i]/midas_stat->g->tmp_erase[i];
		printf("  GROUP %d[%d]: %.4f (ERASE:%.0f)\n", i, midas_stat->g->gsize[i], midas_stat->g->cur_vr[i], midas_stat->g->tmp_erase[i]);
	}
	stat_clear();
}

//TODO change naive_start and max group number before using this function
int change_group_number(int prevnum, int newnum) {
	//change group size
	if (midas_stat->g->gsize[newnum] != 0) {
		printf("there is still some blocks in Group %d\n", newnum);
		abort();
		return 1;
	}
	
	//get queue pointer
	pm_body *p = (pm_body*)page_ftl.algo_body;
	if (p->group[newnum] != NULL) {
		printf("there is still queue in Group %d\n", newnum);
		abort();
		return 1;
	}
	queue* q=p->n->naive_q;

	//change group number info
        if (q->size != (midas_stat->g->gsize[prevnum]-1)) {
                printf("queue size err in change_group_number()\n");
                abort();
        }
	node *cur = q->head;
	uint32_t seg_idx;
	for (int i=0;i<q->size;i++) {
		seg_idx = page_ftl.bm->jy_get_block_idx(page_ftl.bm, cur->d.req);
                seg_assign_ginfo(seg_idx, newnum);
                cur = cur->next;
	}
	for (int i=p->n->naive_start;i<newnum;i++) q_init(&(p->group[i]), _NOS);
	
	//change active segment
	if (p->active[newnum] != NULL) {
		printf("there is a active block : change_group_number() in midas.c\n");
		abort();
	}
	midas_stat->g->gsize[newnum] = midas_stat->g->gsize[prevnum];
        midas_stat->g->gsize[prevnum]=0;
	if (prevnum != 1) {
		p->active[newnum] = p->active[prevnum];
		p->active[prevnum] = NULL;
		seg_assign_ginfo(p->active[newnum]->seg_idx, newnum);
	} else {
		midas_stat->g->gsize[newnum]--;
		midas_stat->g->gsize[prevnum]++;
	}
	p->n->naive_start = newnum;
	p->gnum = newnum+1;
	return 0;
}

//TODO after merge_group() function, then you can change naive_start value
int merge_group(int group_num) {
	pm_body *p = (pm_body*)page_ftl.algo_body;
	int last_g = p->n->naive_start;

	node *cur;
	queue* q;
	uint32_t seg_idx;
	//change group number info
	for (int j=group_num+1;j<last_g+1;j++) {
	        if (j==last_g) q=p->n->naive_q;
		else q=p->group[j];
		cur = q->head;
        	if ((q->size+1) != midas_stat->g->gsize[j]) {
			printf("queue size err in merge_group()\n");
	                abort();
	        }
		for (int i=0;i<q->size;i++) {
	                seg_idx = page_ftl.bm->jy_get_block_idx(page_ftl.bm, cur->d.req);
	                seg_assign_ginfo(seg_idx, group_num);
                	cur = cur->next;
        	}
	}

	//move group queue to heap
	int size;
	for (int i=group_num;i<last_g;i++) {
		size = page_ftl.bm->jy_move_q2h(page_ftl.bm, p->group[i], 0);
		if ((size+1) != midas_stat->g->gsize[i]) {
			printf("size miss: stat->gsize and real queue size is different\n : midas.cpp merge_group()\n");
			abort();
		}
		//edit size
        	midas_stat->g->gsize[last_g] += midas_stat->g->gsize[i];
        	midas_stat->g->gsize[i]=0;
		//free queue
		q_free(p->group[i]);
		p->group[i] = NULL;
	}
	//final size edit
	midas_stat->g->gsize[group_num] = midas_stat->g->gsize[last_g];
	midas_stat->g->gsize[last_g]=0;
	
	//save active block
	for (int i=group_num+1;i<last_g+1;i++) {
		if (p->active[i] == NULL) continue;
                q_enqueue((void*)p->active[i], p->active_q);
                seg_assign_ginfo(p->active[i]->seg_idx, UINT_MAX);
                p->active[i]=NULL;
                midas_stat->g->gsize[group_num]--; //active block
	}
	//change group number
	p->gnum = group_num+1;
	p->n->naive_start = group_num;

	if (p->n->naive_start == 1) {
		hot_merge();
	}

	return group_num;
}

int decrease_group_size(int gnum, int block_num) {
	//change group size
	pm_body *p = (pm_body*)page_ftl.algo_body;
	int last_g = p->n->naive_start;
        queue* q = p->group[gnum];

        //change group number info
        if ((q->size+1) != midas_stat->g->gsize[gnum]) {
                printf("queue size err in naive mida off()\n");
                abort();
        }

	midas_stat->g->gsize[last_g] += block_num;
        midas_stat->g->gsize[gnum] -= block_num;
        node *cur = q->head;
        uint32_t seg_idx;
        for (int i=0;i<block_num;i++) {
                seg_idx = page_ftl.bm->jy_get_block_idx(page_ftl.bm, cur->d.req);
                seg_assign_ginfo(seg_idx, last_g);
                cur = cur->next;
        }

	//move queue node to heap
	int size = page_ftl.bm->jy_move_q2h(page_ftl.bm, p->group[gnum], block_num);
	if (size != block_num) {
		printf("queue size err\n");
		abort();
	}
        return 0;
}

int check_applying_config(double calc_waf) {
	printf("\n");
	//if (midas_stat->tmp_waf-midas_stat->tmp_waf*0.05 < calc_waf) {
	if (midas_stat->tmp_waf < calc_waf) {
		printf("NOT NEED TO CHANGE:: NEW: %.3f, CUR: %.3f\n", calc_waf, midas_stat->tmp_waf);
		return 0;
	} else {
		printf("NEED TO CHANGE:: NEW: %.3f, CUR: %.3f\n", calc_waf, midas_stat->tmp_waf);
                return 1;
		//return 0;
	}
}

int check_modeling() {
	if (G_info->valid==false) return 0;
	pm_body *p = (pm_body*)page_ftl.algo_body;
	if (check_applying_config(G_info->WAF)==0) {
		G_info->valid=false;
		midas_stat->e->errcheck=true;
		return 0;
	}

	if (p->n->naive_on) naive_mida_off();

	//manage the number of groups
	if (p->gnum > G_info->gnum) {
		//TODO need to be small
		printf("=> MERGE GROUP : G%d ~ G%d\n", G_info->gnum-1, p->gnum-1);
		merge_group(G_info->gnum-1);
	} else if (p->gnum < G_info->gnum) {
		//TODO need to be big
		printf("=> MAKE NEW GROUP: G%d -> G%d\n", p->n->naive_start, G_info->gnum-1);
		change_group_number(p->n->naive_start, G_info->gnum-1);
	}

	memcpy(p->m->config, G_info->gsize, sizeof(uint32_t)*MAX_G);
	memcpy(p->m->vr, G_info->vr, sizeof(double)*MAX_G);
	p->m->WAF = G_info->WAF;
	p->m->status=true;
	G_info->valid=false;
	midas_stat->e->errcheck=true;
	//manage the size of groups
	for (int i=0;i<p->gnum-1;i++) {
		if (p->m->config[i] < midas_stat->g->gsize[i]) {
			printf("=> MANAGE GROUP SIZE: G%d (size %u -> %u)\n", i, midas_stat->g->gsize[i], p->m->config[i]);
			decrease_group_size(i, midas_stat->g->gsize[i]-p->m->config[i]);
		}
	}	
	return 1;
}



int err_check() {
	pm_body *p = (pm_body*)page_ftl.algo_body;
	if (p->m->status==false) return 0;
	printf("\n==========ERR check==========\n");
	for (int i=0;i<p->gnum;i++) {
		if ((p->n->naive_on == true) && (i == p->n->naive_start)) break;
		double vr = midas_stat->e->vr[i]/midas_stat->e->erase[i];
		printf("[GROUP %d] calc vr: %.3f, real vr: %.3f", i, p->m->vr[i], vr);
		if ((p->m->vr[i]+0.1 > vr) && (p->m->vr[i]-0.1 < vr)) {
			printf("\t(O)\n");
			continue;
		} else {
			printf("\t(X)\n");
			printf("!!!IRREGULAR GROUP: %d!!!\n", i);
			if (i==p->gnum-1) {
				if (p->n->naive_on==false) naive_mida_on();
			} else {
				if (p->n->naive_on) naive_mida_off();
       				printf("=> MERGE GROUP : G%d ~ G%d\n", i, p->gnum-1);
        			for (int j=i+1;j<p->gnum;j++) p->m->config[i] += p->m->config[j];
				merge_group(i);
				naive_mida_on();
			}
			errstat_clear();
			return 1;
		}
	}
	errstat_clear();
	return 1;
}

int check_configuration_apply() {
	pm_body *p = (pm_body*)page_ftl.algo_body;
	if (p->n->naive_start==0) return 1;
	for (int i=0;i<p->n->naive_start;i++) {
		if (midas_stat->g->gsize[i] != p->m->config[i]) {
			if (midas_stat->g->gsize[i]-1 != p->m->config[i]) return 0;
		}
	}
	return 1;
}


int do_modeling() {
	if (midas_stat->cur_req%GB_REQ==0) {
                //errcheck timing
		//ginfo valid check timing
		midas_stat->write_gb++;
                //printf("\rwrite size: %dGB", midas_stat->write_gb);
                if (midas_stat->e->errcheck) {
                        if (midas_stat->e->errcheck_time == midas_stat->e->err_start) {
				if (check_configuration_apply()) {
					printf("\n=> CONFIGURATION check: OK, ((error check on))\n");
					midas_stat->e->collect=true;
					midas_stat->e->errcheck_time++;
				}
				//else printf("\n=> CONFIGURATION check: NO\n");
			} else {
				midas_stat->e->errcheck_time++;
			}
                }
                int st = check_modeling();
                if (st) printf("!!!UID modeling over & adapt configuration!!!\n");
        }
        if ((midas_stat->write_gb%(GIGAUNIT)==0) && (midas_stat->cur_req%GB_REQ==0)) {
                //printf("\n");
                print_stat();
        }
        if ((midas_stat->e->errcheck_time==midas_stat->e->err_window) && (midas_stat->cur_req%GB_REQ==0)) {
                err_check();
        }

	return 0;
}

