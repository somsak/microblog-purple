/*
    Copyright 2008-2010, Somsak Sriprayoonsakul <somsaks@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Some part of the code is copied from facebook-pidgin protocols.
    For the facebook-pidgin projects, please see http://code.google.com/p/pidgin-facebookchat/.

    Courtesy to eionrobb at gmail dot com
*/
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
