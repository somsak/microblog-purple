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

#include "internal.h"

#include "version.h"

#include "pidgin.h"
#include "account.h"
#include "core.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include <debug.h>
#include "gtknotify.h"
#include "util.h"

#define TW_MAX_MESSAGE_SIZE 140
#define TW_MAX_MESSAGE_SIZE_TEXT "140"

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
static void twitgin_preinsert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *text, gint len, GtkIMHtml *imhtml) {
	// TODO: 
	// if(strcmp(text,"tw:")==0) {
	//	g_signal_stop_emission_by_name(buffer, "insert-text");	
	// }	
}

static void create_twitter_label(PidginConversation *gtkconv) {
	GtkWidget *label = gtk_label_new(_(TW_MAX_MESSAGE_SIZE_TEXT));
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
	return strcmp(conv->account->protocol_id, "prpl-somsaks-twitter")==0;
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

static gboolean twittgin_uri_handler(const char *proto, const char *cmd, GHashTable *params) 
{
	char *acct_id = g_hash_table_lookup(params, "account");	
	PurpleAccount *acct;	
	purple_debug_info("twitgin", "twittgin_uri_handler\n");	
	// do not need to test, because the conversation window must be open before one can click
	acct = purple_accounts_find(acct_id, "prpl-somsaks-twitter"); 
	if (g_ascii_strcasecmp(proto, "tw")!=0) return FALSE;	
	purple_debug_info("twitgin", "found account with libtwtter\n");	
	/* tw:rep?to=sender */
	if (!g_ascii_strcasecmp(cmd, "reply")) {
		purple_debug_info("twitgin", "found reply command\n");	
		PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, "twitter.com", acct);		
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
		gchar *sender = g_hash_table_lookup(params, "to");		
		gchar *name_to_reply = g_strdup_printf("@%s ", sender);
		gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, name_to_reply, -1);
		gtk_widget_grab_focus(GTK_WIDGET(gtkconv->entry));
		g_free(name_to_reply);
		return TRUE;
	}
	return FALSE;
}

static PurpleNotifyUiOps twitgin_ops;
static void *(*saved_notify_uri)(const char *uri);

static void * twitgin_notify_uri(const char *uri) {
	if(strncmp(uri,"tw:",3)==0) {
		purple_debug_info("twitgin", "notify hooked: uri=%s\n", uri);	
		purple_got_protocol_handler_uri(uri);		
	} else {
		saved_notify_uri(uri);
	}
}

static gboolean plugin_load(PurplePlugin *plugin) 
{
		
	GList *convs = purple_get_conversations();
	void *gtk_conv_handle = pidgin_conversations_get_handle();
	
	purple_debug_info("twitgin", "plugin loaded\n");	
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

	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	GList *convs = purple_get_conversations();
	
	purple_debug_info("twitgin", "plugin unloading\n");
	
	if(twitgin_notify_uri != purple_notify_get_ui_ops()->notify_uri) {
		purple_debug_info("twitgin", "ui ops changed, cannot unloading\n");
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
	purple_debug_info("twitgin", "plugin unloaded\n");	
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
	N_("Twitgin"),                                  /**< name */
	DISPLAY_VERSION,                                /**< version */
	N_("Twitter Conversation."),                    /**< summary */
	N_("Support Microblog-purple "
	   "in the conversation window."),              /**< description */
	"Chanwit Kaewkasi <chanwit@gmail.com>",         /**< author */
	PURPLE_WEBSITE,                                 /**< homepage */
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
