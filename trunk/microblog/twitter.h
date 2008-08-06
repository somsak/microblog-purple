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

#ifdef __cplusplus
extern "C" {
#endif
 
#define TW_HOST "twitter.com"
#define TW_PORT 443
#define TW_AGENT "curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1"
#define TW_AGENT_DESC_URL "http://microblog-purple.googlecode.com/files/mb-0.1.xml"
#define TW_MAXBUFF 51200
#define TW_MAX_RETRY 3
#define TW_INTERVAL 60
#define TW_STATUS_COUNT_MAX 200
#define TW_INIT_TWEET 15
#define TW_STATUS_UPDATE_PATH "/statuses/update.xml"
#define TW_STATUS_TXT_MAX 140
//#define TW_AGENT_SOURCE "libpurplemicroblogplugin"
#define TW_AGENT_SOURCE "mbpidgin"

#define TW_FORMAT_BUFFER 2048
#define TW_FORMAT_NAME_MAX 100

enum _TweetTimeLine {
	TL_FRIENDS = 0,
	TL_USER = 1,
	TL_PUBLIC = 2,
	TL_LAST,
};

enum _TweetProxyDataErrorActions {
	TW_NOACTION = 0,
	TW_RAISE_ERROR = 1,
};

// Hold parameter for statuses request
typedef struct _TwitterTimeLineReq {
	const gchar * path;
	const gchar * name;
	gint timeline_id;
	guint count;
} TwitterTimeLineReq;

typedef struct _TwitterAccount {
	PurpleAccount *account;
	PurpleConnection *gc;
	gchar *login_challenge;
	PurpleConnectionState state;
    GHashTable * conn_hash;
	guint timeline_timer;
	unsigned long long last_msg_id;
	time_t last_msg_time;
	GHashTable * sent_id_hash;
} TwitterAccount;

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
	gint conn_id;
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

typedef struct _TwitterMsg {
	unsigned long long id;
	gchar * avatar_url;
	gchar * from;
	gchar * msg_txt;
	time_t msg_time;
	gint flag;
} TwitterMsg;

#ifdef __cplusplus
}
#endif

#endif
