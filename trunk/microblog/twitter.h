/*
    Copyright 2008, Somsak Sriprayoonsakul <somsaks@gmail.com>
    
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
	
	Unless otherwise stated, All Microblog-purple code is released under the GNU General Public License version 3.
	See COPYING for more information.
*/
/*
	Header for twitter-compliant API
 */

#ifndef __MB_TWITTER__
#define __MB_TWITTER__

#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <time.h>

#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#include <sslconn.h>
#include <prpl.h>

#ifdef __cplusplus
extern "C" {
#endif
 
#define TW_HOST "twitter.com"
#define TW_HTTP_PORT 80
#define TW_HTTPS_PORT 443
#define TW_AGENT "curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1"
#define TW_AGENT_DESC_URL "http://microblog-purple.googlecode.com/files/mb-0.1.xml"
#define TW_MAXBUFF 51200
#define TW_MAX_RETRY 3
#define TW_INTERVAL 60
#define TW_STATUS_COUNT_MAX 200
#define TW_INIT_TWEET 15
#define TW_STATUS_TXT_MAX 140

#ifdef MBADIUM
	#define TW_AGENT_SOURCE "mbadium" 
#else
	#define TW_AGENT_SOURCE "mbpidgin"
#endif

#define TW_FORMAT_BUFFER 2048
#define TW_FORMAT_NAME_MAX 100

enum _TweetTimeLine {
	TL_FRIENDS = 0,
	TL_USER = 1,
	TL_PUBLIC = 2,
	TL_REPLIES = 3,
	TL_LAST,
};

enum _TweetProxyDataErrorActions {
	TW_NOACTION = 0,
	TW_RAISE_ERROR = 1,
};

// Hold parameter for statuses request
typedef struct _TwitterTimeLineReq {
	gchar * path;
	gchar * name;
	int timeline_id;
	int count;
	gboolean use_since_id;
	gchar * sys_msg;
	gchar * screen_name; // for /get command to fetch other user TL
} TwitterTimeLineReq;

extern TwitterTimeLineReq * twitter_new_tlr(const char * path, const char * name, int count, int id, const char * sys_msg);
extern void twitter_free_tlr(TwitterTimeLineReq * tlr);

typedef struct _MbAccount {
	PurpleAccount *account;
	PurpleConnection *gc;
	gchar *login_challenge;
	PurpleConnectionState state;
	GSList * conn_data_list;
	guint timeline_timer;
	unsigned long long last_msg_id;
	time_t last_msg_time;
	GHashTable * sent_id_hash;
	gchar * tag;
	gint tag_pos;
	unsigned long long reply_to_status_id;
} MbAccount;

typedef MbAccount TwitterAccount; //< for the sake of simplicity for now

enum tag_position {
	MB_TAG_NONE = 0,
	MB_TAG_PREFIX = 1,
	MB_TAG_POSTFIX = 2,
};

typedef struct _TwitterBuddy {
	TwitterAccount *ta;
	PurpleBuddy *buddy;
	gint uid;
	gchar *name;
	gchar *status;
	gchar *thumb_url;
} TwitterBuddy;

#define TW_MSGFLAG_SKIP 0x1
#define TW_MSGFLAG_DOTAG 0x2

typedef struct _TwitterMsg {
	unsigned long long id;
	gchar * avatar_url;
	gchar * from;
	gchar * msg_txt;
	time_t msg_time;
	gint flag;
	gchar * is_protected;
} TwitterMsg;

typedef TwitterMsg MbMsg;

extern PurplePluginProtocolInfo twitter_prpl_info;
extern const char * _TweetTimeLineNames[];
extern const char * _TweetTimeLinePaths[];
extern const char * _TweetTimeLineConfigs[];

/*
 * Twitter Configuration
 */
enum _TweetConfig {
	TC_HIDE_SELF = 0,
	TC_PLUGIN,
	TC_PRIVACY,
	TC_MSG_REFRESH_RATE,
	TC_INITIAL_TWEET,
	TC_GLOBAL_RETRY,
	TC_HOST,
	TC_USE_HTTPS,
	TC_STATUS_UPDATE,
	TC_VERIFY_PATH,
	TC_FRIENDS_TIMELINE,
	TC_FRIENDS_USER,
	TC_PUBLIC_TIMELINE,
	TC_PUBLIC_USER,
	TC_USER_TIMELINE,
	TC_USER_USER,
	TC_USER_GROUP,
	TC_REPLIES_TIMELINE,
	TC_REPLIES_USER,
	TC_MAX,
};


typedef struct _TwitterConfig {
	gchar * conf; //< configuration name
	gchar * def_str; //< default value to be used
	gint def_int; 
	gboolean def_bool;
} TwitterConfig;

extern TwitterConfig * _tw_conf;

/* Alias for easier usage of these values */
#define tc_name(name) _tw_conf[name].conf
#define tc_def(name) _tw_conf[name].def_str
#define tc_def_int(name) _tw_conf[name].def_int
#define tc_def_bool(name) _tw_conf[name].def_bool

/* Microblog function */

extern MbAccount * mb_account_new(PurpleAccount * acct);
extern void mb_account_free(MbAccount * ta);

/*
 * Protocol functions
 */
extern void twitter_set_status(PurpleAccount *acct, PurpleStatus *status);
extern GList * twitter_statuses(PurpleAccount *acct);
extern gchar * twitter_status_text(PurpleBuddy *buddy);
extern void twitter_login(PurpleAccount *acct);
extern void twitter_close(PurpleConnection *gc);
extern int twitter_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags);
extern void twitter_buddy_free(PurpleBuddy * buddy);
extern void twitter_get_user_host(MbAccount * ta, char ** user_name, char ** host);
extern void twitter_fetch_new_messages(MbAccount * ta, TwitterTimeLineReq * tlr);
extern gboolean twitter_fetch_all_new_messages(gpointer data);
extern void * twitter_on_replying_message(gchar * proto, unsigned long long msg_id, MbAccount * ma);
extern void twitter_favorite_message(MbAccount * ta, gchar * msg_id);
extern void twitter_retweet_message(MbAccount * ta, gchar * msg_id);

#ifdef __cplusplus
}
#endif

#endif
