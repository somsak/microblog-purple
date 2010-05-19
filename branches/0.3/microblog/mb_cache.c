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

/**
 * Build a path to the per-user (friend) cache directory
 *
 * @param ma MbAccount for the cache
 * @param user_name friend's user name
 */
static gchar * build_cache_path(const MbAccount * ma, const gchar * user_name)
{
	gchar * host = NULL, * user = NULL;
	gchar * retval;

	// basedir/host/account/username
	// account may already include host name
	mb_get_user_host(ma, &user, &host);
	retval = g_strdup_printf("%s/%s/%s/%s", cache_base_dir, host, user, user_name);
	g_free(user);
	g_free(host);
	return retval;
}

/*
 * Read in cache data from file, or ignore it if cache already exists
 *
 * @param ma MbAccount entry
 * @param user_name user name to read cache
 * @return cache entry, or NULL if none exists
 */
static MbCacheEntry * read_cache(MbAccount * ma, const gchar * user_name)
{
	MbCacheEntry * cache_entry = NULL;
	gchar * cache_path = NULL;
	gchar * cache_avatar_path = NULL;
	struct stat stat_buf;
	time_t now;

	cache_entry = (MbCacheEntry *)g_hash_table_lookup(ma->cache->data, user_name);
	if(!cache_entry) {
		// Check if cache file exist, then read in the cache
		cache_path = build_cache_path(ma, user_name);
		if(stat(cache_path, &stat_buf) == 0) {
			// insert new cache entry
			cache_entry = g_new(MbCacheEntry, 1);
			cache_entry->avatar_img_id = -1;
			cache_entry->user_name = g_strdup(user_name);
			cache_avatar_path = g_strdup_printf("%s/avatar.png", cache_path);
	//		g_hash_table_insert(ma->cache->data, g_strdup(user_name), );

			g_free(cache_avatar_path);
			// And insert this entry
		} else {
			purple_build_dir(cache_path, 0700);
		}
	}
	if(cache_path != NULL) g_free(cache_path);

	return cache_entry;
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
	struct stat stat_buf;
	const char * user_dir = purple_user_dir();

	if(strlen(cache_base_dir) == 0) {
		snprintf(cache_base_dir, PATH_MAX, "%s/mbpurple", user_dir);
	}
	// Check if base dir exists, and create if not
	if(stat(cache_base_dir, &stat_buf) != 0) {
		purple_build_dir(cache_base_dir, 0700);
	}
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

/**
 * Insert data to cache
 *
 * Insert entry into current cache, thus allocate an image id for the image
 * Since the protocol plug-in can not use and GTK/GDK function
 * The task in reading the image data and make it ready will be responsibility
 * of TwitGin.
 *
 * @param ma MbAccount holding the cache
 * @param entry newly allocated MbCacheEntry
 */
void mb_cache_insert(MbAccount * ma, MbCacheEntry * entry)
{

}

// There will be no cache removal for now, since all cache must available almost all the time

const MbCacheEntry * mb_cache_get(MbCache *tg_cache, MbAccount * ma, const gchar * user_name)
{
}
