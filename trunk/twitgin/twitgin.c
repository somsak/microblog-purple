/*
 * Twitgin - A GUI support of libtwitter/microblog-purple for Conversation dialog
 * Copyright (C) 2008 Chanwit Kaewkasi <chanwit@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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
#include "mb_util.h"
#include "twitpref.h"

#define DBGID "twitgin"
#define PURPLE_MESSAGE_TWITGIN 0x1000

// Dummy tw_conf to resolve external symbol link
TwitterConfig * _tw_conf = NULL;

static PurplePlugin * twitgin_plugin = NULL;

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

static gboolean twittgin_uri_handler(const char *proto, const char *cmd, GHashTable *params) 
{
	char *acct_id = g_hash_table_lookup(params, "account");	
	PurpleAccount *acct = NULL;
	PurpleConversation * conv = NULL;
	PidginConversation * gtkconv;
	int proto_id = 0;
	gchar * src = NULL;
	const char * decoded_rt;
	char * unescaped_rt;

	purple_debug_info(DBGID, "twittgin_uri_handler\n");	
	// do not need to test, because the conversation window must be open before one can click
	if (g_ascii_strcasecmp(proto, "tw") == 0) {
		proto_id = TWITTER_PROTO;
		acct = purple_accounts_find(acct_id, "prpl-mbpurple-twitter"); 
	} else if(g_ascii_strcasecmp(proto, "idc") == 0) {
		proto_id = IDENTICA_PROTO;
		acct = purple_accounts_find(acct_id, "prpl-mbpurple-identica"); 
	}
	src = g_hash_table_lookup(params, "src");
	if(!src) {
		purple_debug_info(DBGID, "no src specified\n");
		switch(proto_id) {
			case TWITTER_PROTO :
				src = "twitter.com";
				break;
			case IDENTICA_PROTO :
				src = "identi.ca";
				break;
		}
	}
	purple_debug_info(DBGID, "src = %s\n", src);

	if ( acct && (proto_id > 0) ) {
		purple_debug_info(DBGID, "found account with libtwitter, proto_id = %d\n", proto_id);

		/* tw:rep?to=sender */
		if (!g_ascii_strcasecmp(cmd, "reply")) {
			gchar * sender, *tmp;
			gchar * name_to_reply;
			unsigned long long msg_id = 0;

			conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, src, acct);
			purple_debug_info(DBGID, "conv = %p\n", conv);
			gtkconv = PIDGIN_CONVERSATION(conv);
			sender = g_hash_table_lookup(params, "to");		
			tmp = g_hash_table_lookup(params, "id");
			if(tmp) {
				msg_id = strtoull(tmp, NULL, 10);
			}
			purple_debug_info(DBGID, "sender = %s, id = %llu\n", sender, msg_id);
			if(msg_id > 0) {
				name_to_reply = g_strdup_printf("@%s ", sender);
				gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, name_to_reply, -1);
				gtk_widget_grab_focus(GTK_WIDGET(gtkconv->entry));
				g_free(name_to_reply);
				purple_signal_emit(twitgin_plugin, "twitgin-replying-message", proto, msg_id);
			}
			return TRUE;
		}

		// retweet hack !
		if (!g_ascii_strcasecmp(cmd, "rt")) {
			gchar * message, * from, * retweet_message;

			conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, src, acct);
			purple_debug_info(DBGID, "conv = %p\n", conv);
			gtkconv = PIDGIN_CONVERSATION(conv);
			message = g_hash_table_lookup(params, "msg");
			from = g_hash_table_lookup(params, "from");
			decoded_rt = purple_url_decode(message);
			unescaped_rt = purple_unescape_html(decoded_rt);
			retweet_message = g_strdup_printf("rt @%s: %s", from, unescaped_rt);
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, retweet_message, -1);
			gtk_widget_grab_focus(GTK_WIDGET(gtkconv->entry));
			g_free(unescaped_rt);
			g_free(retweet_message);
			return TRUE;
		}

		// favorite hack !
		if (!g_ascii_strcasecmp(cmd, "fav")) {
			MbAccount * ta;
			gchar * msg_id;

			conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, src, acct);
			msg_id = g_hash_table_lookup(params, "id");
			ta = mb_account_new(acct);
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
	if ((!is_twitter_conversation(conv)) || (flags & PURPLE_MESSAGE_SYSTEM) ) {
		return FALSE;
	}

	if (!(flags & PURPLE_MESSAGE_TWITGIN)) {		// Twitter msg not from twitgin -> Do not show
		if (flags & PURPLE_MESSAGE_SEND) {
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
			retval = twitter_reformat_msg(ma, &twitter_msg, NULL, FALSE); //< do not reply to myself
			purple_debug_info(DBGID, "new data = %s\n", retval);
			g_free(*msg);
			(*msg) = retval;
			return FALSE;
			/*
		} else if( (flags & PURPLE_MESSAGE_SYSTEM) || (flags & PURPLE_MESSAGE_NO_LOG) ||
				(flags & PURPLE_MESSAGE_ERROR))
		{
			return FALSE;
			*/
		} else if(flags == PURPLE_MESSAGE_RECV) {
			// discard only receiving message
			// Pidgin will free all the message in receiving ends
			purple_debug_info(DBGID, "flags = %x, received %s\n", flags, (*msg));
			return TRUE;
		}
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
void twitgin_on_tweet(MbAccount * ta, gchar * name, TwitterMsg * cur_msg) {

	PurpleConversation * conv;
	gboolean reply_link = purple_prefs_get_bool(TW_PREF_REPLY_LINK);
	gchar * fmt_txt = NULL, * tmp = NULL;
	gchar * displaying_txt = NULL;
	gchar * linkify_txt = NULL;
	gchar * fav_txt = NULL, * rt_txt = NULL, * datetime_txt = NULL;
	const char * embed_rt_txt = NULL;
	const gchar * account = (const gchar *)purple_account_get_username(ta->account);
	const char * uri_txt = mb_get_uri_txt(ta->account);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, name, ta->account);
	if (conv == NULL) {
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, ta->account, name);
	}
	purple_debug_info(DBGID, "raw text msg = ##%s##\n", cur_msg->msg_txt);

	// relink message text g_markup_escape_text will not translate " " to &nbsp;
	tmp = g_markup_escape_text(cur_msg->msg_txt, strlen(cur_msg->msg_txt));
	g_free(cur_msg->msg_txt);
	cur_msg->msg_txt = tmp;

	fmt_txt = twitter_reformat_msg(ta, cur_msg, name, reply_link);
	purple_debug_info(DBGID, "fmted text msg = ##%s##\n", fmt_txt);

	// need to manually linkify text since we are going to send RAW message
	linkify_txt = purple_markup_linkify(fmt_txt);

	if(uri_txt) {
		// display favorite link, if enabled
		if(purple_prefs_get_bool(TW_PREF_FAV_LINK)) {
			fav_txt = g_strdup_printf(" <a href=\"%s:fav?src=%s&account=%s&id=%llu\">*</a>", uri_txt, name, account, cur_msg->id);
		}

		// display rt link, if enabled
		if(purple_prefs_get_bool(TW_PREF_RT_LINK)) {
			// text for retweet url
			embed_rt_txt = purple_url_encode(cur_msg->msg_txt);
			purple_debug_info(DBGID, "url embed text for retweet = ##%s##\n", embed_rt_txt);

			rt_txt = g_strdup_printf(" <a href=\"%s:rt?src=%s&account=%s&from=%s&msg=%s\">rt<a>", uri_txt, name, account, cur_msg->from, embed_rt_txt);
		}

		//display link to message status in the timestamp
		if(purple_prefs_get_bool(TW_PREF_MS_LINK)) {
			
			datetime_txt = g_strdup_printf("<FONT COLOR=\"#cc0000\"><a href=\"http://twitter.com/%s/status/%llu\">%s</a></FONT>", cur_msg->from, cur_msg->id, format_datetime(conv, cur_msg->msg_time)); 
		}
		else {
			datetime_txt = g_strdup_printf("<FONT COLOR=\"#cc0000\">%s</FONT>", format_datetime(conv, cur_msg->msg_time)); 
		}
		
		displaying_txt = g_strdup_printf("%s %s%s%s", datetime_txt, linkify_txt, 
			fav_txt ? fav_txt : "", rt_txt ? rt_txt : "");

		if(fav_txt) g_free(fav_txt);
		if(rt_txt) g_free(rt_txt);
		if(datetime_txt) g_free(datetime_txt);
	} else {
		displaying_txt = g_strdup_printf("%s %s", format_datetime(conv, cur_msg->msg_time), linkify_txt);

	}
	purple_debug_info(DBGID, "displaying text = ##%s##\n", displaying_txt);
	purple_conv_im_write(PURPLE_CONV_IM(conv),
		cur_msg->from, displaying_txt, 
		PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_TWITGIN | PURPLE_MESSAGE_RAW | PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_NICK,
		cur_msg->msg_time);

	g_free(displaying_txt);
	g_free(linkify_txt);
	g_free(fmt_txt);
}

static gboolean plugin_load(PurplePlugin *plugin) 
{
		
	GList *convs = purple_get_conversations();
	void *gtk_conv_handle = pidgin_conversations_get_handle();
	PurplePlugin * prpl_plugin;
	GList * plugins;
	
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

	// twitter
	// handle all mbpurple plug-in
	plugins = purple_plugins_get_all();
	for(; plugins != NULL; plugins = plugins->next) {
		prpl_plugin = plugins->data;
		if( (prpl_plugin->info->id != NULL) && (strncmp(prpl_plugin->info->id, "prpl-mbpurple", 13) == 0)) {
			purple_debug_info(DBGID, "found plug-in %s\n", prpl_plugin->info->id);
			purple_signal_connect(prpl_plugin, "twitter-message", plugin, PURPLE_CALLBACK(twitgin_on_tweet), NULL);
		}
	}

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
	purple_signal_disconnect(pidgin_conversations_get_handle(), "twitgin-message", plugin, PURPLE_CALLBACK(twitgin_on_tweet));

	purple_debug_info(DBGID, "plugin unloaded\n");	
	return TRUE;
}

static PurplePluginPrefFrame * get_plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_name_and_label(TW_PREF_REPLY_LINK, _("Enable reply link"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(TW_PREF_FAV_LINK, _("Enable favorite link"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(TW_PREF_RT_LINK, _("Enable retweet link"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(TW_PREF_MS_LINK, _("Enable message status link"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(TW_PREF_AVATAR_SIZE, _("Avatar size"));
	purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(ppref, "Disabled", GINT_TO_POINTER(0));
	purple_plugin_pref_add_choice(ppref, "8x8", GINT_TO_POINTER(8));
	purple_plugin_pref_add_choice(ppref, "12x12", GINT_TO_POINTER(12));
	purple_plugin_pref_add_choice(ppref, "16x16", GINT_TO_POINTER(16));
	purple_plugin_pref_add_choice(ppref, "24x24", GINT_TO_POINTER(24));
	purple_plugin_pref_add_choice(ppref, "32x32", GINT_TO_POINTER(32));
	purple_plugin_pref_add_choice(ppref, "48x48", GINT_TO_POINTER(48));
	purple_plugin_pref_frame_add(frame, ppref);

	/*
	ppref = purple_plugin_pref_new_with_label("integer");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/int",
									"integer pref");
	purple_plugin_pref_set_bounds(ppref, 0, 255);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/int_choice",
									"integer choice");
	purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(ppref, "One", GINT_TO_POINTER(1));
	purple_plugin_pref_add_choice(ppref, "Two", GINT_TO_POINTER(2));
	purple_plugin_pref_add_choice(ppref, "Four", GINT_TO_POINTER(4));
	purple_plugin_pref_add_choice(ppref, "Eight", GINT_TO_POINTER(8));
	purple_plugin_pref_add_choice(ppref, "Sixteen", GINT_TO_POINTER(16));
	purple_plugin_pref_add_choice(ppref, "Thirty Two", GINT_TO_POINTER(32));
	purple_plugin_pref_add_choice(ppref, "Sixty Four", GINT_TO_POINTER(64));
	purple_plugin_pref_add_choice(ppref, "One Hundred Twenty Eight", GINT_TO_POINTER(128));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_label("string");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
								"/plugins/core/pluginpref_example/string",
								"string pref");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
								"/plugins/core/pluginpref_example/masked_string",
								"masked string");
	purple_plugin_pref_set_masked(ppref, TRUE);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
							"/plugins/core/pluginpref_example/max_string",
							"string pref\n(max length of 16)");
	purple_plugin_pref_set_max_length(ppref, 16);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
							"/plugins/core/pluginpref_example/string_choice",
							"string choice");
	purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(ppref, "red", "red");
	purple_plugin_pref_add_choice(ppref, "orange", "orange");
	purple_plugin_pref_add_choice(ppref, "yellow", "yellow");
	purple_plugin_pref_add_choice(ppref, "green", "green");
	purple_plugin_pref_add_choice(ppref, "blue", "blue");
	purple_plugin_pref_add_choice(ppref, "purple", "purple");
	purple_plugin_pref_frame_add(frame, ppref);
	*/

	return frame;
}

void plugin_destroy(PurplePlugin * plugin)
{
	purple_debug_info("twitgin", "plugin_destroy\n");
	purple_signal_unregister(plugin, "twitgin-replying-message");
}

static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,   /* page_num (Reserved) */
	NULL, /* frame (Reserved) */
	/* Padding */
	NULL,
	NULL,
	NULL,
	NULL
};

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
	"Twitgin",                                      /**< name */
	MBPURPLE_VERSION,                               /**< version */
	"Twitter Conversation.",                        /**< summary */
	"Support Microblog-purple "
	   "in the conversation window.",               /**< description */
	"Chanwit Kaewkasi <chanwit@gmail.com>",         /**< author */
	"http://microblog-purple.googlecode.com",       /**< homepage */
	plugin_load,                                    /**< load */
	plugin_unload,                                  /**< unload */
	plugin_destroy,                                 /**< destroy */
	NULL,                                           /**< ui_info */
	NULL,                                           /**< extra_info */
	&prefs_info,                                    /**< prefs_info */
	NULL,                                           /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void plugin_init(PurplePlugin *plugin) {	

	// default plug-in preference
	purple_prefs_add_none(TW_PREF_PREFIX);
	purple_prefs_add_bool(TW_PREF_REPLY_LINK, TRUE);
	purple_prefs_add_bool(TW_PREF_FAV_LINK, TRUE);
	purple_prefs_add_bool(TW_PREF_RT_LINK, TRUE);
	purple_prefs_add_bool(TW_PREF_MS_LINK, TRUE);
	purple_prefs_add_int(TW_PREF_AVATAR_SIZE, 24);

	purple_signal_register(plugin, "twitgin-replying-message",
			purple_marshal_POINTER__POINTER_INT64,
			purple_value_new(PURPLE_TYPE_POINTER), 2, 
			purple_value_new(PURPLE_TYPE_POINTER), // protocol name (tw or idc)
			purple_value_new(PURPLE_TYPE_INT64) // status ID
	);
	twitgin_plugin = plugin;
}

PURPLE_INIT_PLUGIN(twitgin, plugin_init, info)
