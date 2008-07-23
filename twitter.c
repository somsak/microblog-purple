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
#include <version.h>

#ifdef _WIN32
#	include <win32dep.h>
#else
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#endif

#define LAST_MESSAGE_MAX 10
#define TWITTER_HOST "twitter.com"
#define TWITTER_PORT 443
#define TWITTER_AGENT "Pidgin 2.4"
#define TW_MAXBUFF 10240
#define TW_MAX_RETRY 3
#define TW_INTERVAL 15
#define TW_TIMELINE_COUNT 5

static void twitterim_process_request(gpointer data);

enum _TweetTimeLine {
	TL_FRIENDS = 0,
	TL_USER = 1,
	TL_PUBLIC = 2,
	TL_LAST,
};

const char * _TweetTimeLineNames[] = {
	"TWFriends",
	"TWUser",
	"TWPublic",
};

const char * _TweetTimeLinePaths[] = {
	"/statuses/friends_timeline.xml",
	"/statuses/user_timeline.xml",
	"/statuses/public_timeline.xml",
};

typedef struct _TwitterAccount {
	PurpleAccount *account;
	PurpleConnection *gc;
	gchar *login_challenge;
	PurpleConnectionState state;
	PurpleSslConnection * conn_data;
	guint timeline_timer;
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
} TwitterProxyData;


typedef struct _TwitterBuddy {
	TwitterAccount *ta;
	PurpleBuddy *buddy;
	gint uid;
	gchar *name;
	gchar *status;
	gchar *thumb_url;
} TwitterBuddy;

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
	g_free(tpd);
}

const char * twitterim_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "twitter";
}

GList * twitterim_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurplePluginAction *act;

	/*
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
	char * content_length, * next, * cur, *end_ptr;
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
	gsize len;
	gint res, call_handler = 0;
	GList * it;
	gchar * tmp_data;
	
	purple_debug_info("twitter", "twitterim_get_result\n");

	//purple_debug_info("twitter", "new cur_result_pos = %d\n", tpd->cur_result_pos);
	tmp_data = g_malloc(TW_MAXBUFF + 1);
	res = purple_ssl_read(ssl, tmp_data, TW_MAXBUFF);
	if(res < 0) {
		// error connecting or reading
		purple_input_remove(ssl->inpa);
		// First, chec if we already have everythings
		if(check_http_data(tpd) == 0) {
			// All is fine, proceed to handler
			call_handler = 1;
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
			
			if(ta->conn_data) {
				purple_ssl_close(ta->conn_data);
				ta->conn_data = NULL;
			}
			tpd->retry += 1;
			if(tpd->retry <= tpd->max_retry) {
				// process request will reconnect and exit
				// FIXME: should we add it to timeout here instead?
				twitterim_process_request(data);
				return;
			} else {
				purple_debug_info("twitter", "error while reading data, res = %d, retry = %d, error = %s\n", res, tpd->retry, strerror(errno));
				purple_connection_error(ta->gc, _(tpd->error_message));
				twitterim_free_tpd(tpd);
			}
		}
	} else if(res > 0) {
		// Need more data
		tmp_data[res] = '\0';
		purple_debug_info("twitter", "got partial result: len = %d\n", res);
		purple_debug_info("twitter", "got partial response = %s\n", tmp_data);
		tpd->result_list = g_list_append(tpd->result_list, tmp_data);
		tpd->result_len += res;
	} else {
		// we have all data
		purple_input_remove(ssl->inpa);
		call_handler = 1;
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
				g_free(tpd->result_data);
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
	gsize len;
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
	guint16 i;
	
	purple_debug_info("twitter", "twitterim_process_request\n");

	ta->conn_data = purple_ssl_connect(ta->account, TWITTER_HOST, TWITTER_PORT, twitterim_post_request, twitterim_connect_error, tpd);
	if(ta->conn_data != NULL) {
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
	const guchar * username_temp, *password_temp;
	guchar *merged_temp, *encoded_temp;
	gsize authen_len;

	//purple_url_encode can't be used more than once on the same line
	username_temp = (const guchar *)purple_account_get_username(ta->account);
	password_temp = (const guchar *)purple_account_get_password(ta->account);
	authen_len = strlen(username_temp) + strlen(password_temp) + 1;
	merged_temp = g_strdup_printf("%s:%s", username_temp, password_temp);
	encoded_temp = purple_base64_encode(merged_temp, authen_len);
	strncpy(output, encoded_temp, len);
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

gint twitterim_fetch_new_messages_handler(TwitterProxyData * tpd, gpointer data)
{
	gchar * path = data;
	
	purple_debug_info("twitter", "fetch_new_messages_handler\n");
	
	purple_debug_info("twitter", "received result from %s\n", data);
	
	purple_debug_info("twitter", "%s\n", tpd->result_data);
	
	twitterim_free_tpd(tpd);
	g_free(data);
}

//
// Check for new message periodically
//
void twitterim_fetch_new_messages(gpointer data)
{
	TwitterAccount * ta = data;
	TwitterProxyData * tpd;
	gint i;
	gsize len;
	
	purple_debug_info("twitter", "fetch_new_messages\n");

	// Look for friend list, then have each populate the data themself.
	for(i = 0; i < TL_LAST; i++) {
		if(!purple_find_buddy(ta->account, _TweetTimeLineNames[i])) {
			purple_debug_info("twitter", "skipping %s\n", _TweetTimeLineNames[i]);
			continue;
		}
		tpd = twitterim_new_proxy_data();
		tpd->ta = ta;
		tpd->error_message = g_strdup("Fetching status error");
		// FIXME: Change this to user's option in maximum message fetching retry
		tpd->max_retry = 0;
		tpd->post_data = g_malloc(TW_MAXBUFF);
		snprintf(tpd->post_data, TW_MAXBUFF, "GET %s?count=%d HTTP/1.0\r\n"
				"Host: " TWITTER_HOST "\r\n"
				"User-Agent: " TWITTER_AGENT "\r\n"
				"Acccept: */*\r\n"
				"Connection: Keep-Alive\r\n"
				"Authorization: Basic ", _TweetTimeLinePaths[i], TW_TIMELINE_COUNT);
		len = strlen(tpd->post_data);
		twitterim_get_authen(ta, tpd->post_data + len, TW_MAXBUFF - len);
		len = strlen(tpd->post_data);
		strncat(tpd->post_data, "\r\n\r\n", TW_MAXBUFF - len);
		tpd->handler = twitterim_fetch_new_messages_handler;
		// List to handle twitter message
		tpd->handler_data = g_strdup(_TweetTimeLinePaths[i]);
		twitterim_process_request(tpd);
	}
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
		purple_connection_set_state(tpd->ta->gc, PURPLE_CONNECTED);
		tpd->ta->state = PURPLE_CONNECTED;
		twitterim_get_buddy_list(tpd->ta);
		//tpd->ta->timeline_timer = purple_timeout_add_seconds(TW_INTERVAL, twitterim_fetch_new_messagees, tpd->ta);
		twitterim_fetch_new_messages(tpd->ta);
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
	gchar *username_temp, *password_temp, *challenge_temp;
	gsize len;
	gint res;
	
	purple_debug_info("twitter", "twitterim_login\n");
	
	// Create account data
	ta = g_new(TwitterAccount, 1);
	ta->account = acct;
	ta->gc = acct->gc;
	ta->state = PURPLE_CONNECTING;
	ta->timeline_timer = -1;
	acct->gc->proto_data = ta;
	
	// Create Proxy entity to transfer data to/from
	tpd = twitterim_new_proxy_data();
	tpd->ta = ta;
	tpd->error_message = g_strdup("Authentication Error");
	// FIXME: Change this to user's option in maximum log-on retry
	tpd->max_retry = TW_MAX_RETRY;

	// Prepare data for this process
	purple_debug_info("twitter", "initialize authentication data\n");
	
	tpd->post_data = g_malloc(TW_MAXBUFF);
	strncpy(tpd->post_data, "GET /account/verify_credentials.xml HTTP/1.0\r\n"
								"Host: " TWITTER_HOST "\r\n"
								"User-Agent: " TWITTER_AGENT "\r\n"
								"Acccept: */*\r\n"
								"Connection: Keep-Alive\r\n"
								"Authorization: Basic ", TW_MAXBUFF);
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

void twitterim_close(PurpleConnection *gc)
{
	TwitterAccount *ta = gc->proto_data;
	gc->proto_data = NULL;
	
	purple_debug_info("twitter", "twitterim_close\n");
	ta->state = PURPLE_DISCONNECTED;
	
	if(ta->conn_data) {
		purple_debug_info("twitter", "before cancel connect\n");
		purple_ssl_close(ta->conn_data);
		purple_debug_info("twitter", "after cancel connect\n");
		ta->conn_data = NULL;
	}
	
	if(ta->timeline_timer != -1) {
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
		
	g_free(ta);
}

static void plugin_init(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;

/*	
	option = purple_account_option_bool_new(_("Hide myself in the Buddy List"), "twitter_hide_self", TRUE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_bool_new(_("Set Facebook status through Pidgin status"), "twitter_set_status_through_pidgin", FALSE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_bool_new(_("Show Facebook notifications as e-mails in Pidgin"), "twitter_get_notifications", TRUE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_int_new(_("Number of retries to send message"), "twitter_max_msg_retry", 2);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
*/
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
//	twitterim_send_im,     /* send_im */
	NULL,
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
	"http://pidgin-facebookchat.googlecode.com/", /* FIXME: homepage */
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
