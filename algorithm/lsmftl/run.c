#include "run.h"

run *run_init(uint32_t sst_file_num, uint32_t start_lba, uint32_t end_lba){
	run *res=(run*)calloc(1,sizeof(run));
	res->now_sst_num=0;
	res->max_sst_num=sst_file_num;
	res->start_lba=start_lba;
	res->end_lba=end_lba;
	res->now_contents_num=0;
	res->sst_set=(sst_file*)calloc(sst_file_num, sizeof(sst_file));
	return res;
}

void run_space_init(run *res, uint32_t map_num, uint32_t start_lba, uint32_t end_lba){
	res->max_sst_num=map_num;
	res->now_sst_num=0;
	res->start_lba=start_lba;
	res->end_lba=end_lba;
	res->sst_set=(sst_file*)calloc(map_num, sizeof(sst_file));
}

void run_reinit(run *res){
	res->now_sst_num=0;
	res->start_lba=UINT32_MAX;
	res->end_lba=0;
	res->now_contents_num=0;
	uint32_t sidx;
	sst_file *sptr;
	for_each_sst(res, sptr, sidx){	
		sst_reinit(sptr);
	}
}

static inline void update_range(run *_run, uint32_t start, uint32_t end){
	if(_run->start_lba > start) _run->start_lba=start;
	if(_run->end_lba < end) _run->end_lba=end;
}


sst_file *run_retrieve_sst(run *r, uint32_t lba){
	int s=0, e=r->now_sst_num-1;
	while(s<=e){
		int mid=(s+e)/2;
		if(r->sst_set[mid].start_lba<=lba && r->sst_set[mid].end_lba>=lba){
			return &r->sst_set[mid];
		}
		if(r->sst_set[mid].start_lba > lba){
			e=mid-1;
		}
		else if(r->sst_set[mid].end_lba < lba){
			s=mid+1;
		}
	}
	return NULL;
}

sst_file *run_retrieve_close_sst(run *r, uint32_t lba){
	int s=0, e=r->now_sst_num-1;
	int mid;
	bool is_right=false;
	while(s<=e){
		mid=(s+e)/2;
		if(r->sst_set[mid].start_lba<=lba && r->sst_set[mid].end_lba>=lba){
			EPRINT("overlap not allowed",true);
			return NULL;
		}
		if(r->sst_set[mid].start_lba > lba){
			e=mid-1;
			is_right=false;
		}
		else if(r->sst_set[mid].end_lba < lba){
			s=mid+1;
			is_right=true;
		}
	}

	if(is_right) return &r->sst_set[mid];
	else{ 
		return mid-1<0? NULL:&r->sst_set[mid-1];
	}
}

static inline void __do_run_append_sstfile(run *_run, sst_file *sstfile, bool moved_originality){
	if(_run->now_sst_num>=_run->max_sst_num){
		/*EPRINT("over sst file", true);*/
		sst_file *new_sst_set=(sst_file*)calloc(_run->max_sst_num*2, sizeof(sst_file));
		memcpy(new_sst_set, _run->sst_set, sizeof(sst_file) * _run->now_sst_num);
		free(_run->sst_set);
		_run->sst_set=new_sst_set;
		_run->max_sst_num=_run->max_sst_num*2;
	}
	if(_run->now_sst_num && !(sstfile->start_lba > _run->start_lba && sstfile->start_lba>_run->end_lba)){
		EPRINT("cannot append!", true);
	}
	if(moved_originality){
		sst_shallow_copy_move_originality(&_run->sst_set[_run->now_sst_num], sstfile);
	}
	else{
		sst_shallow_copy(&_run->sst_set[_run->now_sst_num], sstfile);
	}
	update_range(_run, sstfile->start_lba, sstfile->end_lba);
	if(sstfile->_read_helper){
		_run->now_contents_num+=read_helper_get_cnt(sstfile->_read_helper);
	}
	_run->now_sst_num++;
}

void run_append_sstfile_move_originality(run *_run, sst_file *sstfile){
	__do_run_append_sstfile(_run, sstfile, true);
}

void run_append_sstfile(run *_run, sst_file *sstfile){
	__do_run_append_sstfile(_run, sstfile, false);
}

void run_deep_append_sstfile(run *_run, sst_file *sstfile){
	if(_run->now_sst_num>_run->max_sst_num){
		EPRINT("over sst file", true);
	}
	if(_run->now_sst_num && !(sstfile->start_lba > _run->start_lba && sstfile->start_lba>_run->end_lba)){
		EPRINT("cannot append!", true);
	}
	sst_deep_copy(&_run->sst_set[_run->now_sst_num], sstfile);
	update_range(_run, sstfile->start_lba, sstfile->end_lba);
	_run->now_contents_num+=read_helper_get_cnt(sstfile->_read_helper);
	_run->now_sst_num++;
}

void run_free(run *_run){
	free(_run->sst_set);
	free(_run);
}

void run_print(run *rptr){
	printf("lba:%u~%u sst_file num:%u/%u\n",rptr->start_lba, rptr->end_lba,
			rptr->now_sst_num, rptr->max_sst_num);
}

void run_content_print(run *r, bool print_sst){
	sst_file *sptr;
	uint32_t sidx;
	run_print(r);
	if(print_sst){
		for_each_sst(r, sptr, sidx){
			printf("\t\t[%u] ",sidx);
			sst_print(sptr);
		}
	}
}

map_range *run_to_MR(run *r, uint32_t *map_num){
	uint32_t count=0;
	sst_file *sptr;
	uint32_t i;
	for_each_sst(r, sptr, i){
		if(sptr->type==PAGE_FILE){
			count++;
		}
		else{
			count+=sptr->map_num;
		}
	}

	map_range *res=(map_range*)malloc(sizeof(map_range) * count);
	uint32_t res_idx=0, map_idx=0;
	map_range *mptr;
	for_each_sst(r, sptr, i){
		if(sptr->type==PAGE_FILE){
			res[res_idx].start_lba=sptr->start_lba;
			res[res_idx].end_lba=sptr->end_lba;
			res[res_idx].ppa=sptr->file_addr.map_ppa;
			res[res_idx].data=NULL;
			res_idx++;
		}
		else{
			for_each_map_range(sptr, mptr, map_idx){
				res[res_idx]=*mptr;
				res_idx++;
			}
		}
	}
	*map_num=count;
	return res;
}
