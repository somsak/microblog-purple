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
#include "tw_cmd.h"

#ifdef _WIN32
#	include <win32dep.h>
#else
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#endif

#include "twitter.h"

TwitterConfig * _tw_conf = NULL;

static TwCmd * tw_cmd = NULL;

static void plugin_init(PurplePlugin *plugin)
{
}

gboolean plugin_load(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	PurplePluginInfo *info = plugin->info;
	PurplePluginProtocolInfo *prpl_info = info->extra_info;
	
	purple_debug_info("twitterim", "plugin_load\n");
	_tw_conf = (TwitterConfig *)g_malloc0(TC_MAX * sizeof(TwitterConfig));

	_tw_conf[TC_HIDE_SELF].conf = g_strdup("twitter_hide_myself");
	_tw_conf[TC_HIDE_SELF].def_bool = TRUE;
	option = purple_account_option_bool_new(_("Hide myself in conversation"), tc_name(TC_HIDE_SELF), tc_def_bool(TC_HIDE_SELF));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_REPLY_LINK].conf = g_strdup("twitter_hide_myself");
	_tw_conf[TC_REPLY_LINK].def_bool = TRUE;
	option = purple_account_option_bool_new(_("Enable reply link"), tc_name(TC_REPLY_LINK), tc_def_bool(TC_REPLY_LINK));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_MSG_REFRESH_RATE].conf = g_strdup("twitter_msg_refresh_rate");
	_tw_conf[TC_MSG_REFRESH_RATE].def_int = 60;
	option = purple_account_option_int_new(_("Message refresh rate (seconds)"), tc_name(TC_MSG_REFRESH_RATE), tc_def_int(TC_MSG_REFRESH_RATE));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_INITIAL_TWEET].conf = g_strdup("twitter_init_tweet");
	_tw_conf[TC_INITIAL_TWEET].def_int = 15;
	option = purple_account_option_int_new(_("Number of initial tweets"), tc_name(TC_INITIAL_TWEET), tc_def_int(TC_INITIAL_TWEET));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

	_tw_conf[TC_GLOBAL_RETRY].conf = g_strdup("twitter_global_retry");
	_tw_conf[TC_GLOBAL_RETRY].def_int = 3 ;
	option = purple_account_option_int_new(_("Maximum number of retry"), tc_name(TC_GLOBAL_RETRY), tc_def_int(TC_GLOBAL_RETRY));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

	_tw_conf[TC_HOST].conf = g_strdup("twitter_hostname");
	_tw_conf[TC_HOST].def_str = g_strdup("twitter.com");
	option = purple_account_option_string_new(_("Hostname"), tc_name(TC_HOST), tc_def(TC_HOST));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_USE_HTTPS].conf = g_strdup("twitter_use_https");
	_tw_conf[TC_USE_HTTPS].def_bool = TRUE;
	option = purple_account_option_bool_new(_("Use HTTPS"), tc_name(TC_USE_HTTPS), tc_def_bool(TC_USE_HTTPS));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_STATUS_UPDATE].conf = g_strdup("twitter_status_update");
	_tw_conf[TC_STATUS_UPDATE].def_str = g_strdup("/statuses/update.xml");
	option = purple_account_option_string_new(_("Status update path"), tc_name(TC_STATUS_UPDATE), tc_def(TC_STATUS_UPDATE));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_VERIFY_PATH].conf = g_strdup("twitter_verify");
	_tw_conf[TC_VERIFY_PATH].def_str = g_strdup("/account/verify_credentials.xml");
	option = purple_account_option_string_new(_("Account verification path"), tc_name(TC_VERIFY_PATH), tc_def(TC_VERIFY_PATH));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_FRIENDS_TIMELINE].conf = g_strdup("twitter_friends_timeline");
	_tw_conf[TC_FRIENDS_TIMELINE].def_str = g_strdup("/statuses/friends_timeline.xml");
	option = purple_account_option_string_new(_("Friends timeline path"), tc_name(TC_FRIENDS_TIMELINE), tc_def(TC_FRIENDS_TIMELINE));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_USER_TIMELINE].conf = g_strdup("twitter_user_timeline");
	_tw_conf[TC_USER_TIMELINE].def_str = g_strdup("/statuses/user_timeline.xml");
	option = purple_account_option_string_new(_("User timeline path"), tc_name(TC_USER_TIMELINE), tc_def(TC_USER_TIMELINE));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);
	
	_tw_conf[TC_PUBLIC_TIMELINE].conf = g_strdup("twitter_public_timeline");
	_tw_conf[TC_PUBLIC_TIMELINE].def_str = g_strdup("/statuses/public_timeline.xml");
	option = purple_account_option_string_new(_("Public timeline path"), tc_name(TC_PUBLIC_TIMELINE), tc_def(TC_PUBLIC_TIMELINE));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

	_tw_conf[TC_REPLIES_TIMELINE].conf = g_strdup("twitter_replies_timeline");
	_tw_conf[TC_REPLIES_TIMELINE].def_str = g_strdup("/statuses/replies.xml");
	option = purple_account_option_string_new(_("Replies timeline path"), tc_name(TC_REPLIES_TIMELINE), tc_def(TC_REPLIES_TIMELINE));
	prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

	// and now for non-option global
	_tw_conf[TC_FRIENDS_USER].def_str = g_strdup("twitter.com");
	_tw_conf[TC_REPLIES_USER].def_str = g_strdup("twitter.com");
	_tw_conf[TC_PUBLIC_USER].def_str = g_strdup("twpublic");
	_tw_conf[TC_USER_USER].def_str = g_strdup("twuser");
	_tw_conf[TC_USER_GROUP].def_str = g_strdup("Twitter");

	// command support
	tw_cmd = tw_cmd_init(info->id);

	return TRUE;
}

gboolean plugin_unload(PurplePlugin *plugin)
{
	gint i;

	purple_debug_info("twitterim", "plugin_unload\n");

	tw_cmd_finalize(tw_cmd);
	tw_cmd = NULL;
	g_free(_tw_conf[TC_HOST].def_str);
	g_free(_tw_conf[TC_STATUS_UPDATE].def_str);
	g_free(_tw_conf[TC_VERIFY_PATH].def_str);
	g_free(_tw_conf[TC_FRIENDS_TIMELINE].def_str);
	g_free(_tw_conf[TC_USER_TIMELINE].def_str);
	g_free(_tw_conf[TC_PUBLIC_TIMELINE].def_str);
	g_free(_tw_conf[TC_FRIENDS_USER].def_str);
	g_free(_tw_conf[TC_PUBLIC_USER].def_str);
	g_free(_tw_conf[TC_USER_USER].def_str);
	for(i = 0; i < TC_MAX; i++) {
		if(_tw_conf[i].conf) g_free(_tw_conf[i].conf);
	}

	g_free(_tw_conf);
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
