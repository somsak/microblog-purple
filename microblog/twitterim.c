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

static void plugin_init(PurplePlugin *plugin)
{
}

gboolean plugin_load(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;
	
	purple_debug_info("twitterim", "plugin_load\n");

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
	
	option = purple_account_option_bool_new(_("Use HTTPS"), "twitter_use_https", TRUE);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_string_new(_("Twitter status update path"), "twitter_status_update", TW_STATUS_UPDATE_PATH);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_string_new(_("Twitter account verification path"), "twitter_verify", TW_VERIFY_PATH);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_string_new(_("Twitter Friends timeline path"),_TweetTimeLineConfigs[TL_FRIENDS], _TweetTimeLinePaths[TL_FRIENDS]);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_string_new(_("Twitter User timeline path"), _TweetTimeLineConfigs[TL_USER], _TweetTimeLinePaths[TL_USER]);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	option = purple_account_option_string_new(_("Twitter Public timeline path"), _TweetTimeLineConfigs[TL_PUBLIC],  _TweetTimeLinePaths[TL_PUBLIC]);
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	return TRUE;
}

gboolean plugin_unload(PurplePlugin *plugin)
{
	purple_debug_info("twitterim", "plugin_unload\n");
	return TRUE;
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



PurplePluginProtocolInfo twitter_prpl_info = {
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
	twitter_status_text, /* status_text */
//	twitterim_tooltip_text,/* tooltip_text */
	NULL,
	twitter_statuses,    /* status_types */
	NULL,                   /* blist_node_menu */
	NULL,                   /* chat_info */
	NULL,                   /* chat_info_defaults */
	twitter_login,       /* login */
	twitter_close,       /* close */
	twitter_send_im,     /* send_im */
	NULL,                   /* set_info */
//	twitterim_send_typing, /* send_typing */
	NULL,
//	twitterim_get_info,    /* get_info */
	NULL,
	twitter_set_status,/* set_status */
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
	twitter_buddy_free,  /* buddy_free */
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
	sizeof(PurplePluginProtocolInfo) /* struct_size */
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
	"prpl-mbpurple-twitter", /* id */
	"TwitterIM", /* name */
	MBPURPLE_VERSION, /* version */
	"Twitter data feeder", /* summary */
	"Twitter data feeder", /* description */
	"Somsak Sriprayoonsakul <somsaks@gmail.com>", /* author */
	"http://microblog-purple.googlecode.com/", /* homepage */
	plugin_load, /* load */
	plugin_unload, /* unload */
	NULL, /* destroy */
	NULL, /* ui_info */
	&twitter_prpl_info, /* extra_info */
	NULL, /* prefs_info */
	twitterim_actions, /* actions */
	NULL, /* padding */
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(twitterim, plugin_init, info);
