#include <stdint.h>

typedef struct group_info {
	uint32_t *gsize;

        double *tmp_vr;
        double *tmp_erase;
        double *cur_vr;
}G_VAL;

typedef struct err_info {
	bool errcheck;
	bool collect;
	
	uint32_t errcheck_time;

	uint32_t err_start;
	uint32_t err_window;
	double *vr;
	double *erase;
} ERR;

typedef struct stats {
	uint32_t cur_req;
	uint32_t write_gb;
	uint32_t write;
	uint32_t copy;
	uint32_t erase;

	uint32_t tmp_write;
	uint32_t tmp_copy;

	double tmp_waf;
	
	ERR *e;
	G_VAL *g;
}STAT;


void naive_mida_on();
void naive_mida_off();
void stat_init();
void stat_clear();
void errstat_clear();
void print_stat();

int change_group_number(int prevnum, int newnum);
int merge_group(int group_num);
int decrease_group_size(int gnum, int block_num);

int check_applying_config(double calc_waf);
int check_modeling();
int err_check();
int do_modeling();
