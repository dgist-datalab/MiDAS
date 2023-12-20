#include "model.h"
#include "hot.h"
#include "map.h"
#include "midas.h"

extern algorithm page_ftl;
mini_model *mmodel; 
mtime mm_time;
extern G_INFO *G_info;
extern HF_Q* hot_q;
extern HF *hotfilter;
extern int jy_LBANUM;
extern STAT* midas_stat;

extern uint32_t utilization;
char workload_name[64] = "tmp";

bool model_on;
#define DES 1//desnoyer, DES=2 means Greedy

int one_op = 0;
double exp_avg = 0;
unsigned long long hot_req_count = 0;
unsigned long long tot_req_count = 0;
double hot_lba_count = 0;
double tot_lba_count = 0;
unsigned long long f_count=0;
int modeling_num = -1;

/* calculate WAF value using valid ratio per groups
 * values = valid ratio per groups
 * return: calculated WAF
 */
double test_WAF_predictor() {
	int gnum = 6;
	double *valid_ratios = (double*)malloc(sizeof(double)*gnum);
	double values[6] = {0.660701, 0.782772, 0.839476, 0.833228, 0.840996, 0.90478};
	//double values[10] = {0.68, 0.76, 0.83, 0.84, 0.84, 0.87, 0.83, 0.88, 0.90, 0.91};
	memcpy(valid_ratios, values, sizeof(double)*gnum);
	double predicted_WAF = WAF_predictor(valid_ratios, gnum);
	
	printf("predicted_WAF : %f\n", predicted_WAF);
	abort();
	return predicted_WAF;
}

/*initialize model*/
void model_create(int write_size) {
	/* sampling info, unit size, time window size */
	//test_WAF_predictor();
	mmodel = (mini_model*)malloc(sizeof(mini_model));
	mmodel->lba_sampling_ratio=100;
	mmodel->interval_unit_size=1; //segment
	if (write_size == 0)
		mmodel->time_window=44000*1024/(_PPS*L2PGAP)*1024/4/mmodel->interval_unit_size; //800GB/(PPB*BPS)
	else
		mmodel->time_window = write_size*1024/(_PPS*L2PGAP)*1024/4/mmodel->interval_unit_size;

	//checking max entry
	//mmodel->entry_num = 4*1024*1024/(_PPS*L2PGAP)*1024/4/mmodel->interval_unit_size;
	//if (mmodel->time_window <= mmodel->entry_num) mmodel->entry_num = mmodel->time_window;
	mmodel->entry_num = mmodel->time_window;

	/*to check first interval
	 * fnumber: # of interval to check
	 * finterval: index of interval to check
	 */
	mmodel->fnumber=10; 
	uint32_t finterval[10] = {100, 105, 110, 115, 120, 125, 130,135, 140,145};
	/*
        uint32_t finterval[100] = {0};
	for (int i=0;i<mmodel->fnumber; i++){	
		finterval[i] = i+100; //CHECK: change index like fnumber!!!!!!!
	}
	*/
	mmodel->checking_first_interval = (uint32_t*)calloc(mmodel->fnumber, sizeof(uint32_t));
	memcpy(mmodel->checking_first_interval, finterval, sizeof(uint32_t)*mmodel->fnumber);
	if (mmodel->lba_sampling_ratio > 1) {
		mmodel->first_count = 0;
		q_init(&(mmodel->fqueue), _PPS*L2PGAP*mmodel->interval_unit_size);
		mmodel->live_lba=0;
		//int stf = pthread_create(&mmodel->fthread, NULL, first_interval_analyzer, NULL);
		//if (stf != 0) perror("model can't make first interval thread\n");
	}
	/* real or not */
	mm_time.is_real=false;
	
	//TODO have to set interval_unit seg to block
	mm_time.time_window=mmodel->time_window;
	mm_time.interval_unit_size = mmodel->interval_unit_size;	
	mm_time.current_time=0;
	mm_time.request_time=0;
	mm_time.load_timing=0;
	mm_time.extra_load=0/4*1024*1024; //unitsize: page

	printf("*** Update Interval Distribution SETTINGS ***\n- LBA sampling rate: %.2f\n", 1.0/(float)mmodel->lba_sampling_ratio);
	printf("- UID interval unit size: %d segments (%d pages)\n", mmodel->interval_unit_size, mmodel->interval_unit_size*(_PPS));
	printf("- Epoch size: %.2fGB (%llu units)\n", mmodel->time_window/1024.*(_PPS*L2PGAP)/1024.*mmodel->interval_unit_size*4, mmodel->time_window);
	//printf("- real mode: %s \n", mm_time.is_real?"true":"false");
	//printf("- extra load timing: %lluGB */\n\n", mm_time.extra_load/1024/1024*4);
	/*making time stamp list and interval count list*/
	
	mmodel->time_stamp = (unsigned long long*)malloc(sizeof(unsigned long long)*jy_LBANUM/mmodel->lba_sampling_ratio);
	mmodel->hot_lba = (int*)malloc(sizeof(int)*jy_LBANUM/mmodel->lba_sampling_ratio);
	for (int i=0; i<jy_LBANUM/mmodel->lba_sampling_ratio; i++){
		mmodel->time_stamp[i]=UINT_MAX;
		mmodel->hot_lba[i]=0;
	}
	
	//TODO make max entry (for too many time window)
	mmodel->model_count = (unsigned long long*)calloc(mmodel->entry_num, sizeof(unsigned long long));

	G_info = (G_INFO*)malloc(sizeof(G_INFO));
	G_info->valid=false;
	G_info->gnum = 0;
	G_info->commit_g = 19;
	G_info->vr = (double*)calloc(15, sizeof(double));
	G_info->commit_vr = (double*)calloc(15, sizeof(double));
	G_info->gsize = (uint32_t*)calloc(15, sizeof(uint32_t));
	G_info->app_size = (int*)calloc(20, sizeof(int));
	memset(G_info->app_size, -1, sizeof(int)*20);
	G_info->app_flag = (int*)calloc(20, sizeof(int));
	G_info->WAF = 100.0;
	model_on=true;
}

/* initialize structure for making miniature model
 */
void model_initialize() {
	for (int i=0;i<jy_LBANUM/mmodel->lba_sampling_ratio; i++) {
		mmodel->time_stamp[i] = UINT_MAX;
		mmodel->hot_lba[i]=0;
	}
	for (int i=0;i<mmodel->entry_num; i++) {
		mmodel->model_count[i] = 0;
	}
	
	if (mmodel->lba_sampling_ratio > 1) {
		mmodel->first_count=0;
		//int stf = pthread_create(&mmodel->fthread, NULL, first_interval_analyzer, NULL);
		//if (stf != 0) perror("model can't make first interval thread\n");
	}

	mm_time.current_time=0;
	mm_time.request_time=0;

	one_op=0;
	hot_req_count=0;
	tot_req_count=0;
	hot_lba_count=0;
	tot_lba_count=0;

	model_on=true;

}

/* update time */
void time_managing(char mode) {
	if (mm_time.current_time == mm_time.time_window) return;
	mm_time.request_time++;
        if (mm_time.request_time == (_PPS*L2PGAP)*mm_time.interval_unit_size) {
                mm_time.current_time++;
                mm_time.request_time=0;
		//printf("cur time: %d\n", mm_time.current_time);
        }
	//printf("req time: %d, cur time: %d\n", mm_time.request_time, mm_time.current_time);
}

/* checking time window
 * if return is 0, model is not ready
 * if return is 1, run model
 */
bool run_signal=true;
int check_time_window(uint32_t lba, char mode) {
	if (model_on == false) return 0;
	/* time window end */
	if (mm_time.current_time == mm_time.time_window) {
		return 0;
	}

	/* time update */
        if (mm_time.is_real) {
                /* need to check if this is after LOAD_END */
                if (run_signal) {
			if (mm_time.load_timing < mm_time.extra_load) mm_time.load_timing++;
			else time_managing(mode);
			
		}
        } else {
		if (mm_time.load_timing < (unsigned long)jy_LBANUM+mm_time.extra_load) {
			mm_time.load_timing++;
		}
                else {
                        /* after sequential write */
                        time_managing(mode);
                }
        }

        /* time window */
        if ((mm_time.current_time == 0) && (mm_time.request_time == 0)) return 0;
	check_interval(lba, M_WRITE);
	return 1;
}

/* checking lba's interval */
int check_interval(uint32_t lba, char mode) {
	/* fixing first interval's count
	 * in this time, we only check one interval unit */
	

	for (int i=0; i<mmodel->fnumber; i++) {
		if (mm_time.current_time < mmodel->checking_first_interval[i]) break;
		if (mm_time.current_time == mmodel->checking_first_interval[i]) {
			//check_first_interval(lba, mode);
			bool st = false;
                	while (!st) {
	                        st = q_enqueue_int(lba, mmodel->fqueue);
	                }
			if (mm_time.request_time == 1) {
				int stf = pthread_create(&mmodel->fthread, NULL, first_interval_analyzer, NULL);
       	         		if (stf != 0) perror("model can't make first interval thread\n");
			}
		}
	}

	/*	
	if (mmodel->lba_sampling_ratio > 1) {
		bool st = false;
		while (!st) {
			st = q_enqueue_int(lba, mmodel->fqueue);
		}
	}
	*/

	if (mm_time.current_time >= mm_time.time_window) {
		/* collecting count is done
		 * now we have to update last interval count, and make optimal group configuration
		 */
		model_on=false;
		//printf("\n!!!!!ALERT: model creation done!!!!\n\n");
		modeling_num += 1;
		int status = pthread_create(&mmodel->thread_id, NULL, making_group_configuration, NULL);
		if (status != 0) {
			perror("miniature model can't make thread\n");
			abort();
		}
	}

	/* sampling */
	if (lba%mmodel->lba_sampling_ratio) return 0;
        lba = lba/mmodel->lba_sampling_ratio;
	update_count(lba, mode);


	return 1;
}

/* updating count by lba */
int update_count(uint32_t lba, char mode) {
	//TODO
	//check if run correctly
	unsigned long long cur_interval=0;
	unsigned long long tmp_stamp=0;
	if (mmodel->time_stamp[lba] == UINT_MAX) {
	       //first accessed
		if (mode == M_WRITE) {
			mmodel->time_stamp[lba] = mm_time.current_time; 
			mmodel->time_stamp[lba] = mmodel->time_stamp[lba] << 1;
			//first access flag
			mmodel->time_stamp[lba]++;
		}
		tot_req_count ++;
	} else if (mmodel->time_stamp[lba] == UINT_MAX-1) {
		//deleted before this
		if (mode == M_WRITE) {
			mmodel->time_stamp[lba] = mm_time.current_time;
	       		mmodel-> time_stamp[lba] = mmodel->time_stamp[lba]<< 1;
		}
		tot_req_count ++;
	} else if ((mmodel->time_stamp[lba] & 1) == 1) {
		//second accessed
		tmp_stamp = mmodel->time_stamp[lba] >> 1;
		cur_interval = mm_time.current_time - tmp_stamp;
		if (cur_interval >= mmodel->entry_num) cur_interval = mmodel->entry_num-1;
		//add 2
		if (cur_interval < 0) {
			printf("interval number is strange!!!! interval:  %llu\n", cur_interval);
			abort();
		}
		if (mode == M_WRITE) {
			mmodel->time_stamp[lba] = mm_time.current_time;
		        mmodel->time_stamp[lba] = mmodel->time_stamp[lba]<< 1;
			mmodel->model_count[cur_interval] += 1;
		} else {
			mmodel->time_stamp[lba] = UINT_MAX-1;
			mmodel->model_count[cur_interval] ++;
		}
	} else if ((mmodel->time_stamp[lba] & 1) == 0) {
		//or else
		tmp_stamp = mmodel->time_stamp[lba] >> 1;
		cur_interval = mm_time.current_time - (uint32_t) tmp_stamp;
		if (cur_interval >= mmodel->entry_num) cur_interval = mmodel->entry_num-1;
		if (cur_interval < 0) {
                	//printf("interval number is strange!!!! interval:  %llu\n", cur_interval);
                        abort();
                }

		mmodel->model_count[cur_interval]++;
		if (mode == M_WRITE) {
			mmodel->time_stamp[lba] = mm_time.current_time;
			mmodel->time_stamp[lba] = mmodel->time_stamp[lba]<< 1;
		} else {
			mmodel->time_stamp[lba] = UINT_MAX-1;
		}
	} else {
		printf("model count err: there is extra case here\n");
		printf("time stamp: %llu (lba: %d, time: %llu)\n", mmodel->time_stamp[lba], lba, mm_time.current_time);
		abort();
	}
	return 0;
}

/*complete miniature model and make optimal group configuration using markov chain */
void *making_group_configuration(void *arg) {
	/* resizing interval count using time stamp (first group, last group, other groups) */
	unsigned long long total_count = 0;
	total_count = resizing_model();

	hf_q_calculate();
	pm_body *p = (pm_body*)page_ftl.algo_body;

	/*making group configuration */
	// there is an opt group config and a valid ratio list for every #ofgroups
	uint32_t **opt_config = (uint32_t**)malloc(sizeof(uint32_t*)*10);
        double **opt_valid_ratio_list = (double**)malloc(10*sizeof(double*));
        for (int i=0;i<10;i++) {
                opt_config[i] = (uint32_t*)calloc(10, sizeof(uint32_t)); //tmp group configuration
                opt_valid_ratio_list[i] = (double*)calloc(10, sizeof(double));
        }
        opt_config[0][0] = _NOS-FREENUM;
        opt_valid_ratio_list[0][0] = 1.0;

	double opt_traffic=0.0;
        double opt_waf[10] = {10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 10000.0};
        
	//tmp config and tmp valid ratio list
	uint32_t *group_config = (uint32_t*)calloc(10, sizeof(uint32_t));
        double *valid_ratio_list;

	uint32_t gnum=0;
	//save final config and valid ratio list (optimal in every #ofgroups)
        double final_waf = 100.0;
        uint32_t final_gnum=0;
        uint32_t *final_config;
        double *final_valid_ratio_list;
	double final_traffic=0.0;

	//valid_ratio_predictor(opt_config[0], 1, total_count); // valid_ratio_predictor should excuted before one_group_predictor)
	one_op = 1; // printing option for one group valid ratio predict
	//opt_waf[0] = (double) one_group_predictor(opt_config[0], 1, total_count); // for check one group's valid ratio(WAF)

	int g0_desig_size=1;
	double hot_thresh=0.05;
	memcpy(group_config, opt_config[0], sizeof(uint32_t)*10);
	if (hot_q->g0_size > 1.0) {
		if (hot_q->g0_valid > 0.18) {
			printf("MODEL valid ratio is too high!!!\n");
			printf("===MODELING END===\n");
			model_initialize();
			return (void*)0;
		}
	} else {
		if (hot_q->g0_valid > 0.3) {
			printf("MODEL valid ratio is too high!!!\n");
			printf("===MODELING END===\n");
			model_initialize();
			return (void*)0;
		}
	}
	if (midas_stat->g->gsize[p->gnum-1] < 10) {
		//printf("there is no last group segment,,,,,, return\n");
		//printf("===MODELING END===\n");
		//model_initialize();
		//return (void*)0;
	}

	if (hot_q->is_fix==true) {
		g0_desig_size=(int)hot_q->g0_size;
		group_config[0] -= g0_desig_size;
		hot_q->calc_unit=0;
		valid_ratio_list = hot_valid_ratio_predictor(group_config, 1, total_count, g0_desig_size);
		if (valid_ratio_list==NULL) {
			printf("why valid ratio is NULL!!\n");
			model_initialize();
			return (void*)0;
		}
		double predicted_WAF = hot_WAF_predictor(valid_ratio_list, 1);
		if (predicted_WAF > 100) {
			printf("valid ratio of group 1 is too high\n");
			model_initialize();
			return (void*)0;
		}
		opt_waf[0] = predicted_WAF;
		opt_config[0][0] = group_config[0];
		opt_valid_ratio_list[0][0]=valid_ratio_list[0];
		hot_q->best_extra_size = g0_desig_size;
		hot_q->best_extra_traffic = hot_q->calc_traffic;
		opt_traffic = hot_q->g0_traffic;
	} else {
		//printf("!!!!!!!!!!HOT GROUP SIZE FIX!!!!!!!!!!\n");
		for (int i=1;i<30;i++) {
			if (i < hot_q->calc_size) continue;
			group_config[0] = _NOS-FREENUM - i;
			hot_q->calc_unit=0;
			valid_ratio_list = hot_valid_ratio_predictor(group_config, 1, total_count, i);
			if (valid_ratio_list==NULL) {
				printf("NULL in %d\n", i);
				break;
			}
			double predicted_WAF = hot_WAF_predictor(valid_ratio_list, 1);
			//printf("%d ", hot_q->calc_size);
			//print_config2(1, group_config, predicted_WAF, valid_ratio_list);
			//printf("calc traffic: %.3f\n", hot_q->calc_traffic);
			if (predicted_WAF < opt_waf[0]-hot_thresh) {
				hot_q->best_extra_size=hot_q->calc_size;
				hot_q->best_extra_traffic=hot_q->calc_traffic;
				hot_q->best_extra_unit = hot_q->calc_unit;
				opt_waf[0] = predicted_WAF;
				opt_config[0][0] = group_config[0];
				opt_valid_ratio_list[0][0] = valid_ratio_list[0];
				opt_traffic=hot_q->calc_traffic;
				free(valid_ratio_list);
			} else {
				free(valid_ratio_list);
				break;
			}
		}
		hot_q->calc_traffic=hot_q->best_extra_traffic;
		g0_desig_size=hot_q->best_extra_size;
		hot_q->calc_unit=hot_q->best_extra_unit;
	}

	int group_add_flag=0;
	uint32_t max_groupsize=opt_config[0][0]-50;
	uint32_t tmp_maxsize = max_groupsize;
	/* making group configurations */
	//printf("***phase 1***\n");
	print_config2(1, opt_config[0], opt_waf[0], opt_valid_ratio_list[0]);
	for (int i=0; i<8; i++) {
		gnum = i+2;
		group_add_flag=0;
		memcpy(group_config, opt_config[i], sizeof(uint32_t)*10);

		group_config[i+1] = group_config[i];
		group_config[i] = 1;
		max_groupsize = tmp_maxsize;
		for (int j=1;j<max_groupsize; j++) {
			group_config[i]=j;
			group_config[i+1] -= 1;
			if (group_config[i+1]==0) {
				printf("*****ALERT**** last group is 0\n");
				for (int q=0;q<gnum;q++) printf("%d ", group_config[q]);
				printf("\n");
				abort();
				break;
			}
			/* predict valid ratio for the group configuration */
			valid_ratio_list = hot_valid_ratio_predictor(group_config, gnum, total_count, g0_desig_size);
			if (valid_ratio_list == NULL) {
				//TODO check
				//printf("valid ratio list is NULL?\n");
				//print_config2(gnum, opt_config, opt_waf, opt_valid_ratio_list);
				//printf("break\n");
				break;
			} else {
				/* calculate WAF using markov chain */
				double predicted_WAF = hot_WAF_predictor(valid_ratio_list, gnum);
				//if (valid_ratio_list != NULL) print_config2(gnum, group_config, predicted_WAF, valid_ratio_list);
				if (opt_waf[i+1] > predicted_WAF) {
					//opt_gnum = gnum;
					opt_waf[i+1] = predicted_WAF;
					memcpy(opt_config[i+1], group_config, sizeof(uint32_t)*10);
					memcpy(opt_valid_ratio_list[i+1], valid_ratio_list, sizeof(double)*10);
					group_add_flag=1;
					tmp_maxsize = max_groupsize - opt_config[i+1][i];
				}
				free(valid_ratio_list);
			}
		}
		if (opt_waf[i+1] < 100.0) {
			printf("%d ", g0_desig_size);
			print_config2(gnum, opt_config[i+1], opt_waf[i+1], opt_valid_ratio_list[i+1]);
		}
		memcpy(group_config, opt_config[i+1], sizeof(uint32_t)*10);
		//gnum = opt_gnum;
		/*
		if (group_add_flag==0) {
			break;
		}
		*/
	}

	for (int i=0;i<10;i++) {
                if (final_waf > opt_waf[i]) {
                        final_gnum = i+1;
                        final_config = opt_config[i];
                        final_valid_ratio_list = opt_valid_ratio_list[i];
                        final_waf = opt_waf[i];
                }
                else {
                        free(opt_config[i]);
                        free(opt_valid_ratio_list[i]);
                }
        }
	free(opt_config);
	free(opt_valid_ratio_list);
        printf("==phase 1 result==\n");
	printf("%d ", g0_desig_size);
        print_config2(final_gnum, final_config, final_waf, final_valid_ratio_list);
        printf("==================\n");

	//printf("***phase 2***\n");

	/* group configuration phase 2*/
	int initial_last_size=0;
	for (int n=0;n<4;n++) {
		gnum = final_gnum;

		if (hot_q->is_fix==false) {
			memcpy(group_config, final_config, sizeof(uint32_t)*10);
			gnum=final_gnum;
			group_config[gnum-1] += g0_desig_size;
			initial_last_size=group_config[gnum-1];
			//printf("HOT group size check\n");
			hot_q->calc_size=0;
			for (int i=1;i<30;i++) {
				if (i <= hot_q->calc_size) continue;
				group_config[gnum-1] = initial_last_size-i;
				hot_q->calc_unit=0;
				valid_ratio_list = hot_valid_ratio_predictor(group_config, gnum, total_count, i);
				if (valid_ratio_list == NULL) break;
				double predicted_WAF = hot_WAF_predictor(valid_ratio_list, gnum);
				if (predicted_WAF < final_waf) {
					hot_q->best_extra_size=hot_q->calc_size;
					hot_q->best_extra_traffic=hot_q->calc_traffic;
					hot_q->best_extra_unit=hot_q->calc_unit;
					final_waf = predicted_WAF;
					memcpy(final_config, group_config, sizeof(uint32_t)*10);
					memcpy(final_valid_ratio_list, valid_ratio_list, sizeof(double)*10);
					g0_desig_size=i;
					opt_traffic = hot_q->calc_traffic;
				}
				free(valid_ratio_list);
			}
		}
		gnum=final_gnum;
		hot_q->calc_unit=hot_q->best_extra_unit;
		memcpy(group_config, final_config, sizeof(uint32_t)*10);
		for (int i=0;i<8;i++) {
			if (i < gnum-1) {
				group_config[gnum-1] += group_config[i];
			} else {
				group_config[i+1] = group_config[i];
				gnum++;
			}
			max_groupsize = group_config[gnum-1]-50;
			group_add_flag=0;
			group_config[i] = 0;
			for (int j=1;j<max_groupsize; j++) {
				group_config[i] = j;
group_config[gnum-1] -= 1;
				valid_ratio_list = hot_valid_ratio_predictor(group_config, gnum, total_count, g0_desig_size);
				if (valid_ratio_list == NULL) {
					//printf("valid ratio list is NULL \n");
					if (j == 1) {
						printf("phase 2 didn't work\n");
						//printf("%d ", g0_desig_size);
						//print_config2(gnum, final_config, final_waf, final_valid_ratio_list);
					}
                                	break;
				}
				double predicted_WAF = hot_WAF_predictor(valid_ratio_list, gnum);
				if (final_waf > predicted_WAF) {
	                                final_gnum = gnum;
        	                        final_waf = predicted_WAF;
                	                memcpy(final_config, group_config, sizeof(uint32_t)*10);
                        	        memcpy(final_valid_ratio_list, valid_ratio_list, sizeof(double)*10);
                                	group_add_flag=1;
                        	}
				free(valid_ratio_list);
			}
			//print_config2(opt_gnum, opt_config, opt_waf, opt_valid_ratio_list);
                	memcpy(group_config, final_config, sizeof(uint32_t)*10);
                	if (group_add_flag==0) {
				if (i >= final_gnum-1) break;
                	}
			
		}
		//printf("%d ", g0_desig_size);
		//print_config2(final_gnum, final_config, final_waf, final_valid_ratio_list);
	}
	//printf("FINAL RESULTS\n");
	//print_config(opt_gnum, opt_config, opt_waf, opt_valid_ratio_list);
	//print_config_into_log(final_gnum, final_config, final_waf, final_valid_ratio_list);
	//abort();
	final_gnum++;
	for (int i=final_gnum-2;i>=0;i--) {
		final_config[i+1]=final_config[i];
		final_valid_ratio_list[i+1] = final_valid_ratio_list[i];
	}
	final_config[0]=hot_q->best_extra_size;
	final_valid_ratio_list[0]=0.0;
	final_traffic=opt_traffic;

	//set group information
	free(group_config);
	//free(valid_ratio_list);
	if (final_gnum>2) {
		printf("ginfo check: ");
		while(G_info->valid == true) {}
		printf("DONE\n");
		memcpy(G_info->gsize, final_config, sizeof(uint32_t)*10);
		memcpy(G_info->vr, final_valid_ratio_list, sizeof(double)*10);
		G_info->gnum = final_gnum;
		G_info->WAF = final_waf;
		G_info->g0_traffic=final_traffic;
		G_info->valid=true;
	}
	hf_q_reset(true);
	print_config(G_info->gnum, G_info->gsize, G_info->WAF, G_info->vr, final_traffic);
	model_initialize();

	return (void*)0;
}


/* print final results by miniature model */
void print_config(int gnum, uint32_t *opt_config, double opt_waf, double *opt_valid_ratio_list, double opt_traffic) {
	printf("*****GCS Group Configuration RESULTS*****\n");
	printf("group number: %d\n", gnum);
	for (int i=0;i<gnum; i++) printf("*group %d: size %d, valid ratio %f\n", i, opt_config[i], opt_valid_ratio_list[i]);
	printf("calculated WAF: %f\n", opt_waf);
	printf("calculated Traffic: %.3f\n", opt_traffic);
	printf("************************************\n");
}

void print_config_into_log(int gnum, uint32_t *opt_config, double opt_waf, double *opt_valid_ratio_list) {
	char l[128] = "logging/log_";
        strcat(l, workload_name);
        FILE* mFile = fopen(l, "a");
	fprintf(mFile, "*****MODEL PREDICTION RESULTS*****\n");
        fprintf(mFile, "group number: %d\n", gnum);
        for (int i=0;i<gnum; i++) fprintf(mFile, "*group %d: size %d, valid ratio %f\n", i, opt_config[i], opt_valid_ratio_list[i]);
        fprintf(mFile, "calculated WAF: %f\n", opt_waf);
        fprintf(mFile, "************************************\n");
	fclose(mFile);
}

void print_config2(int gnum, uint32_t *opt_config, double opt_waf, double *opt_valid_ratio_list) {
	for (int i=0;i<gnum; i++) printf("%d ", opt_config[i]);
	printf(" : ");
	for (int i=0;i<gnum;i++) printf("%f ", opt_valid_ratio_list[i]);
	printf("(%f)\n", opt_waf);
}

/* predict WAF by group's valid ratios */
double WAF_predictor(double *valid_ratio_list, int group_num) {
	double **valid_matrix = (double**)malloc(sizeof(double*)*(group_num+1));
	double *valid_ratio_list2 = (double*)malloc(sizeof(double)*(group_num+1));
	valid_ratio_list2[0] = 1.0;
	memcpy(&valid_ratio_list2[1], valid_ratio_list, sizeof(double)*group_num);
	//TODO check new valid ratio list
	
	for (int i=0;i<(group_num+1); i++) {
		double *tmp = (double*)calloc(group_num+1, sizeof(double));
		tmp[0] = 1.0-valid_ratio_list2[i];
		//TODO if (i==group_num-1), tmp[group_num] = ?
		if (i==0) tmp[1]=1.0;
		else if (i==group_num) tmp[i] = valid_ratio_list2[i];
		else tmp[i+1] = valid_ratio_list2[i];
		valid_matrix[i]=tmp;
	}
	double *base = (double*)calloc(group_num+1, sizeof(double));
	base[0]=_NOP*0.1;
	base[1]=_NOP*0.9;
	double *result = (double*)calloc(group_num+1, sizeof(double));
	memcpy(result, base, sizeof(double)*(group_num+1));
	uint32_t past=0;
	uint32_t write=0;
	double total_wr=0.0;
	double tmp_tot_wr=0.0;

	for (int i=0;i<2000;i++) {
		tmp_tot_wr=0;
		memcpy(base, result, sizeof(double)*(group_num+1));
		memset(result, 0, sizeof(double)*(group_num+1));
		//dot operation
		for (int j=0;j<(group_num+1);j++) {
			for (int k=0;k<(group_num+1);k++) {
				result[j] += base[k]*valid_matrix[k][j];
			}
		}

		for (int j=0;j<(group_num-1);j++) {
			total_wr += result[j+2];
			tmp_tot_wr += result[j+2];
		}
		past = result[0];
		write += past;
	}
	double WAF = (tmp_tot_wr+(double)past)/(double)past;
	//printf("WAF: %f\n", WAF);

	/* free memory */
	free(result);
	free(base);
	for (int i=0;i<(group_num+1);i++) free(valid_matrix[i]);
	free(valid_ratio_list2);
	free(valid_matrix);


	return WAF;
}

/* predict WAF by group's valid ratios */
double hot_WAF_predictor(double *valid_ratio_list, int group_num) {
	double **valid_matrix = (double**)malloc(sizeof(double*)*(group_num+2));
	double *valid_ratio_list2 = (double*)malloc(sizeof(double)*(group_num+2));
	valid_ratio_list2[0] = 1.0;
	
	if (hot_q->is_fix==true) valid_ratio_list2[1] = hot_q->g0_valid;
	else valid_ratio_list2[1] = 0.0;

	memcpy(&valid_ratio_list2[2], valid_ratio_list, sizeof(double)*group_num);
	//TODO check new valid ratio list
	
	for (int i=0;i<(group_num+2); i++) {
		double *tmp = (double*)calloc(group_num+2, sizeof(double));
		tmp[0] = 1.0-valid_ratio_list2[i];
		//TODO if (i==group_num-1), tmp[group_num] = ?
		if (i==0) {
			if (hot_q->is_fix==true) tmp[1] = hot_q->g0_traffic;
			else tmp[1] = hot_q->calc_traffic;
			tmp[2] = 1.0-tmp[1];
		}
		else if (i==group_num+1) tmp[i] = valid_ratio_list2[i];
		else tmp[i+1] = valid_ratio_list2[i];
		valid_matrix[i]=tmp;
	}
	double *base = (double*)calloc(group_num+2, sizeof(double));
	base[0]=_NOP*0.1;
	base[1]=_NOP*0.9;
	double *result = (double*)calloc(group_num+2, sizeof(double));
	memcpy(result, base, sizeof(double)*(group_num+2));
	uint32_t past=0;
	double tmp_tot_wr=0.0;

	for (int i=0;i<2000;i++) {
		memcpy(base, result, sizeof(double)*(group_num+2));
		memset(result, 0, sizeof(double)*(group_num+2));
		//dot operation
		for (int j=0;j<(group_num+2);j++) {
			for (int k=0;k<(group_num+2);k++) {
				result[j] += base[k]*valid_matrix[k][j];
			}
		}
	}
	if (group_num==1) {
		tmp_tot_wr += result[2]-result[0]*valid_ratio_list2[1];
	} else {
		for (int j=0;j<(group_num-1);j++) {
			tmp_tot_wr += result[j+3];
		}
		tmp_tot_wr += result[1]*valid_ratio_list2[1];
	}
	past = result[0];

	double WAF = (tmp_tot_wr+(double)past)/(double)past;
	//printf("WAF: %f\n", WAF);

	/* free memory */
	free(result);
	free(base);
	for (int i=0;i<(group_num+1);i++) free(valid_matrix[i]);
	free(valid_ratio_list2);
	free(valid_matrix);

	return WAF;
}


/* resize model's interval counts (first group, last group, other groups) */
unsigned long long resizing_model() {

	/* first interval unit */
	//printf("here\n");	
	//char m2[128] = "modeling/modeling_prev_";
	
	//strcat(m2, workload_name);
	/* before resizing
	for (int i=0;i<mmodel->entry_num-1;i++) {
		fprintf(pFile, "%d %llu\n", i, mmodel->model_count[i]);
	}
	*/

	/* last interval unit */
        //mmodel->model_count[mmodel->time_window-1] = utilization/mmodel->lba_sampling_ratio;
	for (int i=0;i<jy_LBANUM/mmodel->lba_sampling_ratio; i++) {
                if ((mmodel->time_stamp[i] != UINT_MAX) && ((mmodel->time_stamp[i]&1) == 1) && (mmodel->time_stamp[i] != UINT_MAX-1)) {
                        mmodel->model_count[mmodel->entry_num-1]++;
                }
        }
	//fprintf(pFile, "%d %llu\n", mmodel->entry_num-1, mmodel->model_count[mmodel->entry_num-1]);
	//fclose(pFile);	
	unsigned long long tot=0;
	/* first interval unit */
	if (mmodel->lba_sampling_ratio > 1) {
		//printf("first interval analyzer check: ");
		while (!(mmodel->first_done)) {}
		//printf("DONE\n");
		mmodel->model_count[0] = mmodel->first_count*mmodel->time_window/mmodel->fnumber/mmodel->lba_sampling_ratio;
	}
	tot += mmodel->model_count[0];
	//printf("first interval calculate done\n");
	double left_time = 0.0;
	//tot += mmodel->model_count[mmodel->time_window-1];
	/* other interval units */
	for (int i=1;i<mmodel->entry_num;i++) {
		//mmodel->model_count[i] = mmodel->model_count[i] + (int)((double)((int)mmodel->time_window%(i))/(double)(i)*mmodel->model_count[i])/(int)(mmodel->time_window/(i));
		left_time = ((double)(mmodel->time_window%i*2+i)/2.0)/(double)i;
		mmodel->model_count[i] = mmodel->model_count[i] + (int)((left_time*mmodel->model_count[i])/((double)mmodel->time_window/(double)(i)));
		//mmodel->model_count[i] = mmodel->model_count[i] + (int)((0.5*mmodel->model_count[i])/((double)mmodel->time_window/(double)(i)));
		tot += mmodel->model_count[i];
	}
	//fprintf(mFile, "%d %u\n", mmodel->time_window-1, mmodel->model_count[mmodel->time_window-1]);



	//printf("total count: %llu\n", tot);
	//printf("utilization: %u\n", utilization);
	return tot;
}
/*
double FIFO_predict(double op){
	return op/(op+lambert_w0(-op*exp(-op)));	
}
*/

double None_FIFO_predict(double op){
	if(op<=1){
		return 0.;
	}
	double r = (double) hot_req_count / (double) tot_req_count;
	double f = (double) hot_lba_count / (double) tot_lba_count;
//	printf("Un-uniform FIFO prediction, tot_req_count: %lld, hot_req_count: %lld, op: %.2f, r: %.4f, f: %.4f, hot_lba_count: %.4f, tot_lba: %.4f\n", tot_req_count, hot_req_count, op, r, f, hot_lba_count, tot_lba_count);
	
	if (one_op == 1){
		//printf("Un-uniform FIFO prediction, op: %.2f, r: %.4f, f: %.4f\n", 1/(double)op, r, f);
		one_op = 0;
	}
	double inc_waf = 1.;
	double test_val = 1.;
	//int lines = 0;
	while(test_val > 0){
		test_val = 1 + r/double(exp(double(r*op)/double(f*inc_waf))-1) + (1-r)/double(exp(double((1-r)*op)/double(double(1-f)*inc_waf))-1)-inc_waf;
		inc_waf += 1;
	//	lines += 1;
/*		if (inc_waf < 20) {
			printf("inc_waf: %.3f, test_val = %.3f\n", inc_waf, test_val);
			printf("op: %.3f, r: %.3f, f: %.3f\n", op, r, f);
		}
*//*		if (inc_waf > 50 && inf_flag ==0){
			printf("!!infinite loop!!\n");
			inf_flag = 1;
		}
*/	}
	inc_waf -= 2;
	test_val = 1.;
	while(test_val > 0){
		test_val = 1 + r/double(exp(double(r*op)/double(f*inc_waf))-1) + (1-r)/double(exp(double((1-r)*op)/double(double(1-f)*inc_waf))-1)-inc_waf;
		inc_waf += 0.1;
		//printf("inc_waf: %.3f\n", inc_waf);
	}
	inc_waf -= 0.2;
	test_val = 1.;
	while(test_val > 0){
		test_val = 1 + r/double(exp(double(r*op)/double(f*inc_waf))-1) + (1-r)/double(exp(double((1-r)*op)/double(double(1-f)*inc_waf))-1)-inc_waf;
		inc_waf += 0.01;
		//printf("inc_waf: %.3f\n", inc_waf);
	}
	inc_waf -= 0.02;
	test_val = 1.;
	while(test_val > 0){
		test_val = 1 + r/double(exp(double(r*op)/double(f*inc_waf))-1) + (1-r)/double(exp(double((1-r)*op)/double(double(1-f)*inc_waf))-1)-inc_waf;
		inc_waf += 0.001;
		//printf("op: %.2f, r: %.2f, f: %.2f, inc_waf: %.3f\n", op, r, f, inc_waf);
	}
	inc_waf -= 0.001;
	
	//printf("inc_waf: %.3f, test_val = %.3f\n", inc_waf, test_val);
	//printf("op: %.3f, r: %.3f, f: %.3f\n", op, r, f);
	return inc_waf;
	//return op/(op+lambert_w0(-op*exp(-op)));	
}


double one_group_predictor(uint32_t *group_config, uint32_t group_num, unsigned long long tot_cnt) {	//input values are same as vlid_ratio_predictor, return valid ratio
	double last_vr;
	double last_group_vp = (double)utilization;  //global var
	//printf("op: %.4f\n", (double) last_group_vp/((double)group_config[group_num-1]*(_PPS*L2PGAP)));
	double last_waf = None_FIFO_predict((double)group_config[group_num-1]*(_PPS*L2PGAP)/(double)last_group_vp);	//None_FIFO_predict()'s input is op 
	//printf("group_config[0] = %d\n", group_config[0]);
	if(group_num!=1){
		printf("Not one group, wrong function call\n");
	}
	if (DES == 1){	//FIFO
		last_vr = 1.-1./last_waf;
		return last_waf;
	}else if (DES == 2){	//Greedy
		last_waf = None_FIFO_predict((1+1/double(2*(_PPS*L2PGAP)))*(double)group_config[group_num-1]*(_PPS*L2PGAP)/(double)last_group_vp)/(1+1/double(2*(_PPS*L2PGAP)));	//Changed op is input
		last_vr = 1.-1./last_waf;
		return last_waf;
	}
	else{
		printf("One group predictor must be used with Desnoyer!\n");
		return -1;
	}
	
}


/* calculate valid ratio of the group configuration
 * input: group configuration, group number
 * output: valid ratio of groups
 */
double *valid_ratio_predictor(uint32_t *group_config, uint32_t group_num, unsigned long long tot_cnt) {
	double *valid_ratio_list = (double*)calloc(10, sizeof(double));
	unsigned long long *invalid_cnt_list = (unsigned long long*)calloc(10, sizeof(unsigned long long));
	
	uint32_t groups_seg_num=0;
	for (int i=0;i<group_num-1;i++) groups_seg_num += group_config[i];
	
	//groups_seg_num *= mmodel->interval_unit_size;
	//to save the valid ratio of last group
	double *last_vr_list = (double*)calloc(mm_time.time_window*mm_time.interval_unit_size, sizeof(double));
	double mig_rate = 1.0; //for calcuting group_time
	double ref_rate = 1.0;
	double tmp_vr=0.0;
	double valid_ratio=0.0;

	unsigned long long invalid_cnt = 0;
	double valid_cnt = 0.0;
	double global_valid_cnt = 0.0;

	double group_time = 0.0;
	//Jeeyun check segment time
	double one_invalid_cnt = 0.0;

	double past_group_time = -1.0;

	unsigned long long lindex=0;

	double evg_lifespan = 0.0;
	unsigned long long lg_req = 0;
	unsigned long long lifespan_num = 0;

	if (group_config[0] < mm_time.interval_unit_size) {
		for (int i=0;i<group_num;i++) valid_ratio_list[i] = 1.0;
		free(invalid_cnt_list);
		free(last_vr_list);
		return valid_ratio_list;
	}

	for (int i=0;i<group_num; i++) {
		invalid_cnt = 0;
		valid_cnt = 0.0;
		one_invalid_cnt=0.0;
		/* calculate group_time value 
		 * (increased when group number increase)
		 */
		mig_rate = 1.0;
		ref_rate = 1.0;
		for (int j=0;j<i;j++) {
			if (j==group_num-1) {
				double tmp = valid_ratio_list[group_num-2]+valid_ratio_list[group_num-1];
				mig_rate *= (1.0/(double)tmp);
				break;
			} else {
				mig_rate *= (1.0/valid_ratio_list[j]);
				ref_rate *= valid_ratio_list[j];
			}
		}
		if (i==0) group_time = (double)group_config[i]*(double)(_PPS*L2PGAP)*mig_rate-(_PPS*L2PGAP)/2;
		else group_time = (double)group_config[i]*(double)(_PPS*L2PGAP)*mig_rate-(_PPS*L2PGAP)*mig_rate/2;
		//error check: if number of segments of groups over the total time window = time window < device size
		if ((i!=group_num-1) && (((uint32_t)group_time+past_group_time) >= ((mmodel->time_window*mm_time.interval_unit_size-50)*(_PPS*L2PGAP)))) {
			//printf("something's wrong in valid ratio predictor: time window over\n");
			//printf("index: %d, configuration: [", i);
			//for
			free(valid_ratio_list);
			free(invalid_cnt_list);
			free(last_vr_list);
			return NULL;
		}

		/* add invalid counts
		 */
		double cur_interval;
		for (int j=0;j<mmodel->entry_num;j++) {
			cur_interval = (double)(j)*(double)(_PPS*L2PGAP)*(double)mm_time.interval_unit_size;
			if ((cur_interval >= past_group_time) && (cur_interval < group_time+past_group_time)) {
				
				invalid_cnt += mmodel->model_count[j];
				//for one group's valid ratio
				if(i == group_num-1){
                                        lg_req += mmodel->model_count[j]*cur_interval;  //make sum of (lifespan * count)
                                        lifespan_num += mmodel->model_count[j];
                                }	
				/*
				if (j*ssd_spec->PPS <= group_time+past_group_time-one_seg_time) {
					invalid_cnt += mmodel->model_count[j];
				} else one_invalid_cnt += mmodel->model_count[j];
				*/
				/*
				if (j*ssd_spec->PPS <= group_time+past_group_time-one_seg_time) {
					one_invalid_cnt += mmodel->model_count[j];
				}
				*/
				
				//for last group
				unsigned long long tmp2 = 0;
				for (int k=0;k<i; k++) tmp2 += invalid_cnt_list[k];
				
				if (i < group_num-1) {
					tmp_vr = 1.0-(double)(invalid_cnt)/(double)(tot_cnt-tmp2);
					global_valid_cnt += tmp_vr*ref_rate*(_PPS*L2PGAP)*mm_time.interval_unit_size;
				}else if (i==group_num-1) {
					//for calculate last group's valid ratio
					tmp_vr = 1.0-(double)(invalid_cnt)/(double)(tot_cnt-tmp2);
					if ((tmp_vr >= 1.0) || (tmp_vr < 0.0))	tmp_vr = 0.0;
					for (unsigned long long k=lindex; k<lindex+mm_time.interval_unit_size; k++) {
						last_vr_list[k] = tmp_vr;
					}
					lindex+=mm_time.interval_unit_size;
				}	
			}
		}
		// for Desnoyer for last group
		evg_lifespan = (double) lg_req / (double) lifespan_num ;  //caclulate the average of lifespan in Last group
		for (int j=0;j<mmodel->entry_num;j++) {
                        cur_interval = (double)(j)*(double)(_PPS*L2PGAP)*(double)mm_time.interval_unit_size;
                        if ((cur_interval >= past_group_time) && (cur_interval < group_time+past_group_time)) {
			if(i == group_num-1){
                                        tot_req_count += mmodel->model_count[j]; // caculate total request count in LAST GROUP
                                        double lba_num = (double) mmodel->model_count[j] / ((double) ((mmodel->time_window*mm_time.interval_unit_size-50)*(_PPS*L2PGAP)) / (double) cur_interval); //count lba that has lifespan as cur_interval
                                        tot_lba_count += lba_num; // caculate total lba count in LAST GROUP
                                        if(cur_interval <= evg_lifespan*0.7){   //check is this request hot
                                                hot_lba_count += lba_num;
                                                hot_req_count += mmodel->model_count[j];
                                        }
                                }
                        }
		}
		//update invalid count
		//CHECK invalid cnt is mean value of last one segment's invalid cnt
		//one_invalid_cnt = one_invalid_cnt;
		//invalid_cnt = (invalid_cnt+one_invalid_cnt);
		//invalid_cnt_list[i] = invalid_cnt+one_invalid_cnt;
		invalid_cnt_list[i] = invalid_cnt;
		//calculate valid ratio
		if (i != 0) {
			unsigned long long tmp3 = 0;
			for (int j=0;j<i;j++) tmp3 += invalid_cnt_list[j];
			valid_ratio = 1.0-(double)invalid_cnt/(double)(tot_cnt-tmp3);
		} else {
			valid_ratio = 1.0-(double)invalid_cnt/(double)tot_cnt;
		//TODO last group is useless, need to change code
		} 
		if (i < group_num-1) {
			valid_ratio_list[i] = valid_ratio;
			past_group_time += (group_time);
			if (i==0) past_group_time++;
		}
	}
	
	double last_group_vp = (double)utilization - global_valid_cnt;
	//printf("util: %d, global valid cnt: %.3f\n", utilization, global_valid_cnt);
	double last_group_vr = last_group_vp/(double)((group_config[group_num-1])*(_PPS*L2PGAP));
	//printf("last_group_vr: %f, (", last_group_vr);
	if (last_group_vr > 1.0) {
		//printf("last group vr is over 1\nlast-group vr: %.2f, spare space: %d\n", last_group_vp,(group_config[group_num-1])*(_PPS*L2PGAP));
		//printf("group confi: %d\n", group_config[group_num-1]);
		free(valid_ratio_list);
		free(invalid_cnt_list);
		free(last_vr_list);
		return NULL;
	}
	//double last_waf = FIFO_predict((double)group_config[group_num-1]*ssd_spec->PPS/(double)last_group_vp);
	double last_waf = None_FIFO_predict((double)group_config[group_num-1]*(_PPS*L2PGAP)/(double)last_group_vp);
	double last_vr;
	last_vr = 1.-1./last_waf;
	
	if (last_vr > 1.0) {
		//printf("last group vr is over 1 after Desnoyer\nlast-group vr: %.2f, spare space: %d\n", last_group_vp,(group_config[group_num-1])*(_PPS*L2PGAP));
		free(valid_ratio_list);
		free(invalid_cnt_list);
		free(last_vr_list);
		return NULL;
	}
	
	//printf("last_vr: %.3f %%\n", last_vr*100);
	//valid_ratio_list[group_num-1] = last_vr;
	
	if (DES == 1){ // FIFO
		valid_ratio_list[group_num-1] = last_vr;
		goto END;
	}
	if (DES == 2){ // Greedy
		last_waf = None_FIFO_predict((1+1/double(2*(_PPS*L2PGAP)))*(double)group_config[group_num-1]*(_PPS*L2PGAP)/(double)last_group_vp)/(1+1/double(2*(_PPS*L2PGAP)));
		last_vr = 1.-1./last_waf;
		valid_ratio_list[group_num-1] = last_vr;
		goto END;
	}
		
END:
//	if (valid_ratio_list[group_num-1]<=0) {
	for(int k = 0; k < group_num; k++){
		if (valid_ratio_list[k] <= 0.){	
			//printf("exit\n");
			free(valid_ratio_list);
			free(invalid_cnt_list);
			free(last_vr_list);
			return NULL;
		}
	}

	
/*	
	for (int i=0;i<group_num;i++) {
		printf("%d  ", group_config[i]);
	}
	printf("[");
	for (int i=0;i<group_num;i++) {
		printf("%f ", valid_ratio_list[i]);
	} printf("]\n");
*/	

	/* free memory */
	free(invalid_cnt_list);
	free(last_vr_list);

	
	return valid_ratio_list;
}

/* calculate valid ratio of the group configuration
 * input: group configuration, group number
 * output: valid ratio of groups
 */
double *hot_valid_ratio_predictor(uint32_t *group_config, uint32_t group_num, unsigned long long tot_cnt, int hot_size) {
	double *valid_ratio_list = (double*)calloc(10, sizeof(double));
	unsigned long long *invalid_cnt_list = (unsigned long long*)calloc(10, sizeof(unsigned long long));
	
	uint32_t groups_seg_num=0;
	for (int i=0;i<group_num-1;i++) groups_seg_num += group_config[i];
	
	//groups_seg_num *= mmodel->interval_unit_size;
	//to save the valid ratio of last group
	double *last_vr_list = (double*)calloc(mm_time.time_window*mm_time.interval_unit_size, sizeof(double));
	double mig_rate = 1.0; //for calcuting group_time
	double ref_rate = 1.0;
	double tmp_vr=0.0;
	double tmp_tr=0.0;
	double valid_ratio=0.0;

	unsigned long long invalid_cnt = 0;
	double valid_cnt = 0.0;
	double global_valid_cnt = 0.0;

	double group_time = 0.0;
	//Jeeyun check segment time
	double one_invalid_cnt = 0.0;

	double past_group_time = 0.0;
	double cur_interval=0.0;

	double evg_lifespan = 0.0;
	unsigned long long lg_req = 0;
	unsigned long long lifespan_num = 0;
	int start_idx=0;

	if (group_config[0] < mm_time.interval_unit_size) {
		for (int i=0;i<group_num;i++) valid_ratio_list[i] = 1.0;
		free(invalid_cnt_list);
		free(last_vr_list);
		return valid_ratio_list;
	}

	if (hot_q->is_fix) {
		//hot group size fixed
		mig_rate = 1.0/(hot_q->g0_traffic);
		ref_rate = hot_q->g0_traffic;
		group_time = (double)hot_q->g0_size*mig_rate-mig_rate/2.0;
		int desired_idx = group_time/mm_time.interval_unit_size+1;
		for (int i=0;i<desired_idx;i++) {
			invalid_cnt+=mmodel->model_count[i];
			tmp_vr = 1.0-(double)invalid_cnt/(double)tot_cnt;
			global_valid_cnt += tmp_vr*ref_rate*(double)mm_time.interval_unit_size*(double)(_PPS*L2PGAP);
		}
		start_idx = desired_idx;
		past_group_time += group_time;
		hot_q->calc_traffic=(double)invalid_cnt/(double)tot_cnt;
		tot_cnt -= invalid_cnt;
	} else {
		double extr_size=0.;
		//hot group size changed
		if (hot_q->calc_unit==0) {
			for (int i=0;i<mmodel->entry_num;i++) {
				invalid_cnt+=mmodel->model_count[i];
				tmp_tr = (double)invalid_cnt/(double)tot_cnt;
				tmp_vr = 1-tmp_tr;
				//TODO ref_rate? traffic? 
				global_valid_cnt += tmp_vr*(double)mm_time.interval_unit_size*(double)(_PPS*L2PGAP);
				cur_interval = (double)(i+1)*(double)mm_time.interval_unit_size;
				extr_size = floor((double)cur_interval*tmp_tr+0.5);
				if (extr_size >= hot_size) {
					start_idx = i+1;
					past_group_time += cur_interval;
					hot_q->calc_traffic=tmp_tr;
					tot_cnt -= invalid_cnt;
					if (extr_size > hot_size) {
						//printf("hot group size over: %.2f (+%.2f)\n", extr_size, extr_size-hot_size);
						group_config[group_num-1] -= (extr_size-hot_size);
					}
					hot_q->calc_size=extr_size;
					hot_q->calc_unit=i+1;
					break;
				}
			}
		} else {
			//has the prev information
			for (int i=0;i<hot_q->calc_unit;i++) {
				invalid_cnt += mmodel->model_count[i];
				tmp_vr = 1.0-(double)invalid_cnt/(double)tot_cnt;
				global_valid_cnt += tmp_vr*mm_time.interval_unit_size*hot_q->calc_traffic;
			}
			tmp_tr = (double)invalid_cnt/(double)tot_cnt;
			cur_interval = (double)hot_q->calc_unit*(double)mmodel->interval_unit_size;
			extr_size = floor((double)cur_interval*tmp_tr+0.5);
			past_group_time += cur_interval;
			start_idx = hot_q->calc_unit;
			hot_q->calc_traffic=tmp_tr;
			tot_cnt -= invalid_cnt;
			hot_q->calc_size = extr_size;
		}
	}

	double g1_traffic=0.0;
	if (hot_q->is_fix) {
		g1_traffic=1.0-hot_q->g0_traffic+(hot_q->g0_traffic*hot_q->g0_valid);
	} else g1_traffic=1.0-hot_q->calc_traffic;
	for (int i=0;i<group_num; i++) {
		invalid_cnt = 0;
		valid_cnt = 0.0;
		one_invalid_cnt=0.0;
		/* calculate group_time value 
		 * (increased when group number increase)
		 */
		mig_rate = 1.0/g1_traffic;
		ref_rate = g1_traffic;
		for (int j=0;j<i;j++) {
			if (j==group_num-1) {
				double tmp = valid_ratio_list[group_num-2]+valid_ratio_list[group_num-1];
				mig_rate *= (1.0/(double)tmp);
				break;
			} else {
				mig_rate *= (1.0/valid_ratio_list[j]);
				ref_rate *= valid_ratio_list[j];
			}
		}
		group_time = (double)group_config[i]*mig_rate-mig_rate/2.0;
		//error check: if number of segments of groups over the total time window = time window < device size
		if ((i!=group_num-1) && (((uint32_t)group_time+past_group_time) >= ((mmodel->time_window*mm_time.interval_unit_size-50)))) {
			//printf("something's wrong in valid ratio predictor: time window over\n");
			//printf("index: %d, configuration: [", i);
			//for
			//abort();
			free(valid_ratio_list);
			free(invalid_cnt_list);
			free(last_vr_list);
			return NULL;
		}

		/* add invalid counts
		 */
		for (int j=start_idx;j<mmodel->entry_num;j++) {
			cur_interval = (double)(j)*(double)mm_time.interval_unit_size;
			if (cur_interval < group_time+past_group_time) {
				
				invalid_cnt += mmodel->model_count[j];
				
				if (i < group_num-1) {
					tmp_vr = 1.0-(double)(invalid_cnt)/(double)tot_cnt;
					global_valid_cnt += tmp_vr*ref_rate*(_PPS*L2PGAP)*mm_time.interval_unit_size;
				}
				if (i==group_num-1) {
					lg_req += mmodel->model_count[j]*cur_interval;
					lifespan_num += mmodel->model_count[j];
				}
			} else {
				if (i != group_num-1) start_idx = j;
				break;
			}
		}
		// for Desnoyer for last group
		if (i==group_num-1) {
			evg_lifespan = (double) lg_req / (double) lifespan_num ;  //caclulate the average of lifespan in Last group
			for (int j=start_idx;j<mmodel->entry_num;j++) {
				cur_interval = (double)(j)*(double)mm_time.interval_unit_size;
				if ((cur_interval < group_time+past_group_time)) {
					tot_req_count += mmodel->model_count[j]; // caculate total request count in LAST GROUP
					double lba_num = (double) mmodel->model_count[j] / ((double) ((mmodel->time_window*mm_time.interval_unit_size-50)*(_PPS*L2PGAP)) / (double) cur_interval); //count lba that has lifespan as cur_interval
					tot_lba_count += lba_num; // caculate total lba count in LAST GROUP
                                        if(cur_interval <= evg_lifespan*0.7){   //check is this request hot
                                                hot_lba_count += lba_num;
                                                hot_req_count += mmodel->model_count[j];
                                        }
                                }
                        }
		}
		//update invalid count
		//CHECK invalid cnt is mean value of last one segment's invalid cnt
		//one_invalid_cnt = one_invalid_cnt;
		//invalid_cnt = (invalid_cnt+one_invalid_cnt);
		//invalid_cnt_list[i] = invalid_cnt+one_invalid_cnt;
		invalid_cnt_list[i] = invalid_cnt;
		valid_ratio = 1.0 - (double)invalid_cnt/(double)tot_cnt;
		tot_cnt -= invalid_cnt;
		//TODO last group is useless, need to change code
		if (i < group_num-1) {
			valid_ratio_list[i] = valid_ratio;
			past_group_time += (group_time);
		}
	}
	
	double last_group_vp = (double)utilization - global_valid_cnt;
	//printf("util: %d, global valid cnt: %.3f\n", utilization, global_valid_cnt);
	//double last_group_vr = last_group_vp/(double)((group_config[group_num-1])*(_PPS*L2PGAP));
	//printf("last_group_vr: %f, (", last_group_vr);
	if (last_group_vp < 0) {
		printf("last group vr is over 1\nlast-group vr: %.2f, spare space: %d\n", last_group_vp,(group_config[group_num-1])*(_PPS*L2PGAP));
		//printf("group confi: %d\n", group_config[group_num-1]);
		free(valid_ratio_list);
		free(invalid_cnt_list);
		free(last_vr_list);
		abort();
		return NULL;
	}
	//double last_waf = FIFO_predict((double)group_config[group_num-1]*ssd_spec->PPS/(double)last_group_vp);
	double last_waf = None_FIFO_predict((double)group_config[group_num-1]*(_PPS*L2PGAP)/(double)last_group_vp);
	double last_vr;
	last_vr = 1.-1./last_waf;
	
	if (last_vr > 1.0) {
		printf("last group vr is over 1 after Desnoyer\nlast-group vr: %.2f, spare space: %d\n", last_group_vp,(group_config[group_num-1])*(_PPS*L2PGAP));
		abort();
		free(valid_ratio_list);
		free(invalid_cnt_list);
		free(last_vr_list);
		return NULL;
	}
	
	//printf("last_vr: %.3f %%\n", last_vr*100);
	//valid_ratio_list[group_num-1] = last_vr;
	
	if (DES == 1){ // FIFO
		valid_ratio_list[group_num-1] = last_vr;
		goto END;
	}
	if (DES == 2){ // Greedy
		last_waf = None_FIFO_predict((1+1/double(2*(_PPS*L2PGAP)))*(double)group_config[group_num-1]*(_PPS*L2PGAP)/(double)last_group_vp)/(1+1/double(2*(_PPS*L2PGAP)));
		last_vr = 1.-1./last_waf;
		valid_ratio_list[group_num-1] = last_vr;
		goto END;
	}
		
END:
//	if (valid_ratio_list[group_num-1]<=0) {
	for(int k = 0; k < group_num; k++){
		if (valid_ratio_list[k] <= 0.){	
			//printf("exit\n");
			//abort();
			free(valid_ratio_list);
			free(invalid_cnt_list);
			free(last_vr_list);
			return NULL;
		}
	}

	
/*	
	for (int i=0;i<group_num;i++) {
		printf("%d  ", group_config[i]);
	}
	printf("[");
	for (int i=0;i<group_num;i++) {
		printf("%f ", valid_ratio_list[i]);
	} printf("]\n");
*/	

	/* free memory */
	free(invalid_cnt_list);
	free(last_vr_list);

	return valid_ratio_list;
}


/*initialize data structures for first interval search*/
void initialize_first_interval() {
	mmodel->rb_lbas = rb_create();
	//q_init(&(mmodel->latest_lbas), _PPS*L2PGAP*mm_time.interval_unit_size);
	mmodel->first_done = false;
}

/* fixing first interval's count
 * there is no sampling */
void *first_interval_analyzer(void* arg) {
	uint32_t request_time=0;
	uint32_t one_unit = _PPS*L2PGAP*mm_time.interval_unit_size;

	int lba=-1;
	int status=0;

	node *nd = NULL;
	Redblack cur_rb;

	initialize_first_interval();
	while (true) {
		if (request_time == one_unit) break;

		lba = q_dequeue_int_lba(mmodel->fqueue);
		if (lba == -1) continue;
		request_time++;
	
		/*
		nd = q_enqueue_int_node(lba, mmodel->latest_lbas);
		if (nd == NULL) {
			//latest queue is full!
			//dequeue
			tmp_lba = q_pick_int(mmodel->latest_lbas);
			status = rb_find_int(mmodel->rb_lbas, tmp_lba, &cur_rb);
			if (status==0) perror("first interval analyzer, why there is no node in redblack???\n");
			//the victim lba was the only lba in the queue
			if (cur_rb->item == (void*)q_dequeue_node(mmodel->latest_lbas)) {
				rb_delete_item(cur_rb, 0, 1);
			} else if (((node*)(cur_rb->item))->d.data != tmp_lba) {
				perror("first interval analyzer\n");
				abort();
			}
			nd = q_enqueue_int_node(lba, mmodel->latest_lbas);
		}
		*/
		status = rb_find_int(mmodel->rb_lbas, lba, &cur_rb);
		if (status) {
			mmodel->first_count++;
			cur_rb->item = (void*)nd;
		} else rb_insert_int(mmodel->rb_lbas, lba, (void*)nd);
	}
	remove_first_interval();
	return (void*)0;
}

/* fixing first interval's count
 * there is no sampling */
/*
int check_first_interval(uint32_t lba, char mode) {
	//initialize
	if (mm_time.request_time == 0) initialize_first_interval();
	//lba search
	int j=0;
	int indx=-1;
	while (mmodel->updated_lbas[j] != UINT_MAX) {
		if (mmodel->updated_lbas[j] == lba) {
			indx = j;
			break;
		}
		j++;
	}
	//lba check
	if (indx != -1) mmodel->first_count++;
	else mmodel->updated_lbas[j] = lba;

	if (mm_time.request_time == (_PPS*L2PGAP)*mm_time.interval_unit_size-1) {
		//remove
		remove_first_interval();
	}	
	return 1;
}
*/


void remove_first_interval() {
	rb_destroy(mmodel->rb_lbas, 1, 1, true);
	//q_free(mmodel->fqueue);
	//mmodel->fqueue=NULL;
	mmodel->first_done = true;
}


void model_destroy() {
	//free(mmodel->time_stamp);
	free(mmodel->model_count);
	free(mmodel);
}
