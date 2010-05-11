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
	time_t last_update; //< Last cache data update
	time_t last_use; //< Last use of this data
	int avatar_img_id; //< imgstore id
	gchar * avatar_path; //< path name storing this avatar
	gpointer avatar_data; //< pointer to image buffer, will be freed by imgstore
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
