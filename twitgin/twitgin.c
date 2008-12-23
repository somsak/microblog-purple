/*
 * Twitgin - A GUI support of libtwitter/microblog-purple for Conversation dialog
 * Copyright (C) 2008 Chanwit Kaewkasi <chanwit@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <glib/gi18n.h>

#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

//#include "internal.h"
//#include <pidgin.h>
#include <account.h>
#include <core.h>
#include <debug.h>
#include <gtkconv.h>
#include <util.h>
#include <version.h>
#include <gtkplugin.h>
#include <gtkimhtml.h>
#include <gtkutils.h>
#include <gtknotify.h>

#define TW_MAX_MESSAGE_SIZE 140
#define TW_MAX_MESSAGE_SIZE_TEXT "140"

#include "twitter.h"
#include "mb_http.h"
#include "mb_net.h"

#define DBGID "twitgin"
#define PURPLE_MESSAGE_TWITGIN 0x1000

//static gint twitter_port = 443;
static const char twitter_fixed_headers[] = "User-Agent:" TW_AGENT "\r\n" \
"Accept: */*\r\n" \
"X-Twitter-Client: " TW_AGENT_SOURCE "\r\n" \
"X-Twitter-Client-Version: 0.1\r\n" \
"X-Twitter-Client-Url: " TW_AGENT_DESC_URL "\r\n" \
"Connection: Close\r\n" \
"Pragma: no-cache\r\n";

// Dummy tw_conf to resolve external symbol link
TwitterConfig * _tw_conf = NULL;

// static void url_clicked_cb(GtkWidget *w, const char *uri);

static void twitgin_entry_buffer_on_changed(PidginConversation *gtkconv) {
	GtkTextIter start;
	GtkTextIter end;
	gchar *text = NULL;
	int size = 0;
	gchar* size_text = NULL;

	GtkWidget *size_label = g_object_get_data(G_OBJECT(gtkconv->toolbar), "size_label");
	if(size_label != NULL) {
		gtk_text_buffer_get_iter_at_offset(gtkconv->entry_buffer, &start, 0);
		gtk_text_buffer_get_iter_at_offset(gtkconv->entry_buffer, &end, 0);
		gtk_text_iter_forward_to_end(&end);
		text = gtk_text_buffer_get_text(gtkconv->entry_buffer, &start, &end, FALSE);	
		size = TW_MAX_MESSAGE_SIZE - g_utf8_strlen(text, -1);
		if(size >= 0) {
			size_text = g_strdup_printf("%d", size);
		} else {
			size_text = g_strdup_printf("<span foreground=\"red\">%d</span>", size);	
		}
		gtk_label_set_markup(GTK_LABEL(size_label), size_text);
		g_free(size_text);
	}
}

/* Editable stuff */
//static void twitgin_preinsert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *text, gint len, GtkIMHtml *imhtml) {
	// TODO: 
	// if(strcmp(text,"tw:")==0) {
	//	g_signal_stop_emission_by_name(buffer, "insert-text");	
	// }	
//}

static void create_twitter_label(PidginConversation *gtkconv) {
	GtkWidget *label = gtk_label_new(TW_MAX_MESSAGE_SIZE_TEXT);
	// int id;	
	gtk_box_pack_end(GTK_BOX(gtkconv->toolbar), label, FALSE, FALSE, 0);	
	gtk_widget_show(label);
	g_object_set_data(G_OBJECT(gtkconv->toolbar), "size_label", label);		
	g_signal_connect_swapped(G_OBJECT(gtkconv->entry_buffer), "changed", G_CALLBACK(twitgin_entry_buffer_on_changed), gtkconv);	
	// g_signal_connect(G_OBJECT(GTK_IMHTML(gtkconv->imhtml)->text_buffer), "insert-text", G_CALLBACK(twitgin_preinsert_cb), gtkconv->imhtml);
}

static void remove_twitter_label(PidginConversation *gtkconv) {
	GtkWidget *size_label = NULL;

	size_label = g_object_get_data(G_OBJECT(gtkconv->toolbar),"size_label");
	if (size_label != NULL) {
		gtk_widget_destroy(size_label);
	}
}

static gboolean is_twitter_conversation(PurpleConversation *conv) {
	purple_debug_info(DBGID, "%s %s\n", __FUNCTION__, conv->account->protocol_id);
	if(conv->account && conv->account->protocol_id) {
		return (strncmp(conv->account->protocol_id, "prpl-mbpurple", 13) == 0);
	} else {
		return FALSE;
	}
}

static void on_conversation_display(PidginConversation *gtkconv)
{
	GtkWidget *size_label = NULL;
	PurpleConversation *conv = gtkconv->active_conv;
	if(is_twitter_conversation(conv)) {
		size_label = g_object_get_data(G_OBJECT(gtkconv->toolbar), "size_label");
		if (size_label == NULL) {
			create_twitter_label(gtkconv);
		}
	}
}

enum {
	TWITTER_PROTO = 1,
	IDENTICA_PROTO = 2,
};

MbAccount * mb_account_new(PurpleAccount * acct)
{
        MbAccount * ta = NULL;

        purple_debug_info(DBGID, "mb_account_new\n");
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
        ta->tag = NULL;
        ta->tag_pos = MB_TAG_NONE;
        acct->gc->proto_data = ta;
        return ta;
}

/*
*  Favourite Handler
*/
void twitter_favorite_message(MbAccount * ta, gchar * msg_id){

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

static gboolean twittgin_uri_handler(const char *proto, const char *cmd, GHashTable *params) 
{
	char *acct_id = g_hash_table_lookup(params, "account");	
	PurpleAccount *acct;	
	PurpleConversation * conv = NULL;
	PidginConversation * gtkconv;
	int proto_id = 0;

	purple_debug_info(DBGID, "twittgin_uri_handler\n");	
	// do not need to test, because the conversation window must be open before one can click
	if (g_ascii_strcasecmp(proto, "tw") == 0) {
		proto_id = TWITTER_PROTO;
		acct = purple_accounts_find(acct_id, "prpl-mbpurple-twitter"); 
	} else if(g_ascii_strcasecmp(proto, "idc") == 0) {
		proto_id = IDENTICA_PROTO;
		acct = purple_accounts_find(acct_id, "prpl-mbpurple-identica"); 
	}
	if ( proto_id > 0 ) {
		purple_debug_info(DBGID, "found account with libtwitter, proto_id = %d\n", proto_id);
		/* tw:rep?to=sender */
		if (!g_ascii_strcasecmp(cmd, "reply")) {
			switch(proto_id) {
				case TWITTER_PROTO :
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, "twitter.com", acct);		
					break;
				case IDENTICA_PROTO :
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, "identi.ca", acct);		
					break;
			}
			purple_debug_info(DBGID, "conv = %p\n", conv);
			gtkconv = PIDGIN_CONVERSATION(conv);
			gchar *sender = g_hash_table_lookup(params, "to");		
			gchar *name_to_reply = g_strdup_printf("@%s ", sender);
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, name_to_reply, -1);
			gtk_widget_grab_focus(GTK_WIDGET(gtkconv->entry));
			g_free(name_to_reply);
			return TRUE;
		}
		// retweet hack !
		if (!g_ascii_strcasecmp(cmd, "rt")) {
			switch(proto_id) {
				case TWITTER_PROTO :
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, "twitter.com", acct);
					break;
				case IDENTICA_PROTO :
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, "identi.ca", acct);		
					break;
			}
			purple_debug_info(DBGID, "conv = %p\n", conv);
			gtkconv = PIDGIN_CONVERSATION(conv);
	                gchar *message = g_hash_table_lookup(params, "msg");
	                gchar *from = g_hash_table_lookup(params, "from");
        	        gchar *retweet_message = g_strdup_printf("rt @%s: %s", from, g_uri_unescape_string (message,NULL));
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, retweet_message, -1);
                	gtk_widget_grab_focus(GTK_WIDGET(gtkconv->entry));
                	g_free(retweet_message);
                	return TRUE;
		}
		// favorite hack !
		if (!g_ascii_strcasecmp(cmd, "fav")) {
			switch(proto_id) {
				case TWITTER_PROTO :
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, "twitter.com", acct);
					break;
				case IDENTICA_PROTO :
					conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, "identi.ca", acct);		
					break;
			}
	                gchar *msg_id = g_hash_table_lookup(params, "id");
			MbAccount *ta = mb_account_new(acct);
			twitter_favorite_message(ta, msg_id);
        		purple_conv_im_write(PURPLE_CONV_IM(conv), NULL, g_strdup_printf("message %s is favorited", msg_id), PURPLE_MESSAGE_SYSTEM, time(NULL));

			return TRUE;
		}
	} 
	return FALSE;
}

static PurpleNotifyUiOps twitgin_ops;
static void *(*saved_notify_uri)(const char *uri);

static void * twitgin_notify_uri(const char *uri) {
	void * retval = NULL;

	if( (strncmp(uri,"tw:",3)==0) || (strncmp(uri, "idc:", 4) == 0) ) {
		purple_debug_info(DBGID, "notify hooked: uri=%s\n", uri);	
		purple_got_protocol_handler_uri(uri);		
	} else {
		retval = saved_notify_uri(uri);
	}
	return retval;
}

/*
 * Modify sending message in the same way as receiving message
 * This should be done only for message generated by self
 */
gboolean twitgin_on_displaying(PurpleAccount * account, const char * who, char ** msg, PurpleConversation * conv, PurpleMessageFlags flags)
{
	MbAccount * ma = account->gc->proto_data;
	char * retval;
	TwitterMsg twitter_msg;

	// Do not edit msg from these
	if ((!is_twitter_conversation(conv)) || (flags & PURPLE_MESSAGE_SYSTEM) || (flags & PURPLE_MESSAGE_SEND)) {
		return FALSE;
	}

	if (!(flags & PURPLE_MESSAGE_TWITGIN)) {		// Twitter msg not from twitgin -> Do not show
		return TRUE;
	}

	// keep the original
	if(is_twitter_conversation(conv) && (flags & PURPLE_MESSAGE_SEND) ) {
		purple_debug_info(DBGID, "data being displayed = %s, from = %s, flags = %x\n", (*msg), who, flags);
		purple_debug_info(DBGID, "conv account = %s, name = %s, title = %s\n", purple_account_get_username(conv->account), conv->name, conv->title);
		purple_debug_info(DBGID, "data not from myself\n");
		twitter_msg.id = 0;
		twitter_msg.avatar_url = NULL;
		twitter_msg.from = NULL; //< force the plug-in not displaying own name
		twitter_msg.msg_txt = (*msg);
		twitter_msg.msg_time = 0;
		twitter_msg.flag = 0;
		twitter_msg.flag |= TW_MSGFLAG_DOTAG;
		purple_debug_info(DBGID, "going to modify message\n");
		retval = twitter_reformat_msg(ma, &twitter_msg, FALSE); //< do not to reply to myself
		purple_debug_info(DBGID, "new data = %s\n", retval);
		g_free(*msg);
		(*msg) = retval;
	}
	return FALSE;

}

/*
* Copied from pidgin original code to make the same time format
*/
gchar * format_datetime(PurpleConversation * conv, time_t mtime) {

	char * mdate = NULL;
	gboolean show_date;
	PidginConversation * gtkconv;

	gtkconv = PIDGIN_CONVERSATION(conv);

	if (gtkconv->newday == 0) {
		struct tm *tm = localtime(&mtime);

	        tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
        	tm->tm_mday++;

	        gtkconv->newday = mktime(tm);
	}

	show_date = (mtime >= gtkconv->newday) || (time(NULL) > mtime + 20*60);

	mdate = purple_signal_emit_return_1(pidgin_conversations_get_handle(),
                                          "conversation-timestamp",
                                          conv, mtime, show_date);
	if (mdate == NULL)
        {
                struct tm *tm = localtime(&mtime);
                const char *tmp;
                if (show_date)
                        tmp = purple_date_format_long(tm);
                else
                        tmp = purple_time_format(tm);
                mdate = g_strdup_printf("(%s)", tmp);
        }

	return mdate;
	
}

/*
* Hack the message display, redirect from normal process (on displaying event) and push them back
*/
void twitgin_on_display_message(MbAccount * ta, gchar * name, TwitterMsg * cur_msg, gboolean reply_link) {

	PurpleConversation * conv;
	gchar * fmt_txt = twitter_reformat_msg(ta, cur_msg, reply_link);	
	const gchar * account = (const gchar *)purple_account_get_username(ta->account);

        conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, name, ta->account);
        if (conv == NULL)
                conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, ta->account, name);

	fmt_txt = g_strdup_printf("<FONT COLOR=\"#cc0000\">%s</FONT> %s <a href=\"tw:fav?account=%s&id=%llu\">*</a> <a href=\"tw:rt?account=%s&from=%s&msg=%s\">rt<a>", 
		format_datetime(conv, cur_msg->msg_time), purple_markup_linkify(fmt_txt), 
		account, cur_msg->id,		
		account, cur_msg->from, g_uri_escape_string(cur_msg->msg_txt, NULL, TRUE));

        purple_conv_im_write(PURPLE_CONV_IM(conv), cur_msg->from, fmt_txt, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_TWITGIN | PURPLE_MESSAGE_RAW, cur_msg->msg_time);

	g_free(fmt_txt);
}


static gboolean plugin_load(PurplePlugin *plugin) 
{
		
	GList *convs = purple_get_conversations();
	void *gtk_conv_handle = pidgin_conversations_get_handle();
	
	purple_debug_info(DBGID, "plugin loaded\n");	
	purple_signal_connect(gtk_conv_handle, "conversation-displayed", plugin, PURPLE_CALLBACK(on_conversation_display), NULL);
	/*
	purple_signal_connect(gtk_conv_handle, "conversation-hiding", plugin,
	                      PURPLE_CALLBACK(conversation_hiding_cb), NULL);
	 */

	while (convs) {		
		PurpleConversation *conv = (PurpleConversation *)convs->data;
		/* Setup UI and Events */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv) && is_twitter_conversation(conv)) {
			create_twitter_label(PIDGIN_CONVERSATION(conv));
		}
		convs = convs->next;
	}
	
	memcpy(&twitgin_ops, purple_notify_get_ui_ops(), sizeof(PurpleNotifyUiOps));
	saved_notify_uri = twitgin_ops.notify_uri;
	twitgin_ops.notify_uri = twitgin_notify_uri;
	purple_notify_set_ui_ops(&twitgin_ops);
	purple_signal_connect(purple_get_core(), "uri-handler", plugin, PURPLE_CALLBACK(twittgin_uri_handler), NULL);

	purple_signal_connect(pidgin_conversations_get_handle(), "displaying-im-msg", plugin, PURPLE_CALLBACK(twitgin_on_displaying), NULL);

	purple_signal_connect(pidgin_conversations_get_handle(), "twitter-message", plugin, PURPLE_CALLBACK(twitgin_on_display_message), NULL);

	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	GList *convs = purple_get_conversations();
	
	purple_debug_info(DBGID, "plugin unloading\n");
	
	if(twitgin_notify_uri != purple_notify_get_ui_ops()->notify_uri) {
		purple_debug_info(DBGID, "ui ops changed, cannot unloading\n");
		return FALSE;
	}	
	
	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* Remove label */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv) && is_twitter_conversation(conv)) {
			remove_twitter_label(PIDGIN_CONVERSATION(conv));
		}
		convs = convs->next;
	}
	
	twitgin_ops.notify_uri = saved_notify_uri;
	purple_notify_set_ui_ops(&twitgin_ops);
	purple_signal_disconnect(purple_get_core(), "uri-handler", plugin, PURPLE_CALLBACK(twittgin_uri_handler));

	purple_signal_disconnect(purple_conversations_get_handle(), "displaying-im-msg", plugin, PURPLE_CALLBACK(twitgin_on_displaying));
	purple_signal_disconnect(pidgin_conversations_get_handle(), "twitgin-message", plugin, PURPLE_CALLBACK(twitgin_on_display_message));

	purple_debug_info(DBGID, "plugin unloaded\n");	
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,                           /**< major version */
	PURPLE_MINOR_VERSION,                           /**< minor version */
	PURPLE_PLUGIN_STANDARD,                         /**< type */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                              /**< flags */
	NULL,                                           /**< dependencies */
	PURPLE_PRIORITY_DEFAULT,                        /**< priority */

	"gtktwitgin",                                   /**< id */
	"Twitgin",                                  /**< name */
	MBPURPLE_VERSION,                                /**< version */
	"Twitter Conversation.",                    /**< summary */
	"Support Microblog-purple "
	   "in the conversation window.",              /**< description */
	"Chanwit Kaewkasi <chanwit@gmail.com>",         /**< author */
	"http://microblog-purple.googlecode.com",                                 /**< homepage */
	plugin_load,                                    /**< load */
	plugin_unload,                                  /**< unload */
	NULL,                                           /**< destroy */
	NULL,                                           /**< ui_info */
	NULL,                                           /**< extra_info */
	NULL,                                           /**< prefs_info */
	NULL,                                           /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void 
init_plugin(PurplePlugin *plugin) {	
}

PURPLE_INIT_PLUGIN(twitgin, init_plugin, info)
