/*
 * mb_oauth.h
 *
 *  Created on: May 16, 2010
 *      Author: somsak
 */

#ifndef MB_OAUTH_H_
#define MB_OAUTH_H_

#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#ifdef __cplusplus
extern "C" {
#endif


struct _MbAccount;

typedef void (* MbOauthUserInput)(struct _MbAccount * ma, gpointer data);
typedef void (* MbOauthResponse)(struct _MbAccount * ma, gpointer data);

typedef struct _MbOauth {
	gchar * c_key; //< Consumer key
	gchar * c_secret; //< Consumer secret
	gchar * request_token; //< request token
	gchar * request_secret; //< request secret
	gchar * pin; //< user input PIN
	gchar * access_token; //< access token
	gchar * access_secret; //< access secret
	MbOauthUserInput input_func;
	MbOauthResponse response_func;
	struct _MbAccount * ma;
	gpointer data;
} MbOauth;

void mb_oauth_init(struct _MbAccount * ma, const gchar * c_key, const gchar * c_secret);
void mb_oauth_requst_token(struct _MbAccount * ma, const gchar * url, int type, MbOauthUserInput func, gpointer data);
void mb_oauth_requst_access(struct _MbAccount * ma, const gchar * url, int type, MbOauthResponse func, gpointer data);
void mb_oauth_free(struct _MbAccount * ma);

#ifdef __cplusplus
}
#endif

#endif /* MB_OAUTH_H_ */
