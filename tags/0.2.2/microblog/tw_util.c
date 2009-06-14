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

#ifdef _WIN32
#	include <win32dep.h>
#else
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#endif

#include "mb_util.h"
#include "twitter.h"

#define DBGID "tw_util"

/*
const char * mb_get_uri_txt(PurpleAccount * pa)
{
	if (strcmp(pa->protocol_id, "prpl-mbpurple-twitter") == 0) {
		return "tw";
	} else if(strcmp(pa->protocol_id, "prpl-mbpurple-identica") == 0) {
		return "idc";
	}
	// no support for laconica for now
	return NULL;
}
*/

static void twitter_update_link(MbAccount * ta, GString * msg, char sym, const char * name)
{
	char * user_name;
	gboolean user_name_eq_name = FALSE;

	twitter_get_user_host(ta, &user_name, NULL);
	purple_debug_info(DBGID, "symbol = %c, name = %s, user_name = %s\n", sym, name, user_name);
	if(strcmp(name, user_name) == 0) {
		purple_debug_info(DBGID, "name and username is equal\n");
		user_name_eq_name = TRUE;
	}
	if(user_name_eq_name) g_string_append_printf(msg, "<i><b>");
	if(strcmp(ta->account->protocol_id, "prpl-mbpurple-twitter") == 0) {
		if(sym == '@') {
			g_string_append_printf(msg, "@<a href=\"http://twitter.com/%s\">%s</a>",name,name);
		} else if(sym == '#') {
			g_string_append_printf(msg, "#<a href=\"http://search.twitter.com/search?q=%%23%s\">%s</a>",name,name);
		}
	} else if(strcmp(ta->account->protocol_id, "prpl-mbpurple-identica") == 0) {
		if(sym == '@') {
			g_string_append_printf(msg, "@<a href=\"http://identi.ca/%s\">%s</a>",name,name);
		} else if(sym == '#') {
			g_string_append_printf(msg, "#<a href=\"http://identi.ca/tag/%s\">%s</a>",name,name);
		}
	} else {
		g_string_append_printf(msg, "%c%s", sym, name);
	}
	if(user_name_eq_name) g_string_append_printf(msg, "</b></i>");
	g_free(user_name);
}


void twitter_get_user_host(MbAccount * ta, char ** user_name, char ** host)
{
	char * at_sign = NULL;

	purple_debug_info(DBGID, "%s\n", __FUNCTION__);
	(*user_name) = g_strdup(purple_account_get_username(ta->account));
	purple_debug_info(DBGID, "username = ##%s##\n", (*user_name));
	if( (at_sign = strrchr(*user_name, '@')) == NULL) {
		if(host != NULL) {
			(*host) = g_strdup(purple_account_get_string(ta->account, tc_name(TC_HOST), tc_def(TC_HOST)));
			purple_debug_info(DBGID, "host (config) = %s\n", (*host));
		}
	} else {
		(*at_sign) = '\0';
		if(host != NULL) {
			(*host) = g_strdup( at_sign + 1 );
			purple_debug_info(DBGID, "host = %s\n", (*host));
		}
	}
}

/*
 * Reformat text message and makes it looks nicer
 *
 * @retval newly allocated buffer of string, need to be freed after used
 */
char * twitter_reformat_msg(MbAccount * ta, const TwitterMsg * msg, const char * conv_name, gboolean reply_link)
{
	gchar * username;
	GString * output;
	gchar * src = NULL;
	gchar * name, *name_color, * uri_txt = NULL;
	gchar sym, old_char, previous_char;
	int i = 0, j = 0;
	gboolean from_eq_username = FALSE;

	purple_debug_info(DBGID, "%s\n", __FUNCTION__);

	twitter_get_user_host(ta, &username, NULL);
	output = g_string_new("");

	// tag for the first thing
	purple_debug_info(DBGID, "checking for tag\n");
	if( (msg->flag & TW_MSGFLAG_DOTAG) && ta->tag ) {
		purple_debug_info(DBGID, "do the tagging of message, for the tag %s\n", ta->tag);
		if(ta->tag_pos == MB_TAG_PREFIX) {
			src = g_strdup_printf("%s %s", ta->tag, msg->msg_txt);
		} else {
			src = g_strdup_printf("%s %s", msg->msg_txt, ta->tag);
		}
	} else {
		purple_debug_info(DBGID, "not doing the tagging of message\n");
		src = g_strdup(msg->msg_txt);
	}

	// color of name
	purple_debug_info(DBGID, "changing colours\n");
	if(msg->from) {
		if( strcmp(msg->from, username) == 0) {
			from_eq_username = TRUE;
			purple_debug_info(DBGID, "self generated message, %s, %s\n", msg->from, username);
		}
		if(from_eq_username) {
			name_color = g_strdup("darkred");
		} else {
			name_color = g_strdup("darkblue");
		}
		g_string_append_printf(output, "<font color=\"%s\"><b>", name_color);
		//	self-filter is not possible now
		//	if(strcmp(msg->from, username) != 0) {
		uri_txt = mb_get_uri_txt(ta->account);
		if(reply_link && conv_name && uri_txt) {
			if(from_eq_username) {
				g_string_append_printf(output, "<i>");
			}
			/*
			purple_debug_info(DBGID, "current output = %s\n", output->str);
			purple_debug_info(DBGID, "url text = %s\n", mb_get_uri_txt(ta->account));
			purple_debug_info(DBGID, "conversation name = %s\n", conv_name);
			purple_debug_info(DBGID, "from = %s\n", msg->from);
			purple_debug_info(DBGID, "username = %s\n", username);
			purple_debug_info(DBGID, "id = %llu\n", msg->id);
			*/
			
			g_string_append_printf(output, "<a href=\"%s:reply?src=%s&to=%s&account=%s&id=%llu\">%s</a>:", uri_txt, conv_name, msg->from, username, msg->id, msg->from);
			if(from_eq_username) {
				g_string_append_printf(output, "</i>");
			}
		} else {
			g_string_append_printf(output, "%s:", msg->from);
		}
		//	}
		g_string_append_printf(output, "</b></font> ");
		g_free(name_color);
	}

	purple_debug_info(DBGID, "display msg = %s\n", output->str);
	purple_debug_info(DBGID, "source msg = %s\n", src);

	// now search message text and look for things to highlight
	previous_char = src[i];
	while(src[i] != '\0') {
		//purple_debug_info(DBGID, "previous_char = %c, src[i] == %c\n", previous_char, src[i]);
		if( (i == 0 || isspace(previous_char)) && 
			((src[i] == '@') || (src[i] == '#')) )
		{
			//purple_debug_info(DBGID, "reformat_msg: sym = %c\n", src[i]);
			sym = src[i];
			// if it's a proper name, extract it
			i++;
			j = i;
			while((src[j] != '\0') && 
				(isalnum(src[j]) ||
				 (src[j] == '_') ||
				 (src[j] == '-')
				)

			)
			{
				j++;
			}
			if(i == j) {
				// empty string
				g_string_append_c(output, sym);
				continue;
			}
			old_char = src[j];
			src[j] = '\0';
			name = &src[i];
			twitter_update_link(ta, output, sym, name);
			src[j] = old_char;
			i = j;
			previous_char = src[i-1];
		} else {
			g_string_append_c(output, src[i]);
			previous_char = src[i];
			i++;
		}
	}
	g_free(username);
	g_free(src);
	return g_string_free(output, FALSE);
}
