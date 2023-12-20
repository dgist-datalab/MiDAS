#ifndef __FUNCTION_TEST_H__
#define __FUNCTION_TEST_H__
#include "level.h"
#include "run.h"
#include "lsmtree.h"

void LSM_traversal(lsmtree *lsm);
bool LSM_find_lba(lsmtree *lsm, uint32_t lba);
bool LSM_level_find_lba(level *lev, uint32_t lba);
void level_consistency_check(level *lev, bool version_check);

#endif
