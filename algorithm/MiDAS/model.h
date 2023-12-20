#include "../../include/container.h"
#include "../../interface/interface.h"
#include "../../include/data_struct/redblack.h"

#include <pthread.h>
#include <math.h>
#include <limits.h>
#include <string.h>

#include <stdint.h>

#define M_WRITE 0
#define M_REMOVE 1


typedef struct miniature_model {
	uint32_t lba_sampling_ratio; //sampling ratio
	uint32_t interval_unit_size; //1 interval unit size, # of segment
	unsigned long long time_window; // time window size, # of interval count
	uint32_t entry_num;

	//uint32_t fnumber; //number of checking first interval
	//uint32_t* checking_first_interval; //interval counts to check first interval
	unsigned long long first_count; //update counts for checked first interval
	uint32_t live_lba;

	pthread_t thread_id;

	unsigned long long* time_stamp; //time stamp
	int* hot_lba; //hot lba bitmap for Desnoyer
	unsigned long long* model_count; // counting update count per intervals
	
	uint32_t fnumber;
	uint32_t *checking_first_interval;
	Redblack rb_lbas; // for first interval's LBAs
	queue *fqueue; //insert LBAS for first interval thread
	pthread_t fthread;
	queue *latest_lbas;
	bool first_done;
}mini_model;

/* manage time window and interval unit size
 * time: # of interval count
 */
typedef struct model_time {
	bool is_real;
	unsigned long long time_window;
	unsigned long long current_time; //time, # of interval count
	uint32_t request_time; //time, # of requests, max: 128*512
	unsigned long long load_timing; //to remove sequential write in model
	uint32_t interval_unit_size;
	unsigned long long extra_load; //in real-world workload, wait extra time after LOAD_END signal
}mtime;

/* used when modeling is over & get new group configuration
 * valid: true if the configuration information is valid
 * when midas use that info and change confi, valid must turn off to false
 */
typedef struct group_configuration{
	bool valid; // true if valid info
	int gnum;
	double *vr;
	double *commit_vr;
	int *app_flag;
	int *app_size;
	int commit_g;
	uint32_t* gsize;
	double WAF;
	double g0_traffic;
}G_INFO;


//midas

// jeeyun
void model_create(int write_size);
void time_managing(char mode);
void model_initialize();
int check_time_window(uint32_t lba, char mode);
int check_interval(uint32_t lba, char mode);
void *first_interval_analyzer(void* arg);
//int check_first_interval(uint32_t lba, char mode);
void *making_group_configuration(void *arg);
void print_config(int, uint32_t*, double, double*, double);
void print_config_into_log(int, uint32_t*, double, double*);
void print_config2(int, uint32_t*, double, double*);
double WAF_predictor(double *, int);
double hot_WAF_predictor(double *valid_ratio_list, int group_num);
unsigned long long resizing_model();
double *valid_ratio_predictor(uint32_t*, uint32_t, unsigned long long);
double *hot_valid_ratio_predictor(uint32_t*, uint32_t, unsigned long long, int);
void initialize_first_interval();
void remove_first_interval();
void model_destroy();
int update_count(uint32_t lba, char mode);

// Desnoyer

double FIFO_predict(double op);
double None_FIFO_predict(double op);

double one_group_predictor(uint32_t*, uint32_t, unsigned long long);
