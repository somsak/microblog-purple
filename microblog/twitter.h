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
	gint timeline_id;
	guint count;
	gboolean use_since_id;
	gchar * sys_msg;
} TwitterTimeLineReq;

extern TwitterTimeLineReq * twitter_new_tlr(const char * path, const char * name, int count, unsigned int id, const char * sys_msg);
extern void twitter_free_tlr(TwitterTimeLineReq * tlr);

typedef struct _TwitterAccount {
	PurpleAccount *account;
	PurpleConnection *gc;
	gchar *login_challenge;
	PurpleConnectionState state;
	GHashTable * conn_hash;
	GHashTable * ssl_conn_hash;
	guint timeline_timer;
	unsigned long long last_msg_id;
	time_t last_msg_time;
	GHashTable * sent_id_hash;
	gchar * tag;
	gint tag_pos;
	unsigned long long reply_to_status_id;
} TwitterAccount;

typedef TwitterAccount MbAccount; //< for the sake of simplicity for now

enum tag_position {
	MB_TAG_NONE = 0,
	MB_TAG_PREFIX = 1,
	MB_TAG_POSTFIX = 2,
};

struct _TwitterProxyData;

// if handler return
// 0 - Everything's ok
// -1 - Requeue the whole process again
typedef gint (*TwitterHandlerFunc)(struct _TwitterProxyData * , gpointer );

typedef struct _TwitterProxyData {
	TwitterAccount * ta;
	gchar * error_message;
	gchar * post_data;
	gint retry;
	gint max_retry;
	gchar * result_data;
	GList * result_list;
	guint result_len;
	TwitterHandlerFunc handler;
	gpointer handler_data;
	gint action_on_error;
	PurpleSslConnection * conn_data;
} TwitterProxyData;

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
 * Utility functions
 */
extern void mbpurple_account_set_ull(PurpleAccount * account, const char * name, unsigned long long value);
extern unsigned long long mbpurple_account_get_ull(PurpleAccount * account, const char * name, unsigned long long default_value);

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
extern char * twitter_reformat_msg(MbAccount * ta, const TwitterMsg * msg, const char * conv_name, gboolean reply_link);
extern void twitter_get_user_host(MbAccount * ta, char ** user_name, char ** host);
extern void twitter_fetch_new_messages(MbAccount * ta, TwitterTimeLineReq * tlr);
extern gboolean twitter_fetch_all_new_messages(gpointer data);
extern void * twitter_on_replying_message(gchar * proto, unsigned long long msg_id, MbAccount * ma);
extern void twitter_favorite_message(MbAccount * ta, gchar * msg_id);

#ifdef __cplusplus
}
#endif

#endif
