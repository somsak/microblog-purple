/*
 * cache.c
 *
 *  Created on: Apr 18, 2010
 *      Author: somsak
 */

#include <util.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "twitter.h"

static char cache_base_dir[PATH_MAX] = "";

static void mb_cache_entry_free(gpointer data)
{
	MbCacheEntry * cache_entry = (MbCacheEntry *)data;

	// unref the image
	// XXX: Implement later!

	// free all path reference and user_name
	g_free(cache_entry->user_name);
	g_free(cache_entry->avatar_path);
}

/*
 * Read in cache data from file, or ignore it if cache already exists
 *
 * @param ma MbAccount entry
 * @param user_name user name to read cache
 * @return cache entry, or NULL if none exists
 */
static MbCacheEntry * mb_cache_read_cache(MbAccount * ma, const gchar * user_name)
{
	MbCacheEntry * cache_entry = NULL;

	cache_entry = (MbCacheEntry *)g_hash_table_lookup(ma->cache->data, user_name);
	if(!cache_entry) {
		// Check if cache file exist, then read in the cache

		// insert new cache entry
		cache_entry = g_new(MbCacheEntry, 1);
		cache_entry->avatar_img_id = -1;
		cache_entry->user_name = g_strdup(user_name);
//		g_hash_table_insert(ma->cache->data, g_strdup(user_name), );

		// And insert this entry
	}
}

/**
 * Return cache base dir
 */
const char * mb_cache_base_dir(void)
{
	return cache_base_dir;
}

/**
 * Initialize cache system
 */
void mb_cache_init(void)
{
	// Create base dir for all images
	const char * user_dir = purple_user_dir();

	if(strlen(cache_base_dir) == 0) {
		snprintf(cache_base_dir, PATH_MAX, "%s/mbpurple", user_dir);
	}
	// Check if base dir exists, and create if not
}

/**
 *  Create new cache
 *
 */
MbCache * mb_cache_new(void)
{
	MbCache * retval = g_new(MbCache, 1);

	retval->data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, mb_cache_entry_free);

	retval->fetcher_is_run = FALSE;
	retval->avatar_fetch_max = 20; //< Fetch at most 20 avatars at a time

	// Initialize cache, if needed

	return retval;
}

// Destroy cache
void mb_cache_free(MbCache * mb_cache)
{
	// MBAccount is something that we can not touch
	g_hash_table_destroy(mb_cache->data);

	g_free(mb_cache);
}

// Insert data to the cache
void mb_cache_insert(MbAccount * ma, MbMsg * msg)
{
	// Currently we only cache Avatar
	if (msg->avatar_url) {
		// Read in cache content
//		mb_cache_read_cache(ma->cache, ma, msg->from);
		// fetch URL, with limit number of simultaneous fetch
		// Check if the avatar_url is newer than cache, if so then fetch it
	}
}

// There will be no cache removal for now, since all cache must available almost all the time

const MbCacheEntry * mb_cache_get(MbCache *tg_cache, MbAccount * ma, const gchar * user_name)
{
}
