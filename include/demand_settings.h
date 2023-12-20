#ifndef __DEMAND_SETTINGS_H__
#define __DEMAND_SETTINGS_H__

#include "settings.h"

#ifdef KVSSD
#define HASH_KVSSD

/* Storing the key(or fingerprint(hash) of the key) in the mapping entry */
#define FP_SIZE 0
#if (FP_SIZE > 0)
#define STORE_KEY_FP
#endif


/* Support variable-sized value. Grain entries of the mapping table as GRAINED_UNIT */
#define GRAINED_UNIT ( PIECE )
#define VAR_VALUE_MIN ( MINVALUE )
#define VAR_VALUE_MAX ( PAGESIZE )
#define GRAIN_PER_PAGE ( PAGESIZE / GRAINED_UNIT )

/* Max hash collision count to logging ( refer utility.c:hash_collision_logging() ) */
#define MAX_HASH_COLLISION 1024
#endif


#define DEMAND_WARNING 1

#if DEMAND_WARNING
/* Warning options here */
#define WARNING_NOTFOUND
#endif


#define PART_RATIO 0.5

#define WRITE_BACK
#define MAX_WRITE_BUF 256

#define STRICT_CACHING

#define PRINT_GC_STATUS

#endif
