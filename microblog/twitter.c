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

#include "mb_net.h"
#include "mb_util.h"

#ifdef _WIN32
#	include <win32dep.h>
#else
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#endif

#include "twitter.h"

//static const char twitter_host[] = "twitter.com";
static gint twitter_port = 443;
static const char twitter_fixed_headers[] = "User-Agent:" TW_AGENT "\r\n" \
"Accept: */*\r\n" \
"X-Twitter-Client: " TW_AGENT_SOURCE "\r\n" \
"X-Twitter-Client-Version: 0.1\r\n" \
"X-Twitter-Client-Url: " TW_AGENT_DESC_URL "\r\n" \
"Connection: Close\r\n" \
"Pragma: no-cache\r\n";

static void twitter_fetch_new_messages(MbAccount * ta, TwitterTimeLineReq * tlr);
static void get_user_host(TwitterAccount * ta, char ** user_name, char ** host);

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

static TwitterTimeLineReq * twitter_new_tlr()
{
	TwitterTimeLineReq * tlr = g_new(TwitterTimeLineReq, 1);
	tlr->path = NULL;
	tlr->name = NULL;
	tlr->count = 0;
	tlr->timeline_id = -1;
	return tlr;
}

static void twitter_free_tlr(TwitterTimeLineReq * tlr)
{
	if(tlr->path != NULL) g_free(tlr->path);
	if(tlr->name != NULL) g_free(tlr->name);
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

// Function to fetch first batch of new message
void twitter_fetch_first_new_messages(TwitterAccount * ta)
{
	TwitterTimeLineReq * tlr = twitter_new_tlr();
	
	tlr->path = g_strdup(purple_account_get_string(ta->account, tc_name(TC_FRIENDS_TIMELINE), tc_def(TC_FRIENDS_TIMELINE)));
	tlr->name = g_strdup(tc_def(TC_FRIENDS_USER));
	tlr->timeline_id = TL_FRIENDS;
	tlr->count = purple_account_get_int(ta->account, tc_name(TC_INITIAL_TWEET), tc_def_int(TC_INITIAL_TWEET));
	twitter_fetch_new_messages(ta, tlr);
}

// Function to fetch all new messages periodically
gboolean twitter_fetch_all_new_messages(gpointer data)
{
	TwitterAccount * ta = data;
	TwitterTimeLineReq * tlr = NULL;
	gint i;
	
	for(i = TC_FRIENDS_TIMELINE; i <= TC_USER_TIMELINE; i+=2) {
		//FIXME: i + 1 is not a very good strategy here
		if(!purple_find_buddy(ta->account, tc_def(i + 1))) {
			purple_debug_info("twitter", "skipping %s\n", tlr->name);
			continue;
		}
		tlr = twitter_new_tlr();
		tlr->path = g_strdup(purple_account_get_string(ta->account, tc_name(i), tc_def(i)));
		tlr->name = g_strdup(tc_def(i + 1));
		purple_debug_info("twitter", "fetching updates from %s to %s\n", tlr->path, tlr->name);
		tlr->timeline_id = i;
		tlr->count = TW_STATUS_COUNT_MAX;
		twitter_fetch_new_messages(ta, tlr);
	}
	return TRUE;
}

gchar* twitter_format_symbols(gchar* src) {
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
static void twitter_list_sent_id_hash(gpointer key, gpointer value, gpointer user_data)
{
	purple_debug_info("twitter", "key/value = %s/%s\n", key, value);
}
#endif

gint twitter_fetch_new_messages_handler(MbConnData * conn_data, gpointer data)
{
	MbAccount * ta = conn_data->ta;
	const gchar * username;
	MbHttpData * response = conn_data->response;
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
	
	username = (const gchar *)purple_account_get_username(ta->account);
	
	if(response->status == HTTP_MOVED_TEMPORARILY) {
		// no new messages
		twitter_free_tlr(tlr);
		purple_debug_info("twitter", "no new messages\n");
		return 0;
	}
	if(response->status != HTTP_OK) {
		twitter_free_tlr(tlr);
		purple_debug_info("twitter", "something's wrong with the message\n");
		return 0; //< should we return -1 instead?
	}
	if(response->content_len == 0) {
		purple_debug_info("twitter", "no data to parse\n");
		twitter_free_tlr(tlr);
		return 0;
	}
	purple_debug_info("twitter", "http_data = #%s#\n", response->content->str);
	top = xmlnode_from_str(response->content->str, -1);
	if(top == NULL) {
		purple_debug_info("twitter", "failed to parse XML data\n");
		twitter_free_tlr(tlr);
		return 0;
	}
	purple_debug_info("twitter", "successfully parse XML\n");
	status = xmlnode_get_child(top, "status");
	purple_debug_info("twitter", "timezone = %ld\n", timezone);
	
	hide_myself = purple_account_get_bool(ta->account, tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF));
	
	while(status) {
		msg_txt = NULL;
		from = NULL;
		xml_str = NULL;
		skip = FALSE;
		
		// ID
		id_node = xmlnode_get_child(status, "id");
		if(id_node) {
			xml_str = xmlnode_get_data_unescaped(id_node);
		}
		// Check for duplicate message
		if(hide_myself) {
			purple_debug_info("twitter", "checking for duplicate message\n");
#if 0
			g_hash_table_foreach(ta->sent_id_hash, twitter_list_sent_id_hash, NULL);
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
			if(purple_account_get_bool(ta->account, tc_name(TC_REPLY_LINK), tc_def_bool(TC_REPLY_LINK))) {
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
				fmt_txt = twitter_format_symbols(cur_msg->msg_txt);
				serv_got_im(ta->gc, tlr->name, fmt_txt, PURPLE_MESSAGE_RECV, cur_msg->msg_time);
				g_free(fmt_txt);
			}
		}
		g_free(cur_msg->msg_txt);
		g_free(cur_msg);
		it->data = NULL;
	}
	if(ta->last_msg_time < last_msg_time_t) {
		ta->last_msg_time = last_msg_time_t;
	}
	g_list_free(msg_list);
	xmlnode_free(top);
	twitter_free_tlr(tlr);
	return 0;
}

//
// Check for new message periodically
//
static void twitter_fetch_new_messages(MbAccount * ta, TwitterTimeLineReq * tlr)
{
	MbConnData * conn_data;
	MbHttpData * request;
	gchar * twitter_host, * user_name;
	gboolean use_https;
	gint twitter_port;
	
	purple_debug_info("twitter", "fetch_new_messages\n");

	get_user_host(ta, &user_name, &twitter_host);
	use_https = purple_account_get_bool(ta->account, tc_name(TC_USE_HTTPS), tc_def_bool(TC_USE_HTTPS));
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}
	
	conn_data = mb_conn_data_new(ta, twitter_host, twitter_port, twitter_fetch_new_messages_handler, use_https);
	mb_conn_data_set_error(conn_data, "Fetching status error", MB_ERROR_NOACTION);
	mb_conn_data_set_retry(conn_data, 0);
	
	request = conn_data->request;
	request->type = HTTP_GET;
	request->port = twitter_port;

	mb_http_data_set_host(request, twitter_host);
	mb_http_data_set_path(request, tlr->path);
	mb_http_data_set_fixed_headers(request, twitter_fixed_headers);
	mb_http_data_set_header(request, "Host", twitter_host);
	mb_http_data_set_basicauth(request, user_name, purple_account_get_password(ta->account));
	mb_http_data_add_param_int(request, "count", tlr->count);
	if(ta->last_msg_id > 0) {
		mb_http_data_add_param_int(request, "since_id", ta->last_msg_id);
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

	purple_debug_info("twitter", "buddy list for account %s\n", ta->account->username);

	//Check if the twitter group already exists
	twitter_group = purple_find_group(tc_def(TC_USER_GROUP));
	
	// Add timeline as "fake" user
	// Is TL_FRIENDS already exist?
	if ( (buddy = purple_find_buddy(ta->account, tc_def(TC_FRIENDS_USER))) == NULL)
	{
		purple_debug_info("twitter", "creating new buddy list for %s\n", tc_def(TC_FRIENDS_USER));
		buddy = purple_buddy_new(ta->account, tc_def(TC_FRIENDS_USER), NULL);
		if (twitter_group == NULL)
		{
			purple_debug_info("twitter", "creating new Twitter group\n");
			twitter_group = purple_group_new(tc_def(TC_USER_GROUP));
			purple_blist_add_group(twitter_group, NULL);
		}
		purple_debug_info("twitter", "setting protocol-specific buddy information to purplebuddy\n");
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
	MbAccount * ta = conn_data->ta;
	MbHttpData * response = conn_data->response;
	
	if(response->status == HTTP_OK) {
		gint interval = purple_account_get_int(conn_data->ta->account, tc_name(TC_MSG_REFRESH_RATE), tc_def_int(TC_MSG_REFRESH_RATE));
		
		purple_connection_set_state(conn_data->ta->gc, PURPLE_CONNECTED);
		conn_data->ta->state = PURPLE_CONNECTED;
		twitter_get_buddy_list(conn_data->ta);
		purple_debug_info("twitter", "refresh interval = %d\n", interval);
		conn_data->ta->timeline_timer = purple_timeout_add_seconds(interval, (GSourceFunc)twitter_fetch_all_new_messages, conn_data->ta);
		twitter_fetch_first_new_messages(conn_data->ta);
		return 0;
	} else {
		purple_connection_set_state(conn_data->ta->gc, PURPLE_DISCONNECTED);
		conn_data->ta->state = PURPLE_DISCONNECTED;
		purple_connection_error(ta->gc, _("Authentication error"));
		return -1;
	}
}

MbAccount * mb_account_new(PurpleAccount * acct)
{
	MbAccount * ta = NULL;
	
	purple_debug_info("twitter", "mb_account_new\n");
	ta = g_new(MbAccount, 1);
	ta->account = acct;
	ta->gc = acct->gc;
	ta->state = PURPLE_CONNECTING;
	ta->timeline_timer = -1;
	ta->last_msg_id = 0;
	ta->last_msg_time = 0;
	ta->conn_hash = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	ta->ssl_conn_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	ta->sent_id_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	acct->gc->proto_data = ta;
	return ta;
}

static void mb_close_connection(gpointer key, gpointer value, gpointer user_data)
{
	MbConnData *conn_data = value;
	
	purple_debug_info("twitter", "closing each connection\n");
	if(conn_data) {
		/*
		purple_debug_info("twitter", "is_https = %d\n", is_https);
		if(is_https) {
			PurpleSslConnection * ssl = NULL;
			
			ssl = conn_data->ssl_conn_data;
			if(ssl) {
				purple_debug_info("twitter", "removing current ssl socket from eventloop\n");
				purple_input_remove(ssl->inpa);
			}
			purple_debug_info("twitter", "closing SSL socket\n");
		} else {
			purple_debug_info("twitter", "cancelling connection with handle = %p\n", conn_data);
			purple_proxy_connect_cancel_with_handle(conn_data);
			if(conn_data->conn_event_handle >= 0) {
				purple_debug_info("twitter", "removing event handle\n");
				purple_input_remove(conn_data->conn_event_handle);
			}
		}
		purple_debug_info("twitter", "free all data\n");
		*/
		purple_debug_info("twitter", "we have %p -> %p\n", key, value);
		mb_conn_data_free(conn_data);
	}	
}

void mb_account_free(MbAccount * ta)
{	
	purple_debug_info("twitter", "mb_account_free\n");
	
	ta->state = PURPLE_DISCONNECTED;
	
	if(ta->timeline_timer != -1) {
		purple_debug_info("twitter", "removing timer\n");
		purple_timeout_remove(ta->timeline_timer);
	}

	// new SSL-base connection hash
	
	if(ta->ssl_conn_hash) {
		purple_debug_info("twitter", "closing all active connection\n");
		g_hash_table_foreach(ta->ssl_conn_hash, mb_close_connection, (gpointer)TRUE);
		purple_debug_info("twitter", "destroying connection hash\n");
		g_hash_table_destroy(ta->ssl_conn_hash);
		ta->ssl_conn_hash = NULL;
	}
	
	if(ta->conn_hash) {
		purple_debug_info("twitter", "closing all non-ssl active connection\n");
		g_hash_table_foreach(ta->conn_hash, mb_close_connection, (gpointer)FALSE);
		purple_debug_info("twitter", "destroying non-SSL connection hash\n");
		g_hash_table_destroy(ta->conn_hash);
		ta->conn_hash = NULL;
	}
	
	if(ta->sent_id_hash) {
		purple_debug_info("twitter", "destroying sent_id hash\n");
		g_hash_table_destroy(ta->sent_id_hash);
		ta->sent_id_hash = NULL;
	}
	
	ta->account = NULL;
	ta->gc = NULL;
	
	purple_debug_info("twitter", "free up memory used for microblog account structure\n");
	g_free(ta);
}

static void get_user_host(TwitterAccount * ta, char ** user_name, char ** host)
{
	char * at_sign = NULL;

	(*user_name) = g_strdup(purple_account_get_username(ta->account));
	if( (at_sign = strchr(*user_name, '@')) == NULL) {
		(*host) = g_strdup(purple_account_get_string(ta->account, tc_name(TC_HOST), tc_def(TC_HOST)));
	} else {
		(*at_sign) = '\0';
		(*host) = g_strdup( at_sign + 1 );
	}
}

void twitter_login(PurpleAccount *acct)
{
	MbAccount *ta = NULL;
	MbConnData * conn_data = NULL;
	gchar * twitter_host = NULL;
	gchar * path = NULL, *user_name;
	gboolean use_https = TRUE;
	
	purple_debug_info("twitter", "twitter_login\n");
	
	// Create account data
	ta = mb_account_new(acct);

	get_user_host(ta, &user_name, &twitter_host);

	purple_debug_info("twitter", "user_name = %s\n", user_name);
	path = g_strdup(purple_account_get_string(ta->account, tc_name(TC_VERIFY_PATH), tc_def(TC_VERIFY_PATH)));
	use_https = purple_account_get_bool(ta->account, tc_name(TC_USE_HTTPS), tc_def_int(TC_USE_HTTPS));
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}
	purple_debug_info("twitter", "path = %s\n", path);
	
	conn_data = mb_conn_data_new(ta, twitter_host, twitter_port, twitter_verify_authen, use_https);
	mb_conn_data_set_error(conn_data, "Authentication error", MB_ERROR_RAISE_ERROR);
	mb_conn_data_set_retry(conn_data, purple_account_get_int(acct, tc_name(TC_GLOBAL_RETRY), tc_def_int(TC_GLOBAL_RETRY)));
	
	conn_data->request->type = HTTP_GET;
	mb_http_data_set_host(conn_data->request, twitter_host);
	mb_http_data_set_path(conn_data->request, path);
	mb_http_data_set_fixed_headers(conn_data->request, twitter_fixed_headers);
	mb_http_data_set_header(conn_data->request, "Host", twitter_host);
	mb_http_data_set_basicauth(conn_data->request, user_name, purple_account_get_password(ta->account));

	//purple_proxy_connect(NULL, ta->account, twitter_host, twitter_port, test_connect_cb, NULL);
	mb_conn_process_request(conn_data);
	g_free(twitter_host);
	g_free(user_name);
	g_free(path);
	
}


void twitter_close(PurpleConnection *gc)
{
	MbAccount *ta = gc->proto_data;

	purple_debug_info("twitter", "twitter_close\n");
	mb_account_free(ta);
	gc->proto_data = NULL;
}

gint twitter_send_im_handler(MbConnData * conn_data, gpointer data)
{
	MbAccount * ta = conn_data->ta;
	MbHttpData * response = conn_data->response;
	gchar * id_str = NULL;
	xmlnode * top, *id_node;
	
	purple_debug_info("twitter", "send_im_handler\n");
	
	if(response->status != 200) {
		purple_debug_info("twitter", "http error\n");
		purple_debug_info("twitter", "http data = #%s#\n", response->content->str);
		return -1;
	}
	
	if(!purple_account_get_bool(ta->account, tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF))) {
		return 0;
	}
	
	// Check for returned ID
	if(response->content->len == 0) {
		purple_debug_info("twitter", "can not find http data\n");
		return -1;
	}

	purple_debug_info("twitter", "http_data = #%s#\n", response->content->str);
	
	// parse response XML
	top = xmlnode_from_str(response->content->str, -1);
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

int twitter_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	TwitterAccount * ta = gc->proto_data;
	MbConnData * conn_data = NULL;
	gchar * post_data = NULL, * tmp_msg_txt = NULL, * user_name = NULL;
	gint msg_len, twitter_port, len;
	gchar * twitter_host, * path;
	gboolean use_https;
	
	purple_debug_info("twitter", "send_im\n");

	// prepare message to send
	tmp_msg_txt = g_strdup(purple_url_encode(g_strchomp(purple_markup_strip_html(message))));
	msg_len = strlen(message);
	purple_debug_info("twitter", "sending message %s\n", tmp_msg_txt);
	
	// connection
	get_user_host(ta, &user_name, &twitter_host);
	path = g_strdup(purple_account_get_string(ta->account, tc_name(TC_STATUS_UPDATE), tc_def(TC_STATUS_UPDATE)));
	use_https = purple_account_get_bool(ta->account, tc_name(TC_USE_HTTPS), tc_def_bool(TC_USE_HTTPS));
	
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}
	conn_data = mb_conn_data_new(ta, twitter_host, twitter_port, twitter_send_im_handler, use_https);
	mb_conn_data_set_error(conn_data, "Sending status error", MB_ERROR_NOACTION);
	mb_conn_data_set_retry(conn_data, 0);
	conn_data->request->type = HTTP_POST;
	mb_http_data_set_host(conn_data->request, twitter_host);
	mb_http_data_set_path(conn_data->request, path);
	mb_http_data_set_fixed_headers(conn_data->request, twitter_fixed_headers);
	mb_http_data_set_header(conn_data->request, "Content-Type", "application/x-www-form-urlencoded");
	mb_http_data_set_header(conn_data->request, "Host", twitter_host);
	mb_http_data_set_basicauth(conn_data->request, 	user_name,purple_account_get_password(ta->account));
	
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
void twitter_set_status(PurpleAccount *acct, PurpleStatus *status) {
  const char *msg = purple_status_get_attr_string(status, "message");
  purple_debug_info("twitter", "setting %s's status to %s: %s\n",
                    acct->username, purple_status_get_name(status), msg);

}