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
*/

// Since Twitter has no concept of log-in/log-out, buddy list, and etc.
// This plug-in will imitate those by
// 1. Log-in - First activity with twitter
// 2. Buddy List - List timelines
//All actions are done over SSL
//
// Main process of this plug-in is:
//
// Initiator function (called by Purple) -> process_request -> post_request -> get_result -> handle request (action specific)
//
// 

//#define PURPLE_PLUGIN <-- defined in compiler flags
//

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

#include <proxy.h>
#include <sslconn.h>
#include <prpl.h>
#include <debug.h>
#include <connection.h>
#include <request.h>
#include <dnsquery.h>
#include <accountopt.h>
#include <xmlnode.h>
#include <version.h>

#include "util.h"

#ifdef _WIN32
#	include <win32dep.h>
#else
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
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

const char * _TweetTimeLineNames[] = {
	"twitter.com",
	"twuser",
	"twpublic",
};

const char * _TweetTimeLinePaths[] = {
	"/statuses/friends_timeline.xml",
	"/statuses/user_timeline.xml",
	"/statuses/public_timeline.xml",
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

static void twitterim_process_request(gpointer data);
void twitterim_fetch_new_messages(TwitterAccount * ta, TwitterTimeLineReq * tlr);

static TwitterBuddy * twitterim_new_buddy()
{
	TwitterBuddy * buddy = g_new(TwitterBuddy, 1);
	
	buddy->ta = NULL;
	buddy->buddy = NULL;
	buddy->uid = -1;
	buddy->name = NULL;
	buddy->status = NULL;
	buddy->thumb_url = NULL;
	
	return buddy;
}

static TwitterProxyData *  twitterim_new_proxy_data()
{
	TwitterProxyData * tpd;
	
	tpd = g_new(TwitterProxyData, 1);
	tpd->ta = NULL;
	tpd->error_message = NULL;
	tpd->post_data = NULL;
	tpd->retry = 0;
	tpd->max_retry = TW_MAX_RETRY;
	tpd->result_data = NULL;
	tpd->result_list = NULL;
	tpd->result_len = 0;
	tpd->handler = NULL;
	tpd->handler_data = NULL;
	tpd->action_on_error = TW_RAISE_ERROR;
	tpd->conn_data = NULL;
	tpd->conn_id = 0;
	return tpd;
}

static void twitterim_free_tpd(TwitterProxyData * tpd)
{
	GList * it;
	if(tpd->error_message) {
		g_free(tpd->error_message);
	}
	if(tpd->post_data) {
		g_free(tpd->post_data);
	}
	if(tpd->result_list) {
		for(it = g_list_first(tpd->result_list); it; it = g_list_next(it)) {
			g_free(it->data);
		}
		g_list_free(tpd->result_list);
		tpd->result_list = NULL;
	}
	if(tpd->result_data) {
		g_free(tpd->result_data);
		tpd->result_data = NULL;
		tpd->result_len = 0;
	}
	if(tpd->handler_data) {
		g_free(tpd->handler_data);
	}
	if(tpd->conn_data) {
		purple_ssl_close(tpd->conn_data);
		tpd->conn_data = NULL;
	}
	tpd->conn_id = 0;
	g_free(tpd);
}

static TwitterTimeLineReq * twitterim_new_tlr()
{
	TwitterTimeLineReq * tlr = g_new(TwitterTimeLineReq, 1);
	tlr->path = NULL;
	tlr->name = NULL;
	tlr->count = 0;
	tlr->timeline_id = -1;
	return tlr;
}

static void twitterim_free_tlr(TwitterTimeLineReq * tlr)
{
	/*
	if(tlr->path != NULL) g_free(tlr->path);
	if(tlr->name != NULL) g_free(tlr->name);
	*/
}

const char * twitterim_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "twitter";
}

GList * twitterim_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	/*
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Set Facebook status..."), twitterim_set_status_cb);
	m = g_list_append(m, act);
	
	act = purple_plugin_action_new(_("Search for buddies..."), twitterim_search_users);
	m = g_list_append(m, act);
	*/
	
	return m;
}

GList * twitterim_statuses(PurpleAccount *acct)
{
	GList *types = NULL;
	PurpleStatusType *status;
	
	//Online people have a status message and also a date when it was set	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, NULL, _("Online"), TRUE, TRUE, FALSE, "message", _("Message"), purple_value_new(PURPLE_TYPE_STRING), "message_date", _("Message changed"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	//Offline people dont have messages
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, _("Offline"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	return types;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Check for validity of a http data
// @return 0 for success, otherwise -1
static gint check_http_data(TwitterProxyData * tpd)
{
	GList * it;
	char * next, * cur, *end_ptr;
	char * http_data_start = NULL;
	char oldval;
	gchar * tmp_data;
	gsize data_size = 0, received_size;
	
	purple_debug_info("twitter", "check_http_data\n");
	// accumulate data
	tmp_data = g_malloc(tpd->result_len + 1);
	tmp_data[0] = '\0';
	for(it = g_list_first(tpd->result_list); it; it = g_list_next(it)) {
		strcat(tmp_data, it->data);
	}
	purple_debug_info("twitter", "packet to check for = %s\n", tmp_data);
	// Now check for http validity
	http_data_start = strstr(tmp_data, "\r\n\r\n");
	if(http_data_start == NULL) {
		g_free(tmp_data);
		return -1;
	}
	cur = tmp_data;
	received_size = tpd->result_len - (http_data_start - cur) - 4 * sizeof(char);
	purple_debug_info("twitter", "received data size = %d\n", received_size);
	next = strstr(cur, "\r\n");
	if(next == NULL) {
		g_free(tmp_data);
		return -1;
	}
	while( (cur < http_data_start) && next) {
		oldval = (*next);
		(*next) = '\0';
		purple_debug_info("twitter", "http line = %s\n", cur);
		purple_debug_info("twitter", "len = %d\n", next - cur);
		if(strncasecmp(cur, "Content-Length:", 15) == 0) {
			purple_debug_info("twitter", "found content-length: %s\n", &cur[15]);
			data_size = strtoul(&cur[15], &end_ptr, 10);
			(*next) = oldval;
			break;
		}
		(*next) = oldval;
		cur = next + 2 * sizeof(char);
		next = strstr(cur, "\r\n");
	}
	purple_debug_info("twitter", "content-length = %d, received_size = %d\n", data_size, received_size);
	g_free(tmp_data);
	if(data_size) {
		if(data_size == received_size) {
			return 0;
		}
	}
	return -1;
}

//
// Get result (read) from posted request
//
// This function read rfc822 HTTP header, then check for its validity,
// If not (yet) valid, continue in reading queue until everything's read
// If valid but failed (bad gateway, etc), go back to "process_request" step and redo everything until max_retry
// If success, call requets handler
static void twitterim_get_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond)
{
	TwitterProxyData * tpd = data;
	TwitterAccount *ta = tpd->ta;
	gint res, call_handler = 0, cur_error;
	GList * it;
	gchar * tmp_data;
	
	purple_debug_info("twitter", "twitterim_get_result\n");

	//purple_debug_info("twitter", "new cur_result_pos = %d\n", tpd->cur_result_pos);
	tmp_data = g_malloc(TW_MAXBUFF + 1);
	res = purple_ssl_read(ssl, tmp_data, TW_MAXBUFF);
	cur_error = errno;
	if( (res < 0) && (cur_error != EAGAIN) ) {
		// error connecting or reading
		purple_input_remove(ssl->inpa);
		// First, chec if we already have everythings
		if(check_http_data(tpd) == 0) {
			// All is fine, proceed to handler
			call_handler = 1;
			g_free(tmp_data);
		} else {
			// Free all data
			if(tpd->result_data) g_free(tpd->result_data);
			tpd->result_data = NULL;
			if(tpd->result_list) {
				for(it = g_list_first(tpd->result_list); it; it = g_list_next(it)) {
					g_free(it->data);
				}
				g_list_free(tpd->result_list);
				tpd->result_list = NULL;
			}
			tpd->result_len = 0;
			g_free(tmp_data);
			
			if(tpd->conn_data) {
				g_hash_table_remove(ta->conn_hash, &tpd->conn_id);
				purple_ssl_close(tpd->conn_data);
				tpd->conn_data = NULL;
				tpd->conn_id = 0;
				
			}
			tpd->retry += 1;
			if(tpd->retry <= tpd->max_retry) {
				// process request will reconnect and exit
				// FIXME: should we add it to timeout here instead?
				purple_debug_info("twitter", "retrying request\n");
				twitterim_process_request(data);
				return;
			} else {
				purple_debug_info("twitter", "error while reading data, res = %d, retry = %d, error = %s\n", res, tpd->retry, strerror(cur_error));
				if(tpd->action_on_error == TW_RAISE_ERROR) {
					purple_connection_error(ta->gc, _(tpd->error_message));
				}
				twitterim_free_tpd(tpd);
			}
		}
	} else if( (res < 0) && (cur_error == EAGAIN)) {
		purple_debug_info("twitter", "error with EAGAIN\n");
		purple_input_remove(ssl->inpa);
		purple_ssl_input_add(ssl, twitterim_get_result, tpd);
		g_free(tmp_data);
	} else if(res > 0) {
		// Need more data
		purple_input_remove(ssl->inpa);
		tmp_data[res] = '\0';
		purple_debug_info("twitter", "got partial result: len = %d\n", res);
		purple_debug_info("twitter", "got partial response = %s\n", tmp_data);
		tpd->result_list = g_list_append(tpd->result_list, tmp_data);
		tpd->result_len += res;
		purple_ssl_input_add(ssl, twitterim_get_result, tpd);
	} else if(res == 0) {
		// we have all data
		purple_input_remove(ssl->inpa);
		if(tpd->conn_data) {
			g_hash_table_remove(ta->conn_hash, &tpd->conn_id);
			purple_ssl_close(tpd->conn_data);
			tpd->conn_data = NULL;			
			tpd->conn_id = 0;
		}
		call_handler = 1;
		g_free(tmp_data);
	} // global if else for connection state
	// Call handler here
	
	if(call_handler) {
		// reassemble data
		tpd->result_data = g_malloc(tpd->result_len + 1);
		tpd->result_data[0] = '\0';
		for(it = g_list_first(tpd->result_list); it; it = g_list_next(it)) {
			strcat(tpd->result_data, it->data);
			g_free(it->data);
		}
		// Free old data
		g_list_free(tpd->result_list);
		tpd->result_list = NULL;
		purple_debug_info("twitter", "got whole response = %s\n", tpd->result_data);
		if(tpd->handler) {
			gint retval;
			
			retval = tpd->handler(tpd, tpd->handler_data);
			if(retval == 0) {
				// Everything's good. Free data structure and go-on with usual works
				twitterim_free_tpd(tpd);
			} else if(retval == -1) {
				// Something's wrong. Requeue the whole process
				if(tpd->result_data) g_free(tpd->result_data);
				tpd->result_len = 0;
				tpd->result_data = NULL;
				twitterim_process_request(data);
			}
		} // if handler != NULL
	}
}

//
// Post (write) the request
// FIXME: Actually we should add input for write, but SSL function has no "write" condition yet
static void twitterim_post_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond)
{
	TwitterProxyData * tpd = data;
	TwitterAccount *ta = tpd->ta;
	gchar *post_data = tpd->post_data;
	gint res;
	
	purple_debug_info("twitter", "twitterim_post_request\n");
	
	if (!ta || ta->state == PURPLE_DISCONNECTED || !ta->account || ta->account->disconnecting)
	{
		purple_debug_info("twitter", "we're going to be disconnected?\n");
		purple_ssl_close(ssl);
		return;
	}
	
	purple_debug_info("twitter", "posting request %s\n", post_data);
	res = purple_ssl_write(ssl, post_data, strlen(post_data) + 1);
	
	if(res <= 0) {
		// error connecting
		purple_debug_info("twitter", "error while posting request %s\n", post_data);
		purple_connection_error(ta->gc, _(tpd->error_message));
	}
	purple_ssl_input_add(ssl, twitterim_get_result, tpd);
}

void twitterim_connect_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data)
{
	TwitterProxyData * tpa = data;
	TwitterAccount *ta = tpa->ta;

	//ssl error is after 2.3.0
	//purple_connection_ssl_error(fba->gc, errortype);
	purple_connection_error(ta->gc, _("SSL Error"));
}

//
// Start the processing of any twitter actions (REST)
// 
static void twitterim_process_request(gpointer data)
{
	TwitterProxyData * tpd = data;
	TwitterAccount *ta = tpd->ta;
	const char * twitter_host = NULL;
	
	purple_debug_info("twitter", "twitterim_process_request\n");

	twitter_host = purple_account_get_string(ta->account, "twitter_hostname", TW_HOST);
	purple_debug_info("twitter", "connecting to %s on port %hd\n", twitter_host, TW_PORT);
	tpd->conn_data = purple_ssl_connect(ta->account, twitter_host, TW_PORT, twitterim_post_request, twitterim_connect_error, tpd);
	purple_debug_info("twitter", "after connect\n");
	if(tpd->conn_data != NULL) {
		// add this to internal hash table
		tpd->conn_id = tpd->conn_data->fd;
		g_hash_table_insert(ta->conn_hash, &tpd->conn_id, &tpd->conn_data);
		purple_debug_info("twitter", "connect (seems to) success\n");
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Add authentication bits into HTTP header
// Currently, twitter only support basic authen, so we only have basic authen here
//
static void twitterim_get_authen(TwitterAccount * ta, gchar * output, gsize len)
{
	const gchar * username_temp, *password_temp;
	gchar *merged_temp, *encoded_temp;
	gsize authen_len;

	//purple_url_encode can't be used more than once on the same line
	username_temp = (const gchar *)purple_account_get_username(ta->account);
	password_temp = (const gchar *)purple_account_get_password(ta->account);
	authen_len = strlen(username_temp) + strlen(password_temp) + 1;
	merged_temp = g_strdup_printf("%s:%s", username_temp, password_temp);
	encoded_temp = purple_base64_encode((const guchar *)merged_temp, authen_len);
	g_strlcpy(output, encoded_temp, len);
	g_free(merged_temp);
	g_free(encoded_temp);
}

void twitterim_buddy_free(PurpleBuddy * buddy)
{
	TwitterBuddy * tbuddy = buddy->proto_data;
	
	if(tbuddy) {
		if(tbuddy->name) g_free(tbuddy->name);
		if(tbuddy->status) g_free(tbuddy->status);
		if(tbuddy->thumb_url) g_free(tbuddy->thumb_url);
		g_free(tbuddy);
		buddy->proto_data = NULL;
	}
}

// Function to fetch first batch of new message
void twitterim_fetch_first_new_messages(TwitterAccount * ta)
{
	TwitterTimeLineReq * tlr = twitterim_new_tlr();
	
	tlr->path = g_strdup(_TweetTimeLinePaths[TL_FRIENDS]);
	tlr->name = g_strdup(_TweetTimeLineNames[TL_FRIENDS]);
	tlr->timeline_id = TL_FRIENDS;
	tlr->count = purple_account_get_int(ta->account, "twitter_init_tweet", TW_INIT_TWEET);
	twitterim_fetch_new_messages(ta, tlr);
}

// Function to fetch all new messages periodically
gboolean twitterim_fetch_all_new_messages(gpointer data)
{
	TwitterAccount * ta = data;
	TwitterTimeLineReq * tlr = NULL;
	gint i;
	
	for(i = 0; i < TL_LAST; i++) {
		if(!purple_find_buddy(ta->account, _TweetTimeLineNames[i])) {
			purple_debug_info("twitter", "skipping %s\n", tlr->name);
			continue;
		}
		tlr = twitterim_new_tlr();
		tlr->path = _TweetTimeLinePaths[i];
		tlr->name = _TweetTimeLineNames[i];
		tlr->timeline_id = i;
		tlr->count = TW_STATUS_COUNT_MAX;
		twitterim_fetch_new_messages(ta, tlr);
	}
	return TRUE;
}

gchar* twitterim_format_symbols(gchar* src) {
	gchar* tmp_buffer = g_malloc(TW_FORMAT_BUFFER);
	int i = 0;
	int new_i = 0;
	while(src[i] != '\0') {
		if(src[i]=='@' || src[i]=='#') {
			gchar sym = src[i];
			i++; // goto next char
			gchar* name = g_malloc(TW_FORMAT_NAME_MAX);
			// if it's a proper name, extract it
			int j = 0;
			while((src[i]>='a' && src[i] <='z') || (src[i]>='A' && src[i] <='Z') || (src[i]>='0' && src[i] <='9') || src[i]=='_' || src[i]=='-') {
				name[j++] = src[i++];
			}
			name[j]='\0';
			gchar* fmt_name;
			if(sym=='@') { // for @ create a hyper link <a href="http://twitter.com/%s">%s</a>
				fmt_name = g_strdup_printf("@<a href=\"http://twitter.com/%s\">%s</a>",name,name);
			} else {
				fmt_name = g_strdup_printf("#<a href=\"http://search.twitter.com/search?q=%%23%s\">%s</a>",name,name);
			}
			int k=0;
			while(fmt_name[k] != '\0') {
				tmp_buffer[new_i++] = fmt_name[k++];
			}
			g_free(fmt_name);
			g_free(name);
		} else {
			tmp_buffer[new_i++] = src[i++];
		}
	}
	tmp_buffer[new_i]='\0';
	return tmp_buffer;
}

#if 0
static void twitterim_list_sent_id_hash(gpointer key, gpointer value, gpointer user_data)
{
	purple_debug_info("twitter", "key/value = %s/%s\n", key, value);
}
#endif

gint twitterim_fetch_new_messages_handler(TwitterProxyData * tpd, gpointer data)
{
	TwitterAccount * ta = tpd->ta;
	const gchar * username;
	gchar * http_data = NULL;
	gsize http_len = 0;
	TwitterTimeLineReq * tlr = data;
	xmlnode * top = NULL, *id_node, *time_node, *status, * text, * user, * user_name, * image_url;
	gint count = 0;
	gchar * from, * msg_txt, * avatar_url, *xml_str = NULL;
	time_t msg_time_t, last_msg_time_t = 0;
	unsigned long long cur_id;
	GList * msg_list = NULL, *it = NULL;
	TwitterMsg * cur_msg = NULL;
	gboolean hide_myself, skip = FALSE;
	gchar * name_color;
	
	purple_debug_info("twitter", "fetch_new_messages_handler\n");
	
	purple_debug_info("twitter", "received result from %s\n", tlr->path);
	
	purple_debug_info("twitter", "%s\n", tpd->result_data);
	
	username = (const gchar *)purple_account_get_username(ta->account);
	if(strstr(tpd->result_data, "HTTP/1.1 304")) {
		// no new messages
		twitterim_free_tlr(tlr);
		purple_debug_info("twitter", "no new messages\n");
		return 0;
	}
	if(strstr(tpd->result_data, "HTTP/1.1 200") == NULL) {
		twitterim_free_tlr(tlr);
		purple_debug_info("twitter", "something's wrong with the message\n");
		return 0; //< should we return -1 instead?
	}
	http_data = strstr(tpd->result_data, "\r\n\r\n");
	if(http_data == NULL) {
		purple_debug_info("twitter", "can not find new-line separater in rfc822 packet\n");
		twitterim_free_tlr(tlr);
		return 0;
	}
	http_data += 4;
	http_len = http_data - tpd->result_data;
	purple_debug_info("twitter", "http_data = #%s#\n", http_data);
	top = xmlnode_from_str(http_data, -1);
	if(top == NULL) {
		purple_debug_info("twitter", "failed to parse XML data\n");
		twitterim_free_tlr(tlr);
		return 0;
	}
	purple_debug_info("twitter", "successfully parse XML\n");
	status = xmlnode_get_child(top, "status");
	purple_debug_info("twitter", "timezone = %ld\n", timezone);
	
	hide_myself = purple_account_get_bool(ta->account, "twitter_hide_myself", TRUE);
	
	while(status) {
		msg_txt = NULL;
		from = NULL;
		xml_str = NULL;
		
		// ID
		id_node = xmlnode_get_child(status, "id");
		if(id_node) {
			xml_str = xmlnode_get_data_unescaped(id_node);
		}
		// Check for duplicate message
		if(hide_myself) {
			purple_debug_info("twitter", "checking for duplicate message\n");
#if 0
			g_hash_table_foreach(ta->sent_id_hash, twitterim_list_sent_id_hash, NULL);
#endif			
			if(g_hash_table_remove(ta->sent_id_hash, xml_str)  == TRUE) {
				// Dupplicate ID, moved to next value
				purple_debug_info("twitter", "duplicate id = %s\n", xml_str);
				skip = TRUE;
			}
		}
		cur_id = strtoul(xml_str, NULL, 10);
		g_free(xml_str);

		// time
		time_node = xmlnode_get_child(status, "created_at");
		if(time_node) {
			xml_str = xmlnode_get_data_unescaped(time_node);
		}
		purple_debug_info("twitter", "msg time = %s\n", xml_str);
		msg_time_t = mb_mktime(xml_str) - timezone;
		if(last_msg_time_t < msg_time_t) {
			last_msg_time_t = msg_time_t;
		}
		g_free(xml_str);
		
		// message
		text = xmlnode_get_child(status, "text");
		if(text) {
			msg_txt = xmlnode_get_data(text);
		}
		
		// user name
		user = xmlnode_get_child(status, "user");
		if(user) {
			user_name = xmlnode_get_child(user, "screen_name");
			if(user_name) {
				from = xmlnode_get_data_unescaped(user_name);
			}
			image_url = xmlnode_get_child(user, "profile_image_url");
			if(user_name) {
				avatar_url = xmlnode_get_data(image_url);
			}
		}

		if(from && msg_txt) {
			cur_msg = g_new(TwitterMsg, 1);
			
			purple_debug_info("twitter", "from = %s, msg = %s\n", from, msg_txt);
			cur_msg->id = cur_id;
			cur_msg->from = from; //< actually we don't need this for now
			cur_msg->avatar_url = avatar_url; //< actually we don't need this for now
			cur_msg->msg_time = msg_time_t;
			cur_msg->flag = 0;
			if(skip) {
				cur_msg->flag |= TW_MSGFLAG_SKIP;
			}

			if (g_strrstr(msg_txt, username) || !g_str_equal(from, username)) {
				name_color = g_strdup("darkblue");
			} else {
				name_color = g_strdup("darkred");
			}
			if(purple_account_get_bool(ta->account, "twitter_reply_link", FALSE)) {
				cur_msg->msg_txt = g_strdup_printf("<font color=\"%s\"><b><a href=\"tw:reply?to=%s&account=%s\">%s</a>:</b></font> %s", name_color, from, username, from, msg_txt);
			} else {
				cur_msg->msg_txt = g_strdup_printf("<font color=\"%s\"><b>%s:</b></font> %s", name_color, from, msg_txt);
			}
			g_free(name_color);
			g_free(from);
			g_free(avatar_url);
			g_free(msg_txt);
			//serv_got_im(tpd->ta->gc, tlr->name, real_msg, PURPLE_MESSAGE_RECV, msg_time_t);
			
			//purple_debug_info("twitter", "appending message with id = %llu\n", cur_id);
			msg_list = g_list_append(msg_list, cur_msg);
		}
		count++;
		status = xmlnode_get_next_twin(status);
	}
	purple_debug_info("twitter", "we got %d messages\n", count);
	
	// reverse the list and append it
	// only if id > last_msg_id
	msg_list = g_list_reverse(msg_list);
	for(it = g_list_first(msg_list); it; it = g_list_next(it)) {
		cur_msg = it->data;
		if(cur_msg->id > ta->last_msg_id) { //< should we still double check this? It's already being covered by since_id
			gchar * fmt_txt = NULL;
			
			ta->last_msg_id = cur_msg->id;
			if(! cur_msg->flag & TW_MSGFLAG_SKIP)  {
				fmt_txt = twitterim_format_symbols(cur_msg->msg_txt);
				serv_got_im(ta->gc, tlr->name, fmt_txt, PURPLE_MESSAGE_RECV, cur_msg->msg_time);
				g_free(fmt_txt);
			}
		}
		g_free(cur_msg->msg_txt);
	}
	if(ta->last_msg_time < last_msg_time_t) {
		ta->last_msg_time = last_msg_time_t;
	}
	g_list_free(msg_list);
	xmlnode_free(top);
	twitterim_free_tlr(tlr);
	return 0;
}

//
// Check for new message periodically
//
void twitterim_fetch_new_messages(TwitterAccount * ta, TwitterTimeLineReq * tlr)
{
	TwitterProxyData * tpd;
	gsize len;
	gchar since_id[TW_MAXBUFF] = "";
	const char * twitter_host = NULL;
	
	purple_debug_info("twitter", "fetch_new_messages\n");

	// Look for friend list, then have each populate the data themself.


	tpd = twitterim_new_proxy_data();
	tpd->ta = ta;
	tpd->error_message = g_strdup("Fetching status error");
	// FIXME: Change this to user's option in maximum message fetching retry
	tpd->max_retry = 0;
	tpd->action_on_error = TW_NOACTION;
	tpd->post_data = g_malloc(TW_MAXBUFF);
	if(ta->last_msg_id > 0) {
		snprintf(since_id, sizeof(since_id), "&since_id=%lld", ta->last_msg_id);
	}
	twitter_host = purple_account_get_string(ta->account, "twitter_hostname", TW_HOST);
	snprintf(tpd->post_data, TW_MAXBUFF, "GET %s?count=%d%s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"User-Agent: " TW_AGENT "\r\n"
			"Acccept: */*\r\n"
			"Connection: Close\r\n"
			"Authorization: Basic ", tlr->path, tlr->count, since_id, twitter_host);
	len = strlen(tpd->post_data);
	twitterim_get_authen(ta, tpd->post_data + len, TW_MAXBUFF - len);
	len = strlen(tpd->post_data);
	strncat(tpd->post_data, "\r\n\r\n", TW_MAXBUFF - len);
	tpd->handler = twitterim_fetch_new_messages_handler;
	// Request handler for specific request
	tpd->handler_data = tlr;
	twitterim_process_request(tpd);
}

//
// Generate 'fake' buddy list for Twitter
// For now, we only add TwFriends, TwUsers, and TwPublic
void twitterim_get_buddy_list(TwitterAccount * ta)
{
	PurpleBuddy *buddy;
	TwitterBuddy *tbuddy;
	PurpleGroup *twitter_group = NULL;

	purple_debug_info("twitter", "buddy list for account %s\n", ta->account->username);

	//Check if the twitter group already exists
	twitter_group = purple_find_group("Twitter");
	
	// Add timeline as "fake" user
	// Is TL_FRIENDS already exist?
	if ( (buddy = purple_find_buddy(ta->account, _TweetTimeLineNames[TL_FRIENDS])) == NULL)
	{
		purple_debug_info("twitter", "creating new buddy list for %s\n", _TweetTimeLineNames[TL_FRIENDS]);
		buddy = purple_buddy_new(ta->account, _TweetTimeLineNames[TL_FRIENDS], NULL);
		if (twitter_group == NULL)
		{
			purple_debug_info("twitter", "creating new Twitter group\n");
			twitter_group = purple_group_new("Twitter");
			purple_blist_add_group(twitter_group, NULL);
		}
		purple_debug_info("twitter", "setting protocol-specific buddy information to purplebuddy\n");
		if(buddy->proto_data == NULL) {
			tbuddy = twitterim_new_buddy();
			buddy->proto_data = tbuddy;
			tbuddy->buddy = buddy;
			tbuddy->ta = ta;
			tbuddy->uid = TL_FRIENDS;
			tbuddy->name = g_strdup(_TweetTimeLineNames[TL_FRIENDS]);
		}
		purple_blist_add_buddy(buddy, NULL, twitter_group, NULL);
	}
	purple_prpl_got_user_status(ta->account, buddy->name, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE), NULL);
	// We'll deal with public and users timeline later
}

gint twitterim_verify_authen(TwitterProxyData * tpd, gpointer data)
{
	if(strstr(tpd->result_data, "HTTP/1.1 200 OK")) {
		gint interval = purple_account_get_int(tpd->ta->account, "twitter_msg_refresh_rate", TW_INTERVAL);
		
		purple_connection_set_state(tpd->ta->gc, PURPLE_CONNECTED);
		tpd->ta->state = PURPLE_CONNECTED;
		twitterim_get_buddy_list(tpd->ta);
		purple_debug_info("twitter", "refresh interval = %d\n", interval);
		tpd->ta->timeline_timer = purple_timeout_add_seconds(interval, (GSourceFunc)twitterim_fetch_all_new_messages, tpd->ta);
		twitterim_fetch_first_new_messages(tpd->ta);
		return 0;
	} else {
		purple_connection_set_state(tpd->ta->gc, PURPLE_DISCONNECTED);
		tpd->ta->state = PURPLE_DISCONNECTED;
		return -1;
	}
}

void twitterim_login(PurpleAccount *acct)
{
	TwitterAccount *ta = NULL;
	TwitterProxyData * tpd = NULL;
	gsize len;
	const char * twitter_host = NULL;
	
	purple_debug_info("twitter", "twitterim_login\n");
	
	// Create account data
	ta = g_new(TwitterAccount, 1);
	ta->account = acct;
	ta->gc = acct->gc;
	ta->state = PURPLE_CONNECTING;
	ta->timeline_timer = -1;
	ta->last_msg_id = 0;
	ta->last_msg_time = 0;
	ta->conn_hash = g_hash_table_new(g_int_hash, g_int_equal);
	ta->sent_id_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	acct->gc->proto_data = ta;
	
	// Create Proxy entity to transfer data to/from
	tpd = twitterim_new_proxy_data();
	tpd->ta = ta;
	tpd->error_message = g_strdup("Authentication Error");
	// FIXME: Change this to user's option in maximum log-on retry
	tpd->max_retry = purple_account_get_int(acct, "twitter_global_retry", TW_MAX_RETRY);

	// Prepare data for this process
	purple_debug_info("twitter", "initialize authentication data\n");
	
	tpd->post_data = g_malloc(TW_MAXBUFF);
	twitter_host = purple_account_get_string(ta->account, "twitter_hostname", TW_HOST);
	snprintf(tpd->post_data, TW_MAXBUFF, "GET /account/verify_credentials.xml HTTP/1.0\r\n"
								"Host: %s\r\n"
								"User-Agent: " TW_AGENT "\r\n"
								"Acccept: */*\r\n"
								"Connection: Keep-Alive\r\n"
								"Authorization: Basic ", twitter_host);
	len = strlen(tpd->post_data);
	twitterim_get_authen(ta, tpd->post_data + len, TW_MAXBUFF - len);
	len = strlen(tpd->post_data);
	strncat(tpd->post_data, "\r\n\r\n", TW_MAXBUFF - len);
	tpd->handler = twitterim_verify_authen;
	tpd->handler_data = NULL;
	
	purple_debug_info("twitter", "authentication verification data is\n");
	purple_debug_info("twitter", "%s", tpd->post_data);
	// End data preparation
	
	twitterim_process_request(tpd);
}

static void twitterim_close_ssl_connection(gpointer key, gpointer value, gpointer user_data)
{
	PurpleSslConnection * ssl = (PurpleSslConnection *)value;
	
	purple_input_remove(ssl->inpa);
	purple_ssl_close(ssl);
}

void twitterim_close(PurpleConnection *gc)
{
	TwitterAccount *ta = gc->proto_data;
	gc->proto_data = NULL;
	
	purple_debug_info("twitter", "twitterim_close\n");
	ta->state = PURPLE_DISCONNECTED;
	
	if(ta->conn_hash) {
		purple_debug_info("twitter", "closing all active connection\n");
		g_hash_table_foreach(ta->conn_hash, twitterim_close_ssl_connection, NULL);
		purple_debug_info("twitter", "destroying connection hash\n");
		g_hash_table_destroy(ta->conn_hash);
		ta->conn_hash = NULL;
	}
	
	if(ta->sent_id_hash) {
		purple_debug_info("twitter", "destroying sent_id hash\n");
		g_hash_table_destroy(ta->sent_id_hash);
		ta->sent_id_hash = NULL;
	}
	
	if(ta->timeline_timer != -1) {
		purple_debug_info("twitter", "removing timer\n");
		purple_timeout_remove(ta->timeline_timer);
	}
	
	ta->account = NULL;
	ta->gc = NULL;
	
	/*
	purple_timeout_remove(fba->buddy_list_timeout);
	purple_timeout_remove(fba->friend_request_timeout);
	purple_timeout_remove(fba->notifications_timeout);
	*/
	
	//not sure which one of these lines is the right way to logout
	//facebookim_post(fba, "apps.facebook.com", "/ajax/chat/settings.php", "visibility=false", NULL, NULL, FALSE);
	//facebookim_post(fba, "www.facebook.com", "/logout.php", "confirm=1", NULL, NULL, FALSE);
	
	purple_debug_info("twitter", "free up memory used for twitter account structure\n");
	g_free(ta);
}

gint twitterim_send_im_handler(TwitterProxyData * tpd, gpointer data)
{
	TwitterAccount * ta = tpd->ta;
	gchar * http_data = NULL, *id_str = NULL;
	gsize http_len = 0;
	xmlnode * top, *id_node;
	
	purple_debug_info("twitter", "send_im_handler\n");
	
	if(strstr(tpd->result_data, "HTTP/1.1 200 OK") == NULL) {
		purple_debug_info("twitter", "http error\n");
		purple_debug_info("twitter", "http data = #%s#\n", tpd->result_data);
		return -1;
	}

	// Are we going to check this?
	if(!purple_account_get_bool(ta->account, "twitter_hide_myself", TRUE)) {
		return 0;
	}
	
	// Check for returned ID
	http_data = strstr(tpd->result_data, "\r\n\r\n");
	if(http_data == NULL) {
		purple_debug_info("twitter", "can not find new-line separater in rfc822 packet for send_im_handler\n");
		return -1;
	}
	http_data += 4;
	http_len = http_data - tpd->result_data;
	purple_debug_info("twitter", "http_data = #%s#\n", http_data);
	
	// parse response XML
	top = xmlnode_from_str(http_data, -1);
	if(top == NULL) {
		purple_debug_info("twitter", "failed to parse XML data\n");
		return -1;
	}
	purple_debug_info("twitter", "successfully parse XML\n");

	// ID
	id_node = xmlnode_get_child(top, "id");
	if(id_node) {
		id_str = xmlnode_get_data_unescaped(id_node);
	}
	
	// save it to account
	g_hash_table_insert(ta->sent_id_hash, id_str, id_str);
	
	//hash_table supposed to free this for use
	//g_free(id_str);
	xmlnode_free(top);
	return 0;
}

int twitterim_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	TwitterProxyData *tpd = NULL;
	TwitterAccount * ta = gc->proto_data;
	gchar * tmp_msg_txt = NULL;
	gsize len, msg_len;
	const char * twitter_host = NULL;

	//convert html to plaintext, removing trailing spaces
	purple_debug_info("twitter", "send_im\n");

	tmp_msg_txt = g_strdup(purple_url_encode(g_strchomp(purple_markup_strip_html(message))));
	msg_len = strlen(message);
	purple_debug_info("twitter", "sending message %s\n", tmp_msg_txt);

	tpd = twitterim_new_proxy_data();
	tpd->ta = ta;
	tpd->error_message = g_strdup("Sending status error");
	// FIXME: Change this to user's option in maximum message fetching retry
	tpd->max_retry = 0;
	tpd->action_on_error = TW_NOACTION;
	tpd->post_data = g_malloc(TW_MAXBUFF);
	twitter_host = purple_account_get_string(ta->account, "twitter_hostname", TW_HOST);
	snprintf(tpd->post_data, TW_MAXBUFF,  "POST " TW_STATUS_UPDATE_PATH " HTTP/1.1\r\n"
			"Host: %s\r\n"
			"User-Agent: " TW_AGENT "\r\n"
			"Acccept: */*\r\n"
			"X-Twitter-Client: " TW_AGENT_SOURCE "\r\n"
			"X-Twitter-Client-Version: 0.1\r\n"
			"X-Twitter-Client-Url: " TW_AGENT_DESC_URL "\r\n"
			"Connection: Close\r\n"
			"Pragma: no-cache\r\n"
			"Content-Length: %d\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Authorization: Basic ", twitter_host, strlen(tmp_msg_txt) + strlen(TW_AGENT_SOURCE) + 15);
	
	len = strlen(tpd->post_data);
	twitterim_get_authen(ta, tpd->post_data + len, TW_MAXBUFF - len);
	len = strlen(tpd->post_data);
	snprintf(&tpd->post_data[len], TW_MAXBUFF - len, "\r\n\r\nstatus=%s&source=" TW_AGENT_SOURCE, tmp_msg_txt);
	tpd->handler = twitterim_send_im_handler;
	// Request handler for specific request
	tpd->handler_data = NULL;
	purple_debug_info("twitter", "sending http data = %s\n", tpd->post_data);
	twitterim_process_request(tpd);
	g_free(tmp_msg_txt);

	return msg_len;
}


static void plugin_init(PurplePlugin *plugin)
{

	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;

	option = purple_account_option_bool_new(_("Hide myself in conversation"), "twitter_hide_myself", TRUE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_bool_new(_("Enable reply link"), "twitter_reply_link", FALSE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_int_new(_("Message refresh rate (seconds)"), "twitter_msg_refresh_rate", 60);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_int_new(_("Number of initial tweets"), "twitter_init_tweet", 15);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

	option = purple_account_option_int_new(_("Maximum number of retry"), "twitter_global_retry", 3);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

	option = purple_account_option_string_new(_("Twitter hostname"), "twitter_hostname", "twitter.com");
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	

}

gboolean plugin_load(PurplePlugin *plugin)
{
	purple_debug_info("twitterim", "plugin_load\n");
	return TRUE;
}

gboolean plugin_unload(PurplePlugin *plugin)
{
	purple_debug_info("twitterim", "plugin_unload\n");
	return TRUE;
}

gchar * twitterim_status_text(PurpleBuddy *buddy)
{
	TwitterBuddy * tbuddy = buddy->proto_data;
	
	if (tbuddy && tbuddy->status && strlen(tbuddy->status))
		return g_strdup(tbuddy->status);
	
	return NULL;
}

// There's no concept of status in TwitterIM for now
void twitterim_set_status(PurpleAccount *acct, PurpleStatus *status) {
  const char *msg = purple_status_get_attr_string(status, "message");
  purple_debug_info("twitterim", "setting %s's status to %s: %s\n",
                    acct->username, purple_status_get_name(status), msg);

}

static PurplePluginProtocolInfo prpl_info = {
	/* options */
	OPT_PROTO_UNIQUE_CHATNAME,
	NULL,                   /* user_splits */
	NULL,                   /* protocol_options */
	//NO_BUDDY_ICONS          /* icon_spec */
	{   /* icon_spec, a PurpleBuddyIconSpec */
		"png,jpg,gif",                   /* format */
		0,                               /* min_width */
		0,                               /* min_height */
		50,                             /* max_width */
		50,                             /* max_height */
		10000,                           /* max_filesize */
		PURPLE_ICON_SCALE_DISPLAY,       /* scale_rules */
	},
	twitterim_list_icon,   /* list_icon */
	NULL,                   /* list_emblems */
	twitterim_status_text, /* status_text */
//	twitterim_tooltip_text,/* tooltip_text */
	NULL,
	twitterim_statuses,    /* status_types */
	NULL,                   /* blist_node_menu */
	NULL,                   /* chat_info */
	NULL,                   /* chat_info_defaults */
	twitterim_login,       /* login */
	twitterim_close,       /* close */
	twitterim_send_im,     /* send_im */
	NULL,                   /* set_info */
//	twitterim_send_typing, /* send_typing */
	NULL,
//	twitterim_get_info,    /* get_info */
	NULL,
	twitterim_set_status,/* set_status */
	NULL,                   /* set_idle */
	NULL,                   /* change_passwd */
//	twitterim_add_buddy,   /* add_buddy */
	NULL,
	NULL,                   /* add_buddies */
//	twitterim_remove_buddy,/* remove_buddy */
	NULL,
	NULL,                   /* remove_buddies */
	NULL,                   /* add_permit */
	NULL,                   /* add_deny */
	NULL,                   /* rem_permit */
	NULL,                   /* rem_deny */
	NULL,                   /* set_permit_deny */
	NULL,                   /* join_chat */
	NULL,                   /* reject chat invite */
	NULL,                   /* get_chat_name */
	NULL,                   /* chat_invite */
	NULL,                   /* chat_leave */
	NULL,                   /* chat_whisper */
	NULL,                   /* chat_send */
	NULL,                   /* keepalive */
	NULL,                   /* register_user */
	NULL,                   /* get_cb_info */
	NULL,                   /* get_cb_away */
	NULL,                   /* alias_buddy */
	NULL,                   /* group_buddy */
	NULL,                   /* rename_group */
	twitterim_buddy_free,  /* buddy_free */
	NULL,                   /* convo_closed */
	purple_normalize_nocase,/* normalize */
	NULL,                   /* set_buddy_icon */
	NULL,                   /* remove_group */
	NULL,                   /* get_cb_real_name */
	NULL,                   /* set_chat_topic */
	NULL,                   /* find_blist_chat */
	NULL,                   /* roomlist_get_list */
	NULL,                   /* roomlist_cancel */
	NULL,                   /* roomlist_expand_category */
	NULL,                   /* can_receive_file */
	NULL,                   /* send_file */
	NULL,                   /* new_xfer */
	NULL,                   /* offline_message */
	NULL,                   /* whiteboard_prpl_ops */
	NULL,                   /* send_raw */
	NULL,                   /* roomlist_room_serialize */
	NULL,                   /* unregister_user */
	NULL,                   /* send_attention */
	NULL,                   /* attention_types */
	(gpointer)sizeof(PurplePluginProtocolInfo) /* struct_size */
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL, /* type */
	NULL, /* ui_requirement */
	0, /* flags */
	NULL, /* dependencies */
	PURPLE_PRIORITY_DEFAULT, /* priority */
	"prpl-somsaks-twitter", /* id */
	"Twitter", /* name */
	"0.1", /* version */
	"Twitter data feeder", /* summary */
	"Twitter data feeder", /* description */
	"Somsak Sriprayoonsakul <somsaks@gmail.com>", /* author */
	"http://microblog-purple.googlecode.com/", /* FIXME: homepage */
	plugin_load, /* load */
	plugin_unload, /* unload */
	NULL, /* destroy */
	NULL, /* ui_info */
	&prpl_info, /* extra_info */
	NULL, /* prefs_info */
	twitterim_actions, /* actions */
	NULL, /* padding */
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(twitterim, plugin_init, info);
