/*
 * mb_cache_util.h
 *
 *  Created on: May 8, 2010
 *      Author: somsak
 */

#ifndef MB_CACHE_UTIL_H_
#define MB_CACHE_UTIL_H_

#include "twitter.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize cache system
// Currently, this just create directories, so no need for finalize
extern void mb_cache_init(void);

// Get base dir to cache directory
extern const char * mb_cache_base_dir(void);

// Create new cache
extern MbCache * mb_cache_new(void);

// Destroy cache
extern void mb_cache_free(MbCache * mb_cache);

// Insert data to the cache
extern void mb_cache_insert(MbAccount * ma, MbCacheEntry * entry);

// There will be no cache removal for now, since all cache must available almost all the time
extern const MbCacheEntry * mb_cache_get(const MbAccount * ma, const gchar * user_name);

#ifdef __cplusplus
}
#endif

#endif /* MB_CACHE_UTIL_H_ */
