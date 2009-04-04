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
#define TW_ACCT_LAST_REPLIES_ID "twitter_last_replies_id"

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

static TwitterBuddy * twitter_new_buddy()
{
	TwitterBuddy * buddy = g_new0(TwitterBuddy, 1);
	
	buddy->ta = NULL;
	buddy->buddy = NULL;
	buddy->uid = -1;
	buddy->name = NULL;
	buddy->status = NULL;
	buddy->thumb_url = NULL;
	buddy->type = TWITTER_BUDDY_TYPE_UNKNOWN;
	
	return buddy;
}

static TwitterBuddy * twitter_find_buddy(TwitterAccount *ac, const gchar *who)
{
	PurpleBuddy *b = purple_find_buddy(ac->account, who);
	return (b ? b->proto_data : NULL);
}

TwitterTimeLineReq * twitter_new_tlr(const char * path, const char * name, int id, unsigned int count, const char * sys_msg,
		TlrSuccessFunc success_cb, TlrErrorFunc error_cb)
{
	TwitterTimeLineReq * tlr = g_new(TwitterTimeLineReq, 1);
	tlr->path = g_strdup(path);
	tlr->name = g_strdup(name);
	tlr->count = count;
	tlr->timeline_id = id;
	tlr->use_since_id = TRUE;
	if(sys_msg) {
		tlr->sys_msg = g_strdup(sys_msg);
	} else {
		tlr->sys_msg = NULL;
	}
	tlr->success_cb = success_cb;
	tlr->error_cb = error_cb;
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
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, NULL, _("Online"), TRUE, TRUE, FALSE, "message", _("Message"), purple_value_new(PURPLE_TYPE_STRING), "message_date", _("Message changed"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	//status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, _("Online"), TRUE, TRUE, FALSE);
	//purple_status_type_add_attr(status, "message", _("Online"), purple_value_new(PURPLE_TYPE_STRING));

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
TwitterBuddy *twitter_buddy_add(TwitterAccount *ta, const gchar *screenname, const gchar *alias)
{
	PurpleGroup *g;
	PurpleBuddy *b = purple_buddy_new(ta->account, screenname, alias);
	TwitterBuddy *tw;

	//TODO: config
	if ((g = purple_find_group("twitter")) == NULL)
		g = purple_group_new("twitter");
	purple_blist_add_buddy(b, NULL, g, NULL);

	//TODO: function
	tw = twitter_new_buddy();
	tw->buddy = b;
	tw->ta = ta;
	tw->type = TWITTER_BUDDY_TYPE_NORMAL;
	b->proto_data = tw;
	return tw;
}

void twitter_msg_free(TwitterMsg *msg)
{
	if (!msg)
		return;
	if (msg->to) g_free(msg->to);
	if (msg->msg_txt) g_free(msg->msg_txt);
	if (msg->from) g_free(msg->from);
	if (msg->avatar_url) g_free(msg->avatar_url);
	g_free(msg);
}
void twitter_msg_list_free(GList *msg_list)
{
	return;
	GList *l;
	for (l = msg_list; l; l = l->next)
		twitter_msg_free(l->data);
	g_list_free(msg_list);
}
gboolean twitter_account_names_equal(TwitterAccount *ac, const char *name1, const char *name2)
{
	gboolean equal;
	char *name1_normalized = g_strdup(purple_normalize(ac->account, name1));
	equal = !strcmp(name1_normalized, purple_normalize(ac->account, name2));
	g_free(name1_normalized);
	return equal;
}

//this is a placeholder. TODO: Check account preferences
gchar *twitter_account_format_im(TwitterAccount *ta, TwitterMsg *cur_msg)
{
	gchar *space_loc; 
	space_loc = strchr(cur_msg->msg_txt, ' ');
	if (!space_loc)
		return NULL;
	return g_strdup(space_loc + 1);
}

gint twitter_replies_timeline_success_handler(MbAccount *ta, TwitterTimeLineReq *tlr,
		GList *msg_list, time_t last_msg_time_t)
{
	GList *it;
	// reverse the list and append it
	// only if id > last_msg_id
	for(it = g_list_first(msg_list); it; it = g_list_next(it)) {

		TwitterMsg *cur_msg = it->data;
		if(cur_msg->id > ta->last_replies_id) {
			ta->last_replies_id = cur_msg->id;
			mbpurple_account_set_ull(ta->account, TW_ACCT_LAST_REPLIES_ID, ta->last_replies_id);
		}
		//TODO: add option to not display
		if (cur_msg->to && twitter_account_names_equal(ta, ta->account->username, cur_msg->to)) {
			gchar *msg_txt = twitter_account_format_im(ta, cur_msg);
			if (msg_txt)
			{
				serv_got_im(ta->gc, cur_msg->from, msg_txt, PURPLE_MESSAGE_RECV, cur_msg->msg_time);
				g_free(msg_txt);
			}
		}
		//TODO: bug - if a user sends messages from a non-pidgin client
		//pidgin won't see the message in the conv. 
		/* else if (cur_msg->to && cur_msg->from 
				&& twitter_account_names_equal(ta, ta->account->username, cur_msg->from)
				&& !in_sent_id_hash
				&& (msg_txt = twitter_account_format_im(ta, cur_msg))) {

			//Message from account, to other user, but not sent from pidgin
			//Show in im window
			PurpleConversation *conv = purple_find_conversation_with_account(
					PURPLE_CONV_TYPE_IM, cur_msg->to,
					ta->account);
			PurpleConvIm *conv_im;
			if (!conv)
			{
				conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, ta->account, cur_msg->to);
			}
			if (conv && (conv_im = purple_conversation_get_im_data(conv)) != NULL)
			{
				purple_conversation_get_im_data(conv);
				if (conv_im)
				{
					purple_conv_im_write(conv_im, ta->account->username,
							msg_txt, PURPLE_MESSAGE_SEND,
							cur_msg->msg_time);
				}
			}
			g_free(msg_txt);
		}*/
	}
	if(ta->last_replies_time < last_msg_time_t) {
		ta->last_replies_time = last_msg_time_t;
	}
	return 0;
}
gboolean twitter_fetch_replies_messages(gpointer data)
{
	TwitterAccount *ta = data;
	TwitterTimeLineReq * tlr;
	const gchar * tl_path;

	tl_path = purple_account_get_string(ta->account, tc_name(TC_REPLIES_TIMELINE), tc_def(TC_REPLIES_TIMELINE));
	tlr = twitter_new_tlr(tl_path, tc_def(TC_REPLIES_USER), TL_REPLIES, TW_STATUS_COUNT_MAX,
			NULL, twitter_replies_timeline_success_handler, NULL);
	purple_debug_info(DBGID, "fetching replies from %s to %s\n", tlr->path, tlr->name);
	twitter_fetch_new_messages(ta, tlr);
	return TRUE;
}

gint twitter_replies_timeline_success_handler_init(MbAccount *ta, TwitterTimeLineReq *tlr,
		GList *msg_list, time_t last_msg_time_t)
{
	GList *it;
	gint interval = purple_account_get_int(ta->account, tc_name(TC_REPLIES_REFRESH_RATE), tc_def_int(TC_REPLIES_REFRESH_RATE));
	for (it = msg_list; it; it = it->next)
	{
		TwitterMsg *cur_msg = it->data;
		if (cur_msg->id > ta->last_replies_id)
		{
			ta->last_replies_id = cur_msg->id;
			mbpurple_account_set_ull(ta->account, TW_ACCT_LAST_REPLIES_ID, ta->last_replies_id);
		}
	}

	ta->replies_timeline_timer = purple_timeout_add_seconds(interval, (GSourceFunc)twitter_fetch_replies_messages, ta);
	return 0;
}

void twitter_msg_set_buddy_status(MbAccount *ta, TwitterMsg *msg)
{
	TwitterBuddy *tb;
	if (!msg || !msg->from)
		return;
	tb = twitter_find_buddy(ta, msg->from);
	if (!tb)
		return;
	if (tb->last_msg_id > msg->id)
		return;
	purple_prpl_got_user_status(ta->account, msg->from, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE),
			"message", msg->msg_txt, NULL);
}
gint twitter_friends_timeline_success_handler(MbAccount *ta, TwitterTimeLineReq *tlr,
		GList *msg_list, time_t last_msg_time_t)
{
	gboolean hide_myself = purple_account_get_bool(ta->account, tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF));
	GList *it;
	for(it = g_list_first(msg_list); it; it = g_list_next(it)) {

		gboolean in_sent_id_hash = FALSE;
		TwitterMsg *cur_msg = it->data;
		gchar *id_str = g_strdup_printf("%llu", cur_msg->id);
		if(cur_msg->id > ta->last_msg_id) {
			ta->last_msg_id = cur_msg->id;
			mbpurple_account_set_ull(ta->account, TW_ACCT_LAST_MSG_ID, ta->last_msg_id);
		}
		in_sent_id_hash = g_hash_table_remove(ta->sent_id_hash, id_str);

		if(!(hide_myself && in_sent_id_hash)) {
			gchar *msg_txt = g_strdup_printf("%s: %s", cur_msg->from, cur_msg->msg_txt);
			// we still call serv_got_im here, so purple take the message to the log
			serv_got_im(ta->gc, tlr->name, msg_txt, PURPLE_MESSAGE_RECV, cur_msg->msg_time);
			// by handling diaplying-im-msg, the message shouldn't be displayed anymore
			purple_signal_emit(tc_def(TC_PLUGIN), "twitter-message", ta, tlr->name, cur_msg);
			g_free(msg_txt);
		}

		twitter_msg_set_buddy_status(ta, cur_msg);

		g_free(id_str);
	}
	if(ta->last_msg_time < last_msg_time_t) {
		ta->last_msg_time = last_msg_time_t;
	}
	return 0;
}
gchar * twitter_msg_get_to(const char *msg)
{
	const char *msg_space_loc;
	if (msg == NULL || msg[0] != '@')
		return NULL;

	msg_space_loc = strchr(msg, ' ');
	if (!msg_space_loc)
		return NULL;

	return g_strndup(msg + 1, msg_space_loc - msg - 1);
}

GList * twitter_decode_messages(const char * data, time_t * last_msg_time) //XXX: last_msg_time shouldn't be here
{
	GList * retval = NULL;
	xmlnode * top = NULL, *id_node, *time_node, *status, * text, * user, * user_name, * image_url;
	gchar * from, * to, * msg_txt, * avatar_url, *xml_str = NULL;
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
	
	//hide_myself = purple_account_get_bool(ta->account, tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF));
	
	while(status) {
		msg_txt = NULL;
		from = NULL;
		xml_str = NULL;
		to = NULL;
		//skip = FALSE;
		
		// ID
		id_node = xmlnode_get_child(status, "id");
		if(id_node) {
			xml_str = xmlnode_get_data_unescaped(id_node);
		}
		// Check for duplicate message
		/*
		if(hide_myself) {
			purple_debug_info(DBGID, "checking for duplicate message\n");
#if 0
			g_hash_table_foreach(ta->sent_id_hash, twitter_list_sent_id_hash, NULL);
#endif			
			if(g_hash_table_remove(ta->sent_id_hash, xml_str)  == TRUE) {
				// Dupplicate ID, moved to next value
				purple_debug_info(DBGID, "duplicate id = %s\n", xml_str);
				skip = TRUE;
			}
		}
		*/
		cur_id = strtoull(xml_str, NULL, 10);
		g_free(xml_str);

		// time
		time_node = xmlnode_get_child(status, "created_at");
		if(time_node) {
			xml_str = xmlnode_get_data_unescaped(time_node);
		}
		purple_debug_info(DBGID, "msg time = %s\n", xml_str);
		msg_time_t = mb_mktime(xml_str) - timezone;
		if( (*last_msg_time) < msg_time_t) {
			(*last_msg_time) = msg_time_t;
		}
		g_free(xml_str);
		
		// message
		text = xmlnode_get_child(status, "text");
		if(text) {
			msg_txt = xmlnode_get_data_unescaped(text);
			to = twitter_msg_get_to(msg_txt);
		}

		// user name
		user = xmlnode_get_child(status, "user");
		if(user) {
			user_name = xmlnode_get_child(user, "screen_name");
			if(user_name) {
				from = xmlnode_get_data(user_name);
			}
			image_url = xmlnode_get_child(user, "profile_image_url");
			if(user_name) {
				avatar_url = xmlnode_get_data(image_url);
			}
		}

		if(from && msg_txt) {
			cur_msg = g_new(TwitterMsg, 1);
			
			purple_debug_info(DBGID, "from = %s, msg = %s\n", from, msg_txt);
			cur_msg->id = cur_id;
			cur_msg->to = to;
			cur_msg->from = from; //< actually we don't need this for now
			cur_msg->avatar_url = avatar_url; //< actually we don't need this for now
			cur_msg->msg_time = msg_time_t;
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

gint twitter_fetch_friends_handler(MbConnData * conn_data, gpointer data)
{
	//TODO: write a wrapper around all this too
	MbAccount * ta = conn_data->ta;
	MbHttpData * response = conn_data->response;
	time_t last_msg_time_t = 0;
	GList * msg_list = NULL;
	GList * it = NULL;
	
	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	
	if(response->status == HTTP_MOVED_TEMPORARILY) {
		// no new messages
		//TODO: error cb here? - Neaveru
		purple_debug_info(DBGID, "no new messages\n");
		return 0;
	}
	if(response->status != HTTP_OK) {
		//TODO: error cb here - Neaveru
		purple_debug_info(DBGID, "something's wrong with the message\n");
		return 0; //< should we return -1 instead?
	}
	if(response->content_len == 0) {
		//TODO: error cb here? - Neaveru
		purple_debug_info(DBGID, "no data to parse\n");
		return 0;
	}
	purple_debug_info(DBGID, "http_data = #%s#\n", response->content->str);
	msg_list = twitter_decode_messages(response->content->str, &last_msg_time_t);

	for(it = g_list_first(msg_list); it; it = g_list_next(it)) {

		TwitterMsg *msg = it->data;
		TwitterBuddy *tb = twitter_find_buddy(ta, msg->from);
		if (!tb)
		{
			//TODO: alias
			twitter_buddy_add(ta, msg->from, NULL);
			purple_prpl_got_user_status(ta->account, msg->from, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE),
					"message", msg->msg_txt, NULL);
		}
	}

	// reverse the list and append it
	// only if id > last_msg_id
	msg_list = g_list_reverse(msg_list);

	twitter_msg_list_free(msg_list);
	return 0;
}
void twitter_fetch_friends(TwitterAccount *ta)
{
	MbConnData * conn_data = NULL;
	gchar * post_data = NULL, * tmp_msg_txt = NULL, * user_name = NULL;
	gint twitter_port, len;
	gchar * twitter_host, * path;
	gboolean use_https;
	
	purple_debug_info(DBGID, "retrieving all friends\n");

	// connection
	//TODO: there should really be a wrapper around all this stuff
	twitter_get_user_host(ta, &user_name, &twitter_host);
	//this shouldn't be strdup'd
	path = g_strdup(purple_account_get_string(ta->account, tc_name(TC_FRIENDS_STATUSES), tc_def(TC_FRIENDS_STATUSES)));
	use_https = purple_account_get_bool(ta->account, tc_name(TC_USE_HTTPS), tc_def_bool(TC_USE_HTTPS));
	
	if(use_https) {
		twitter_port = TW_HTTPS_PORT;
	} else {
		twitter_port = TW_HTTP_PORT;
	}

	conn_data = mb_conn_data_new(ta, twitter_host, twitter_port, twitter_fetch_friends_handler, use_https);
	mb_conn_data_set_error(conn_data, "Retrieve friends error", MB_ERROR_NOACTION);
	mb_conn_data_set_retry(conn_data, 0);
	conn_data->request->type = HTTP_POST; //set_type ?
	mb_http_data_set_host(conn_data->request, twitter_host);
	mb_http_data_set_path(conn_data->request, path);
	mb_http_data_set_fixed_headers(conn_data->request, twitter_fixed_headers);
	mb_http_data_set_header(conn_data->request, "Content-Type", "application/x-www-form-urlencoded");
	mb_http_data_set_header(conn_data->request, "Host", twitter_host);
	mb_http_data_set_basicauth(conn_data->request, 	user_name,purple_account_get_password(ta->account));

	/*if(ta->reply_to_status_id > 0) {
		purple_debug_info(DBGID, "setting in_reply_to_status_id = %llu\n", ta->reply_to_status_id);
		mb_http_data_add_param_ull(conn_data->request, "in_reply_to_status_id", ta->reply_to_status_id);
		ta->reply_to_status_id = 0;
	}*/
	
	//why not g_strdup_printf ?
	post_data = g_malloc(TW_MAXBUFF);
	len = snprintf(post_data, TW_MAXBUFF, "status=%s&source=" TW_AGENT_SOURCE, tmp_msg_txt);
	mb_http_data_set_content(conn_data->request, post_data, len);
	
	mb_conn_process_request(conn_data);
	g_free(twitter_host);
	g_free(user_name);
	g_free(path);
	g_free(post_data);
	
	return;
}

// Function to fetch first batch of new message
void twitter_fetch_first_new_messages(TwitterAccount * ta)
{
	TwitterTimeLineReq * tlr;
	const gchar * tl_path;
	int count;

	twitter_fetch_friends(ta);
	
	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	tl_path = purple_account_get_string(ta->account, tc_name(TC_FRIENDS_TIMELINE), tc_def(TC_FRIENDS_TIMELINE));
	count = purple_account_get_int(ta->account, tc_name(TC_INITIAL_TWEET), tc_def_int(TC_INITIAL_TWEET));
	purple_debug_info(DBGID, "count = %d\n", count);
	tlr = twitter_new_tlr(tl_path, tc_def(TC_FRIENDS_USER), TL_FRIENDS, count, NULL, twitter_friends_timeline_success_handler, NULL);
	
	twitter_fetch_new_messages(ta, tlr);

	tl_path = purple_account_get_string(ta->account, tc_name(TC_REPLIES_TIMELINE), tc_def(TC_REPLIES_TIMELINE));
	tlr = twitter_new_tlr(tl_path, tc_def(TC_REPLIES_USER), TL_REPLIES, 1, NULL,
			twitter_replies_timeline_success_handler_init, NULL);
	purple_debug_info(DBGID, "fetching replies from %s to %s\n", tlr->path, tlr->name);
	twitter_fetch_new_messages(ta, tlr);
}

// Function to fetch all new messages periodically
gboolean twitter_fetch_all_new_messages(gpointer data)
{
	TwitterAccount * ta = data;
	TwitterTimeLineReq * tlr = NULL;
	const gchar * tl_path;
	
	tl_path = purple_account_get_string(ta->account, tc_name(TC_FRIENDS_TIMELINE), tc_def(TC_FRIENDS_TIMELINE));
	tlr = twitter_new_tlr(tl_path, tc_def(TC_FRIENDS_USER), TL_FRIENDS, TW_STATUS_COUNT_MAX, NULL,
			twitter_friends_timeline_success_handler, NULL);
	twitter_fetch_new_messages(ta, tlr);

	return TRUE;
}

#if 0
static void twitter_list_sent_id_hash(gpointer key, gpointer value, gpointer user_data)
{
	purple_debug_info(DBGID, "key/value = %s/%s\n", key, value);
}
#endif



gint twitter_fetch_new_messages_handler(MbConnData * conn_data, gpointer data)
{
	//TODO: write a wrapper around all this too
	MbAccount * ta = conn_data->ta;
	const gchar * username;
	MbHttpData * response = conn_data->response;
	TwitterTimeLineReq * tlr = data;
	time_t last_msg_time_t = 0;
	GList * msg_list = NULL;
	
	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	purple_debug_info(DBGID, "received result from %s\n", tlr->path);
	
	username = (const gchar *)purple_account_get_username(ta->account);
	
	if(response->status == HTTP_MOVED_TEMPORARILY) {
		// no new messages
		//TODO: error cb here? - Neaveru
		twitter_free_tlr(tlr);
		purple_debug_info(DBGID, "no new messages\n");
		return 0;
	}
	if(response->status != HTTP_OK) {
		//TODO: error cb here - Neaveru
		twitter_free_tlr(tlr);
		purple_debug_info(DBGID, "something's wrong with the message\n");
		return 0; //< should we return -1 instead?
	}
	if(response->content_len == 0) {
		//TODO: error cb here? - Neaveru
		purple_debug_info(DBGID, "no data to parse\n");
		twitter_free_tlr(tlr);
		return 0;
	}
	purple_debug_info(DBGID, "http_data = #%s#\n", response->content->str);
	msg_list = twitter_decode_messages(response->content->str, &last_msg_time_t);
	/*
	if(msg_list == NULL) {
		twitter_free_tlr(tlr);
		return 0;
	}
	*/
	
	// reverse the list and append it
	// only if id > last_msg_id
	msg_list = g_list_reverse(msg_list);
	if (tlr->success_cb)
		tlr->success_cb(ta, tlr, msg_list, last_msg_time_t);

	twitter_msg_list_free(msg_list);
	if(tlr->sys_msg) {
		serv_got_im(ta->gc, tlr->name, tlr->sys_msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
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
	MbHttpData * request;
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
	if(tlr->count > 0) {
		purple_debug_info(DBGID, "tlr->count = %d\n", tlr->count);
		mb_http_data_add_param_int(request, "count", tlr->count);
	}
	if (tlr->timeline_id == TC_REPLIES_TIMELINE) { //this sucks
		if(tlr->use_since_id && (ta->last_replies_id > 0) ) {
			mb_http_data_add_param_int(request, "since_id", ta->last_replies_id);
		}
	} else {
		if(tlr->use_since_id && (ta->last_msg_id > 0) ) {
			mb_http_data_add_param_int(request, "since_id", ta->last_msg_id);
		}
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
	GSList *buddies;
	GSList *l;

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
		purple_blist_add_buddy(buddy, NULL, twitter_group, NULL);
	}
	purple_debug_info(DBGID, "setting protocol-specific buddy information to purplebuddy\n");
	if(buddy->proto_data == NULL) {
		tbuddy = twitter_new_buddy();
		buddy->proto_data = tbuddy;
		tbuddy->buddy = buddy;
		tbuddy->ta = ta;
		tbuddy->uid = TL_FRIENDS;
		tbuddy->name = g_strdup(tc_def(TC_FRIENDS_USER));
		tbuddy->type = TWITTER_BUDDY_TYPE_SYSTEM;
	}
	purple_prpl_got_user_status(ta->account, buddy->name, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE), NULL);

	buddies = purple_find_buddies(ta->account, NULL);
	for (l = buddies; l; l = l->next)
	{
		buddy = l->data;
		if (!buddy || buddy->proto_data)
			continue;
		tbuddy = twitter_new_buddy();
		buddy->proto_data = tbuddy;
		tbuddy->buddy = buddy;
		tbuddy->ta = ta;
		//tbuddy->uid = -1; //?
		//tbuddy->name = g_strdup(buddy->name); //?
		tbuddy->type = TWITTER_BUDDY_TYPE_NORMAL;
	}
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
		purple_debug_info(DBGID, "refresh interval = %d\n", interval);
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
	
	purple_debug_info(DBGID, "mb_account_new\n");
	ta = g_new(MbAccount, 1);
	ta->account = acct;
	ta->gc = acct->gc;
	ta->state = PURPLE_CONNECTING;
	ta->timeline_timer = -1;
	ta->replies_timeline_timer = -1;
	ta->last_msg_id = mbpurple_account_get_ull(acct, TW_ACCT_LAST_MSG_ID, 0);
	ta->last_replies_id = mbpurple_account_get_ull(acct, TW_ACCT_LAST_REPLIES_ID, 0);
	ta->last_msg_time = 0;
	ta->last_replies_time = 0;
	ta->conn_hash = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	ta->ssl_conn_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	ta->sent_id_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ta->tag = NULL;
	ta->tag_pos = MB_TAG_NONE;
	ta->reply_to_status_id = 0;
	acct->gc->proto_data = ta;
	return ta;
}

static void mb_close_connection(gpointer key, gpointer value, gpointer user_data)
{
	MbConnData *conn_data = value;
	
	purple_debug_info(DBGID, "closing each connection\n");
	if(conn_data) {
		/*
		purple_debug_info(DBGID, "is_https = %d\n", is_https);
		if(is_https) {
			PurpleSslConnection * ssl = NULL;
			
			ssl = conn_data->ssl_conn_data;
			if(ssl) {
				purple_debug_info(DBGID, "removing current ssl socket from eventloop\n");
				purple_input_remove(ssl->inpa);
			}
			purple_debug_info(DBGID, "closing SSL socket\n");
		} else {
			purple_debug_info(DBGID, "cancelling connection with handle = %p\n", conn_data);
			purple_proxy_connect_cancel_with_handle(conn_data);
			if(conn_data->conn_event_handle >= 0) {
				purple_debug_info(DBGID, "removing event handle\n");
				purple_input_remove(conn_data->conn_event_handle);
			}
		}
		purple_debug_info(DBGID, "free all data\n");
		*/
		purple_debug_info(DBGID, "we have %p -> %p\n", key, value);
		mb_conn_data_free(conn_data);
	}	
}

void mb_account_free(MbAccount * ta)
{	
	//purple_debug_info(DBGID, "mb_account_free\n");
	purple_debug_info(DBGID, "%s\n", __FUNCTION__);
	
	if(ta->tag) {
		g_free(ta->tag);
		ta->tag = NULL;
	}
	ta->tag_pos = MB_TAG_NONE;
	ta->state = PURPLE_DISCONNECTED;
	
	if(ta->timeline_timer != -1) {
		purple_debug_info(DBGID, "removing timer\n");
		purple_timeout_remove(ta->timeline_timer);
	}
	if (ta->replies_timeline_timer != -1)
	{
		purple_timeout_remove(ta->replies_timeline_timer);
	}

	// new SSL-base connection hash
	// Do I need to iterate over conn_hash now? since we use timer to free MbAccount
	// so basically this should never be called	
	if(ta->ssl_conn_hash) {
		purple_debug_info(DBGID, "closing all active connection\n");
		g_hash_table_foreach(ta->ssl_conn_hash, mb_close_connection, (gpointer)TRUE);
		purple_debug_info(DBGID, "destroying connection hash\n");
		g_hash_table_destroy(ta->ssl_conn_hash);
		ta->ssl_conn_hash = NULL;
	}
	
	if(ta->conn_hash) {
		purple_debug_info(DBGID, "closing all non-ssl active connection\n");
		g_hash_table_foreach(ta->conn_hash, mb_close_connection, (gpointer)FALSE);
		purple_debug_info(DBGID, "destroying non-SSL connection hash\n");
		g_hash_table_destroy(ta->conn_hash);
		ta->conn_hash = NULL;
	}
	
	if(ta->sent_id_hash) {
		purple_debug_info(DBGID, "destroying sent_id hash\n");
		g_hash_table_destroy(ta->sent_id_hash);
		ta->sent_id_hash = NULL;
	}
	
	ta->account = NULL;
	ta->gc = NULL;
	
	purple_debug_info(DBGID, "free up memory used for microblog account structure\n");
	g_free(ta);
}

/*
gboolean twitter_close_timer(gpointer data)
{
	MbAccount * ma = data;

	if( (g_hash_table_size(ma->ssl_conn_hash) > 0) || (g_hash_table_size(ma->conn_hash) > 0) ) {
		return TRUE;
	} else {
		mb_account_free(ma);
		return FALSE;
	}
}
*/

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

	// connect to twitgin here
	purple_debug_info(DBGID, "looking for twitgin\n");
	if(!twitgin_plugin) {
		twitgin_plugin = purple_plugins_find_with_id("gtktwitgin");
	}
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
	MbAccount * ta = conn_data->ta;
	MbHttpData * response = conn_data->response;
	gchar * id_str = NULL;
	xmlnode * top, *id_node;
	
	purple_debug_info(DBGID, "send_im_handler\n");
	
	if(response->status != 200) {
		purple_debug_info(DBGID, "http error\n");
		//purple_debug_info(DBGID, "http data = #%s#\n", response->content->str);
		return -1;
	}
	
	if(!purple_account_get_bool(ta->account, tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF))) {
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
	g_hash_table_insert(ta->sent_id_hash, id_str, id_str);
	
	//hash_table supposed to free this for use
	//g_free(id_str);
	xmlnode_free(top);
	return 0;
}

int twitter_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	TwitterAccount * ta = gc->proto_data;
	TwitterBuddy * tb = twitter_find_buddy(ta, who);
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

	if (!TWITTER_BUDDY_IS_SYSTEM(tb))
	{
		gchar * new_msg_txt = g_strdup_printf("@%s %s", who, tmp_msg_txt);
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

	if(ta->reply_to_status_id > 0) {
		purple_debug_info(DBGID, "setting in_reply_to_status_id = %llu\n", ta->reply_to_status_id);
		mb_http_data_add_param_ull(conn_data->request, "in_reply_to_status_id", ta->reply_to_status_id);
		ta->reply_to_status_id = 0;
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
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleStatus *status = purple_presence_get_active_status(presence);
	const char *message = status ? purple_status_get_attr_string(status, "message") : NULL;

	if (message && strlen(message) > 0)
		return g_strdup(g_markup_escape_text(message, -1));

	return NULL;
}

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

        conn_data = mb_conn_data_new(ta, twitter_host, twitter_port, NULL, use_https);
        mb_conn_data_set_error(conn_data, "Favourite message error", MB_ERROR_NOACTION);
        mb_conn_data_set_retry(conn_data, 0);

        request = conn_data->request;
        request->type = HTTP_POST;
        request->port = twitter_port;

	mb_http_data_set_host(request, twitter_host);
        mb_http_data_set_path(request, path);
        mb_http_data_set_fixed_headers(request, twitter_fixed_headers);
        mb_http_data_set_header(request, "Host", twitter_host);
        mb_http_data_set_basicauth(request, user_name, purple_account_get_password(ta->account));

        //conn_data->handler_data = tlr;

        mb_conn_process_request(conn_data);
        g_free(twitter_host);
        g_free(user_name);
	g_free(path);

}
