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
/**
 * This is a "script" to modify account.xml and replace old prpl name to new one
 *
 * Namely, changing prpl-somsaks-twitter-> prpl-mbpurple-twitter
 *
 * To cover up my stupidity :P
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

int main(int argc, char * argv[])
{
	FILE * fp = NULL;
	char * account = NULL;
	GString * account_content = NULL;
	char buf[10240];
	char * key;

	if(argc < 2) {
		printf("Usage: %s <purple config file>\n", argv[0]);
		return EXIT_FAILURE;
	}
	account = argv[1];
	if( (fp = fopen(account, "r+")) == NULL) {
		printf("Can not open file %s\n", account);
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	// read in the account file, then modify its content 
	account_content = g_string_new(NULL);
	while(fgets(buf, sizeof(buf), fp) != NULL) {
		if( (key = strstr(buf, "<protocol>prpl-somsaks-twitter</protocol>")) != NULL) {
			char * rest_of_line = key + 41;
			char tmp_buf[10240];

			strcpy(tmp_buf, rest_of_line);
			(*key) = '\0';
			strcat(key, "<protocol>prpl-mbpurple-twitter</protocol>");
			strcat(key, tmp_buf);
		}
		g_string_append(account_content, buf);
	}
	//
	fseek(fp, 0, SEEK_SET);
	fputs(account_content->str, fp);
	fclose(fp);

	return EXIT_SUCCESS;
}
