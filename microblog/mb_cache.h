/*
 * Cache user information locally and make it available for other Twitgin part
 *
 *  Created on: Apr 18, 2010
 *      Author: somsak
 */

#ifndef __MBPURPLE_CACHE__
#define __MBPURPLE_CACHE__

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <sys/time.h>

#include <glib.h>

typedef struct {
	gchar * user_name; //< owner of this cache entry
	time_t last_update;
	time_t last_use;
	int avatar_img_id;
	gchar * avatar_path;
} MbCacheEntry;

typedef struct {
	// Mapping between cache entry and data inside
	// A mapping between MbAccount and TGCacheGroup
	GHashTable * data;
	gboolean fetcher_is_run;
	int avatar_fetch_max;
} MbCache;


#ifdef __cplusplus
}
#endif

#endif //< MBPURPLE_CACHE
