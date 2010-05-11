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

#include <glib.h>
#include <ctype.h>
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
#include <signals.h>

#include "mb_net.h"
#include "mb_util.h"
#include "mb_cache_util.h"

#ifdef _WIN32
#	include <win32dep.h>
#else
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#endif

#include "twitter.h"

#define DBGID "twitter"
#define TW_ACCT_LAST_MSG_ID "twitter_last_msg_id"
#define TW_ACCT_SENT_MSG_IDS "twitter_sent_msg_ids"

//static const char twitter_host[] = "twitter.com";
static gint twitter_port = 443;
static const char twitter_fixed_headers[] = "User-Agent:" TW_AGENT "\r\n" \
"Accept: */*\r\n" \
"X-Twitter-Client: " TW_AGENT_SOURCE "\r\n" \
"X-Twitter-Client-Version: 0.1\r\n" \
"X-Twitter-Client-Url: " TW_AGENT_DESC_URL "\r\n" \
"Connection: Close\r\n" \
"Pragma: no-cache\r\n";

PurplePlugin * twitgin_plugin = NULL;

/**
 * Convenient function to initialize new connection and set necessary value
 */
static MbConnData * twitter_init_connection(MbAccount * ma, const gchar * host, gint port, gint type,
		const char * path, const gchar * user_name, const gchar * password,
		MbHandlerFunc handler, gboolean use_https)
{
	MbConnData * conn_data = NULL;

	conn_data = mb_conn_data_new(ma, host, port, handler, use_https);
	mb_conn_data_set_retry(conn_data, 0);

	conn_data->request;
	conn_data->request->type = type;
	conn_data->request->port = port;

	mb_http_data_set_host(conn_data->request, host);
	mb_http_data_set_path(conn_data->request, path);
	// XXX: Use global here -> twitter_fixed_headers
	mb_http_data_set_fixed_headers(conn_data->request, twitter_fixed_headers);
	mb_http_data_set_header(conn_data->request, "Host", host);
	mb_http_data_set_basicauth(conn_data->request, user_name, password);

	return conn_data;
}

static TwitterBuddy * twitter_new_buddy()
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

TwitterTimeLineReq * twitter_new_tlr(const char * path, const char * name, int id, int count, const char * sys_msg)
{
	TwitterTimeLineReq * tlr = g_new(TwitterTimeLineReq, 1);
	tlr->path = g_strdup(path);
	tlr->name = g_strdup(name);
	tlr->count = count;
	tlr->timeline_id = id;
	tlr->use_since_id = TRUE;
	tlr->screen_name = NULL;
	if(sys_msg) {
		tlr->sys_msg = g_strdup(sys_msg);
	} else {
		tlr->sys_msg = NULL;
	}
	return tlr;
}

void twitter_free_tlr(TwitterTimeLineReq * tlr)
{
	if(tlr->path != NULL) g_free(tlr->path);
	if(tlr->name != NULL) g_free(tlr->name);
	if(tlr->sys_msg != NULL) g_free(tlr->sys_msg);
	g_free(tlr);
}

GList * twitter_statuses(PurpleAccount *acct)
{
	GList *types = NULL;
	PurpleStatusType *status;
	
	
	//Online people have a status message and also a date when it was set	
	//status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, NULL, _("Online"), TRUE, TRUE, FALSE, "message", _("Message"), purple_value_new(PURPLE_TYPE_STRING), "message_date", _("Message changed"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, _("Online"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_UNAVAILABLE, NULL, _("Unavailable"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	//Offline people dont have messages
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, _("Offline"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	return types;
}

void twitter_buddy_free(PurpleBuddy * buddy)
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

// Privacy mode skip fetching messages during unavailable state
gboolean twitter_skip_fetching_messages(PurpleAccount * acct) 
{
	gboolean privacy_mode = purple_account_get_bool(acct, tc_name(TC_PRIVACY), tc_def_bool(TC_PRIVACY));
	gboolean available = purple_status_is_available(purple_account_get_active_status(acct));

	if(privacy_mode && !available) {
		purple_debug_info(DBGID, "Unavailable, skipping fetching due privacy mode\n");
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Function to fetch first batch of new message
void twitter_fetch_first_new_messages(TwitterAccount * ta)
{
	if(twitter_skip_fetching_messages(ta->account)) {
		return;
	}

	TwitterTimeLineReq * tlr;
	const gchar * tl_path;
	int count;
	
	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	tl_path = purple_account_get_string(ta->account, tc_name(TC_FRIENDS_TIMELINE), tc_def(TC_FRIENDS_TIMELINE));
	count = purple_account_get_int(ta->account, tc_name(TC_INITIAL_TWEET), tc_def_int(TC_INITIAL_TWEET));
	purple_debug_info(DBGID, "count = %d\n", count);
	tlr = twitter_new_tlr(tl_path, tc_def(TC_FRIENDS_USER), TL_FRIENDS, count, NULL);
	twitter_fetch_new_messages(ta, tlr);
}

// Function to fetch all new messages periodically
gboolean twitter_fetch_all_new_messages(gpointer data)
{
	TwitterAccount * ta = data;
	TwitterTimeLineReq * tlr = NULL;
	gint i;
	const gchar * tl_path;

	if(twitter_skip_fetching_messages(ta->account)) {
		return TRUE;
	}
	
	for(i = TC_FRIENDS_TIMELINE; i <= TC_USER_TIMELINE; i+=2) {
		//FIXME: i + 1 is not a very good strategy here
		if(!purple_find_buddy(ta->account, tc_def(i + 1))) {
			purple_debug_info(DBGID, "skipping %s\n", tlr->name);
			continue;
		}
		tl_path = purple_account_get_string(ta->account, tc_name(i), tc_def(i));
		tlr = twitter_new_tlr(tl_path, tc_def(i + 1), i, TW_STATUS_COUNT_MAX, NULL);
		purple_debug_info(DBGID, "fetching updates from %s to %s\n", tlr->path, tlr->name);
		twitter_fetch_new_messages(ta, tlr);
	}
	return TRUE;
}

#if 0
static void twitter_list_sent_id_hash(gpointer key, gpointer value, gpointer user_data)
{
	purple_debug_info(DBGID, "key/value = %s/%s\n", key, value);
}
#endif

//
// Decode error message from twitter
//
char * twitter_decode_error(const char * data)
{
	xmlnode * top = NULL, * error = NULL;
	gchar * error_str = NULL;

	top = xmlnode_from_str(data, -1);
	if(top == NULL) {
		purple_debug_info(DBGID, "failed to parse XML data from error response\n");
		return NULL;
	}
	error = xmlnode_get_child(top, "error");
	if(error) {
		error_str = xmlnode_get_data_unescaped(error);
	}
	xmlnode_free(top);

	return error_str;
}

//
// Decode timeline message
//
GList * twitter_decode_messages(const char * data, time_t * last_msg_time)
{
	GList * retval = NULL;
	xmlnode * top = NULL, *id_node, *time_node, *status, * text, * user, * user_name, * image_url, * user_is_protected;
	gchar * from, * msg_txt, * avatar_url = NULL, *xml_str = NULL, * is_protected = NULL;
	TwitterMsg * cur_msg = NULL;
	unsigned long long cur_id;
	time_t msg_time_t;

	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	top = xmlnode_from_str(data, -1);
	if(top == NULL) {
		purple_debug_info(DBGID, "failed to parse XML data\n");
		return NULL;
	}

	purple_debug_info(DBGID, "successfully parse XML\n");
	status = xmlnode_get_child(top, "status");
	purple_debug_info(DBGID, "timezone = %ld\n", timezone);
	
	while(status) {
		msg_txt = NULL;
		from = NULL;
		xml_str = NULL;
		//skip = FALSE;
		
		// ID
		id_node = xmlnode_get_child(status, "id");
		if(id_node) {
			xml_str = xmlnode_get_data_unescaped(id_node);
		}
		cur_id = strtoull(xml_str, NULL, 10);
		g_free(xml_str);

		// time
		time_node = xmlnode_get_child(status, "created_at");
		if(time_node) {
			xml_str = xmlnode_get_data_unescaped(time_node);
		}
		purple_debug_info(DBGID, "msg time = %s\n", xml_str);
		msg_time_t = mb_mktime(xml_str);
		if( (*last_msg_time) < msg_time_t) {
			(*last_msg_time) = msg_time_t;
		}
		g_free(xml_str);
		
		// message
		text = xmlnode_get_child(status, "text");
		if(text) {
			msg_txt = xmlnode_get_data_unescaped(text);
		}

		// user name
		user = xmlnode_get_child(status, "user");
		if(user) {
			user_name = xmlnode_get_child(user, "screen_name");
			if(user_name) {
				from = xmlnode_get_data(user_name);
			}
			image_url = xmlnode_get_child(user, "profile_image_url");
			if(image_url) {
				avatar_url = xmlnode_get_data(image_url);
			}
			user_is_protected = xmlnode_get_child(user, "protected");
			if(user_is_protected) {
				is_protected = xmlnode_get_data(user_is_protected);
			}
		}

		if(from && msg_txt) {
			cur_msg = g_new(TwitterMsg, 1);
			
			purple_debug_info(DBGID, "from = %s, msg = %s\n", from, msg_txt);
			cur_msg->id = cur_id;
			cur_msg->from = from; //< actually we don't need this for now
			cur_msg->avatar_url = avatar_url; //< actually we don't need this for now
			cur_msg->msg_time = msg_time_t;
			if(is_protected && (strcmp(is_protected, "false") == 0) ) {
				cur_msg->is_protected = FALSE;
				g_free(is_protected);
			} else {
				cur_msg->is_protected = TRUE;
			}
			cur_msg->flag = 0;
			/*
			if(skip) {
				cur_msg->flag |= TW_MSGFLAG_SKIP;
			}
			*/
			cur_msg->msg_txt = msg_txt;
			
			//purple_debug_info(DBGID, "appending message with id = %llu\n", cur_id);
			retval = g_list_append(retval, cur_msg);
		}
		status = xmlnode_get_next_twin(status);
	}
	xmlnode_free(top);

	return retval;
}

gint twitter_fetch_new_messages_handler(MbConnData * conn_data, gpointer data)
{
	MbAccount * ma = conn_data->ma;
	const gchar * username;
	MbHttpData * response = conn_data->response;
	TwitterTimeLineReq * tlr = data;
	time_t last_msg_time_t = 0;
	GList * msg_list = NULL, *it = NULL;
	TwitterMsg * cur_msg = NULL;
	gboolean hide_myself;
	gchar * id_str = NULL, * msg_txt = NULL;
	
	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	purple_debug_info(DBGID, "received result from %s\n", tlr->path);
	
	username = (const gchar *)purple_account_get_username(ma->account);
	
	if(response->status == HTTP_MOVED_TEMPORARILY) {
		// no new messages
		twitter_free_tlr(tlr);
		purple_debug_info(DBGID, "no new messages\n");
		return 0;
	}
	if(response->status != HTTP_OK) {
		twitter_free_tlr(tlr);
		if(response->status == HTTP_BAD_REQUEST) {
			// rate limit exceed?
			if(response->content_len > 0) {
				gchar * error_str = NULL;

				error_str = twitter_decode_error(response->content->str);
				if(ma->gc != NULL) {
					purple_connection_set_state(ma->gc, PURPLE_DISCONNECTED);
					ma->state = PURPLE_DISCONNECTED;
					purple_connection_error(ma->gc, error_str);
				}
				g_free(error_str);
			}
			return 0; //< return 0 so the request is not being issued again
		} else {
			purple_debug_info(DBGID, "something's wrong with the message?, status = %d\n", response->status);
			return 0; //< should we return -1 instead?
		}
	}
	if(response->content_len == 0) {
		purple_debug_info(DBGID, "no data to parse\n");
		twitter_free_tlr(tlr);
		return 0;
	}
	purple_debug_info(DBGID, "http_data = #%s#\n", response->content->str);
	msg_list = twitter_decode_messages(response->content->str, &last_msg_time_t);
	if(msg_list == NULL) {
		twitter_free_tlr(tlr);
		return 0;
	}
	
	// reverse the list and append it
	// only if id > last_msg_id
	hide_myself = purple_account_get_bool(ma->account, tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF));
	msg_list = g_list_reverse(msg_list);
	for(it = g_list_first(msg_list); it; it = g_list_next(it)) {

		cur_msg = it->data;
		purple_debug_info(DBGID, "**twitpocalypse** cur_msg->id = %llu, ma->last_msg_id = %llu\n", cur_msg->id, ma->last_msg_id);
		if(cur_msg->id > ma->last_msg_id) {
			ma->last_msg_id = cur_msg->id;
			mb_account_set_ull(ma->account, TW_ACCT_LAST_MSG_ID, ma->last_msg_id);
		}
		id_str = g_strdup_printf("%llu", cur_msg->id);
		if(!(hide_myself && (g_hash_table_remove(ma->sent_id_hash, id_str) == TRUE))) {
			msg_txt = g_strdup_printf("%s: %s", cur_msg->from, cur_msg->msg_txt);
			// we still call serv_got_im here, so purple take the message to the log
			serv_got_im(ma->gc, tlr->name, msg_txt, PURPLE_MESSAGE_RECV, cur_msg->msg_time);
			// by handling diaplying-im-msg, the message shouldn't be displayed anymore
			purple_signal_emit(tc_def(TC_PLUGIN), "twitter-message", ma, tlr->name, cur_msg);
			g_free(msg_txt);
		}
		g_free(id_str);
		g_free(cur_msg->msg_txt);
		g_free(cur_msg->from);
		g_free(cur_msg->avatar_url);
		g_free(cur_msg);
		it->data = NULL;
	}
	if(ma->last_msg_time < last_msg_time_t) {
		ma->last_msg_time = last_msg_time_t;
	}
	g_list_free(msg_list);
	if(tlr->sys_msg) {
		serv_got_im(ma->gc, tlr->name, tlr->sys_msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
	}
	twitter_free_tlr(tlr);
	return 0;
}


//
// Check for new message periodically
//
void twitter_fetch_new_messages(MbAccount * ta, TwitterTimeLineReq * tlr)
{
	MbConnData * conn_data;
	gchar * twitter_host, * user_name;
	gboolean use_https;
	gint twitter_port;
	
	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);

	twitter_get_user_host(ta, &user_name, &twitter_host);
	use_https = purple_account_get_bool(ta->account, tc_name(TC_USE_HTTPS), tc_def_bool(TC_USE_HTTPS));
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}
	
	conn_data = twitter_init_connection(ta, twitter_host, twitter_port, HTTP_GET, tlr->path,
			user_name, purple_account_get_password(ta->account),twitter_fetch_new_messages_handler, use_https);

	if(tlr->count > 0) {
		purple_debug_info(DBGID, "tlr->count = %d\n", tlr->count);
		mb_http_data_add_param_int(conn_data->request, "count", tlr->count);
	}
	if(tlr->use_since_id && (ta->last_msg_id > 0) ) {
		mb_http_data_add_param_ull(conn_data->request, "since_id", ta->last_msg_id);
	}
	if(tlr->screen_name != NULL) {
		mb_http_data_add_param(conn_data->request, "screen_name", tlr->screen_name);
	}
	conn_data->handler_data = tlr;
	
	mb_conn_process_request(conn_data);
	g_free(twitter_host);
	g_free(user_name);
}



//
// Generate 'fake' buddy list for Twitter
// For now, we only add TwFriends, TwUsers, and TwPublic
void twitter_get_buddy_list(TwitterAccount * ta)
{
	PurpleBuddy *buddy;
	TwitterBuddy *tbuddy;
	PurpleGroup *twitter_group = NULL;

	purple_debug_info(DBGID, "buddy list for account %s\n", ta->account->username);

	//Check if the twitter group already exists
	twitter_group = purple_find_group(tc_def(TC_USER_GROUP));
	
	// Add timeline as "fake" user
	// Is TL_FRIENDS already exist?
	if ( (buddy = purple_find_buddy(ta->account, tc_def(TC_FRIENDS_USER))) == NULL)
	{
		purple_debug_info(DBGID, "creating new buddy list for %s\n", tc_def(TC_FRIENDS_USER));
		buddy = purple_buddy_new(ta->account, tc_def(TC_FRIENDS_USER), NULL);
		if (twitter_group == NULL)
		{
			purple_debug_info(DBGID, "creating new Twitter group\n");
			twitter_group = purple_group_new(tc_def(TC_USER_GROUP));
			purple_blist_add_group(twitter_group, NULL);
		}
		purple_debug_info(DBGID, "setting protocol-specific buddy information to purplebuddy\n");
		if(buddy->proto_data == NULL) {
			tbuddy = twitter_new_buddy();
			buddy->proto_data = tbuddy;
			tbuddy->buddy = buddy;
			tbuddy->ta = ta;
			tbuddy->uid = TL_FRIENDS;
			tbuddy->name = g_strdup(tc_def(TC_FRIENDS_USER));
		}
		purple_blist_add_buddy(buddy, NULL, twitter_group, NULL);
	}
	purple_prpl_got_user_status(ta->account, buddy->name, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE), NULL);
	// We'll deal with public and users timeline later
}

gint twitter_verify_authen(MbConnData * conn_data, gpointer data)
{
	MbAccount * ma = conn_data->ma;
	MbHttpData * response = conn_data->response;
	
	if(response->status == HTTP_OK) {
		gint interval = purple_account_get_int(conn_data->ma->account, tc_name(TC_MSG_REFRESH_RATE), tc_def_int(TC_MSG_REFRESH_RATE));
		
		purple_connection_set_state(conn_data->ma->gc, PURPLE_CONNECTED);
		conn_data->ma->state = PURPLE_CONNECTED;
		twitter_get_buddy_list(conn_data->ma);
		purple_debug_info(DBGID, "refresh interval = %d\n", interval);
		conn_data->ma->timeline_timer = purple_timeout_add_seconds(interval, (GSourceFunc)twitter_fetch_all_new_messages, conn_data->ma);
		twitter_fetch_first_new_messages(conn_data->ma);
		return 0;
	} else {
		// XXX: Crash at the line below
		purple_connection_set_state(conn_data->ma->gc, PURPLE_DISCONNECTED);
		conn_data->ma->state = PURPLE_DISCONNECTED;
		purple_connection_error(ma->gc, _("Authentication error"));
		return -1;
	}
}

MbAccount * mb_account_new(PurpleAccount * acct)
{
	MbAccount * ma = NULL;
	
	purple_debug_info(DBGID, "%s\n", __FUNCTION__);
	ma = g_new(MbAccount, 1);
	ma->account = acct;
	ma->gc = acct->gc;
	ma->state = PURPLE_CONNECTING;
	ma->timeline_timer = -1;
	ma->last_msg_id = mb_account_get_ull(acct, TW_ACCT_LAST_MSG_ID, 0);
	ma->last_msg_time = 0;
	ma->conn_data_list = NULL;
	ma->sent_id_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ma->tag = NULL;
	ma->tag_pos = MB_TAG_NONE;
	ma->reply_to_status_id = 0;
	// Cache
//	ma->cache = mb_cache_new();
	acct->gc->proto_data = ma;
	return ma;
}

/*
static void mb_close_connection(gpointer key, gpointer value, gpointer user_data)
{
	MbConnData *conn_data = value;
	
	purple_debug_info(DBGID, "closing each connection\n");
	if(conn_data) {
		purple_debug_info(DBGID, "we have %p -> %p\n", key, value);
		mb_conn_data_free(conn_data);
	}	
}
*/

static gboolean foreach_remove_expire_idhash(gpointer key, gpointer val, gpointer userdata)
{
	MbAccount * ma = (MbAccount *)userdata;
	unsigned long long msg_id;

	msg_id = strtoull(key, NULL, 10);
	if(ma->last_msg_id >= msg_id) {
		purple_debug_info(DBGID, "removing %s since it is less than %llu\n", (gchar *)key, ma->last_msg_id);
		return TRUE;
	} else {
		return FALSE;
	}
}

void mb_account_free(MbAccount * ma)
{	
	guint num_remove;

	//purple_debug_info(DBGID, "mb_account_free\n");
	purple_debug_info(DBGID, "%s\n", __FUNCTION__);

	// Remove cache
//	mb_cache_free(ma->cache);
	ma->cache = NULL;
	
	if(ma->tag) {
		g_free(ma->tag);
		ma->tag = NULL;
	}
	ma->tag_pos = MB_TAG_NONE;
	ma->state = PURPLE_DISCONNECTED;
	
	if(ma->timeline_timer != -1) {
		purple_debug_info(DBGID, "removing timer\n");
		purple_timeout_remove(ma->timeline_timer);
	}

	while(ma->conn_data_list) {
		purple_debug_info(DBGID, "free-up connection with fetch_url_data = %p\n", ((MbConnData *)ma->conn_data_list->data)->fetch_url_data);
		mb_conn_data_free(ma->conn_data_list->data);
		// don't need to delete the list, it will be deleted by conn_data_free eventually
	}

	num_remove = g_hash_table_foreach_remove(ma->sent_id_hash, foreach_remove_expire_idhash, ma);
	purple_debug_info(DBGID, "%u key removed\n", num_remove);
	mb_account_set_idhash(ma->account, TW_ACCT_SENT_MSG_IDS, ma->sent_id_hash);
	if(ma->sent_id_hash) {
		purple_debug_info(DBGID, "destroying sent_id hash\n");
		g_hash_table_destroy(ma->sent_id_hash);
		ma->sent_id_hash = NULL;
	}
	
	ma->account = NULL;
	ma->gc = NULL;
	
	purple_debug_info(DBGID, "free up memory used for microblog account structure\n");
	g_free(ma);
}

void twitter_login(PurpleAccount *acct)
{
	MbAccount *ta = NULL;
	MbConnData * conn_data = NULL;
	gchar * twitter_host = NULL;
	gchar * path = NULL, * user_name = NULL;
	gboolean use_https = TRUE;
	
	purple_debug_info(DBGID, "twitter_login\n");
	
	// Create account data
	ta = mb_account_new(acct);

	purple_debug_info(DBGID, "creating id hash for sentid\n");
	mb_account_get_idhash(acct, TW_ACCT_SENT_MSG_IDS,ta->sent_id_hash);

	purple_debug_info(DBGID, "getting user/host\n");
	twitter_get_user_host(ta, &user_name, &twitter_host);

	purple_debug_info(DBGID, "user_name = %s\n", user_name);
	path = g_strdup(purple_account_get_string(ta->account, tc_name(TC_VERIFY_PATH), tc_def(TC_VERIFY_PATH)));
	use_https = purple_account_get_bool(ta->account, tc_name(TC_USE_HTTPS), tc_def_int(TC_USE_HTTPS));
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}
	purple_debug_info(DBGID, "path = %s\n", path);
	
	conn_data = twitter_init_connection(ta, twitter_host, twitter_port, HTTP_GET, path,
			user_name, purple_account_get_password(ta->account),twitter_verify_authen, use_https);

	mb_conn_process_request(conn_data);
	g_free(twitter_host);
	g_free(user_name);
	g_free(path);

	// connect to twitgin here
	purple_debug_info(DBGID, "looking for twitgin\n");
	twitgin_plugin = purple_plugins_find_with_id("gtktwitgin");
	if(twitgin_plugin) {
		purple_debug_info(DBGID, "registering twitgin-replying-message signal\n");
		purple_signal_connect(twitgin_plugin, "twitgin-replying-message", acct, PURPLE_CALLBACK(twitter_on_replying_message), ta);
	}
	
}


void twitter_close(PurpleConnection *gc)
{
	MbAccount *ma = gc->proto_data;

	if(twitgin_plugin) {
		purple_signal_disconnect(twitgin_plugin, "twitgin-replying-message", ma->account, PURPLE_CALLBACK(twitter_on_replying_message));
	}

	purple_debug_info(DBGID, "twitter_close\n");

	if(ma->timeline_timer != -1) {
		purple_debug_info(DBGID, "removing timer\n");
		purple_timeout_remove(ma->timeline_timer);
		ma->timeline_timer = -1;
	}
	//purple_timeout_add(300, (GSourceFunc)twitter_close_timer, ma);
	mb_account_free(ma);
	gc->proto_data = NULL;
}

gint twitter_send_im_handler(MbConnData * conn_data, gpointer data)
{
	MbAccount * ma = conn_data->ma;
	MbHttpData * response = conn_data->response;
	gchar * id_str = NULL;
	xmlnode * top, *id_node;
	
	purple_debug_info(DBGID, "send_im_handler\n");
	
	if(response->status != 200) {
		purple_debug_info(DBGID, "http error\n");
		//purple_debug_info(DBGID, "http data = #%s#\n", response->content->str);
		return -1;
	}
	
	if(!purple_account_get_bool(ma->account, tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF))) {
		return 0;
	}
	
	// Check for returned ID
	if(response->content->len == 0) {
		purple_debug_info(DBGID, "can not find http data\n");
		return -1;
	}

	purple_debug_info(DBGID, "http_data = #%s#\n", response->content->str);
	
	// parse response XML
	top = xmlnode_from_str(response->content->str, -1);
	if(top == NULL) {
		purple_debug_info(DBGID, "failed to parse XML data\n");
		return -1;
	}
	purple_debug_info(DBGID, "successfully parse XML\n");

	// ID
	id_node = xmlnode_get_child(top, "id");
	if(id_node) {
		id_str = xmlnode_get_data_unescaped(id_node);
	}

	// save it to account
	g_hash_table_insert(ma->sent_id_hash, id_str, id_str);
	
	//hash_table supposed to free this for use
	//g_free(id_str);
	xmlnode_free(top);
	return 0;
}

int twitter_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	TwitterAccount * ta = gc->proto_data;
	MbConnData * conn_data = NULL;
	gchar * post_data = NULL, * tmp_msg_txt = NULL, * user_name = NULL;
	gint msg_len, twitter_port, len;
	gchar * twitter_host, * path;
	gboolean use_https;
	
	purple_debug_info(DBGID, "send_im\n");

	// prepare message to send
	tmp_msg_txt = g_strdup(purple_url_encode(g_strchomp(purple_markup_strip_html(message))));
	if(ta->tag) {
		gchar * new_msg_txt;

		if(ta->tag_pos == MB_TAG_PREFIX) {
			new_msg_txt  = g_strdup_printf("%s %s", ta->tag, tmp_msg_txt);
		} else {
			new_msg_txt  = g_strdup_printf("%s %s", tmp_msg_txt, ta->tag);
		}
		g_free(tmp_msg_txt);
		tmp_msg_txt = new_msg_txt;
	}
	msg_len = strlen(tmp_msg_txt);

	purple_debug_info(DBGID, "sending message %s\n", tmp_msg_txt);
	
	// connection
	twitter_get_user_host(ta, &user_name, &twitter_host);
	path = g_strdup(purple_account_get_string(ta->account, tc_name(TC_STATUS_UPDATE), tc_def(TC_STATUS_UPDATE)));
	use_https = purple_account_get_bool(ta->account, tc_name(TC_USE_HTTPS), tc_def_bool(TC_USE_HTTPS));
	
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}
	conn_data = twitter_init_connection(ta, twitter_host, twitter_port, HTTP_POST, path,
			user_name, purple_account_get_password(ta->account),twitter_send_im_handler, use_https);

	if(ta->reply_to_status_id > 0) {
		int i;
		gboolean do_reply = FALSE;
		// do not add reply tag if the message does not contains @ in the front
		for(i = 0; i < strlen(message); i++) {
			if(isspace(message[i])) {
				continue;
			} else {
				if(message[i] == '@') {
					do_reply = TRUE;
				}
				break;
			}
		}
		if(do_reply) {
			purple_debug_info(DBGID, "setting in_reply_to_status_id = %llu\n", ta->reply_to_status_id);
			mb_http_data_add_param_ull(conn_data->request, "in_reply_to_status_id", ta->reply_to_status_id);
			ta->reply_to_status_id = 0;
		} else {
			ta->reply_to_status_id = 0;
		}
	}
	
	post_data = g_malloc(TW_MAXBUFF);
	len = snprintf(post_data, TW_MAXBUFF, "status=%s&source=" TW_AGENT_SOURCE, tmp_msg_txt);
	mb_http_data_set_content(conn_data->request, post_data, len);
	
	mb_conn_process_request(conn_data);
	g_free(twitter_host);
	g_free(user_name);
	g_free(path);
	g_free(post_data);
	g_free(tmp_msg_txt);
	
	return msg_len;
}


gchar * twitter_status_text(PurpleBuddy *buddy)
{
	TwitterBuddy * tbuddy = buddy->proto_data;
	
	if (tbuddy && tbuddy->status && strlen(tbuddy->status))
		return g_strdup(tbuddy->status);
	
	return NULL;
}

// There's no concept of status in TwitterIM for now
void twitter_set_status(PurpleAccount *acct, PurpleStatus *status)
{
  const char *msg = purple_status_get_attr_string(status, "message");
  purple_debug_info(DBGID, "setting %s's status to %s: %s\n",
                    acct->username, purple_status_get_name(status), msg);

}

void * twitter_on_replying_message(gchar * proto, unsigned long long msg_id, MbAccount * ma)
{
	purple_debug_info(DBGID, "%s called!\n", __FUNCTION__);
	purple_debug_info(DBGID, "setting reply_to_id (was %llu) to %llu\n", ma->reply_to_status_id, msg_id);
	ma->reply_to_status_id = msg_id;
	return NULL;
}

/*
*  Favourite Handler
*/
void twitter_favorite_message(MbAccount * ta, gchar * msg_id)
{

	// create new connection and call API POST
	MbConnData * conn_data;
	MbHttpData * request;
	gchar * twitter_host, * user_name, * path;
	gboolean use_https;
	gint twitter_port;

	user_name = g_strdup_printf("%s", purple_account_get_username(ta->account));
	twitter_host = g_strdup_printf("%s", "twitter.com");
	path = g_strdup_printf("/favorites/create/%s.xml", msg_id);

	use_https = TRUE;
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}

	conn_data = twitter_init_connection(ta, twitter_host, twitter_port, HTTP_POST, path,
			user_name, purple_account_get_password(ta->account), NULL, use_https);

	mb_conn_process_request(conn_data);
	g_free(twitter_host);
	g_free(user_name);
	g_free(path);

}

/*
*  Retweet API Handler
*/
void twitter_retweet_message(MbAccount * ta, gchar * msg_id)
{

	// create new connection and call API POST
	MbConnData * conn_data;
	gchar * twitter_host, * user_name, * path;
	gboolean use_https;
	gint twitter_port;

	user_name = g_strdup_printf("%s", purple_account_get_username(ta->account));
	twitter_host = g_strdup_printf("%s", "twitter.com");
	path = g_strdup_printf("/statuses/retweet/%s.xml", msg_id);

	use_https = TRUE;
	if(use_https) {
			twitter_port = TW_HTTPS_PORT;
	} else {
			twitter_port = TW_HTTP_PORT;
	}

	conn_data = twitter_init_connection(ta, twitter_host, twitter_port, HTTP_POST, path,
			user_name, purple_account_get_password(ta->account), NULL, use_https);

	mb_conn_process_request(conn_data);
	g_free(twitter_host);
	g_free(user_name);
	g_free(path);

}
