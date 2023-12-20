#ifndef __TAG_Q_H__
#define __TAG_Q_H__
#include <queue>
#include <set>
#include <pthread.h>
#include <stdint.h>
typedef struct tag_manager{
	std::queue<uint32_t> *tagQ;
	pthread_mutex_t tag_lock;
	pthread_cond_t tag_cond;
	uint32_t max_tag_num;
#ifdef FD_DEBUG
	std::set<uint32_t>* check_tag;
#endif
}tag_manager;

tag_manager *tag_manager_init(uint32_t tag_num);
uint32_t tag_manager_get_tag(tag_manager *);
void tag_manager_free_tag(tag_manager *, uint32_t tag_num);
void tag_manager_free_manager(tag_manager *);
#endif
