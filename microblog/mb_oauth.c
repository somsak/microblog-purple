/*
 * mb_oauth.c
 *
 *  Created on: May 16, 2010
 *      Author: somsak
 */

#include <stdlib.h>
#include "twitter.h"
#include "mb_http.h"

#include "mb_oauth.h"

static const char fixed_headers[] = "User-Agent:" TW_AGENT "\r\n" \
"Accept: */*\r\n" \
"X-Twitter-Client: " TW_AGENT_SOURCE "\r\n" \
"X-Twitter-Client-Version: 0.1\r\n" \
"X-Twitter-Client-Url: " TW_AGENT_DESC_URL "\r\n" \
"Connection: Close\r\n" \
"Pragma: no-cache\r\n";

void mb_oauth_init(struct _MbAccount * ma, const gchar * c_key, const gchar * c_secret)
{
	MbOauth * oauth = &ma->oauth;

	oauth->c_key = g_strdup(c_key);
	oauth->c_secret = g_strdup(c_secret);
	oauth->request_token = NULL;
	oauth->request_secret = NULL;
	oauth->pin = NULL;
	oauth->access_token = NULL;
	oauth->access_secret = NULL;

	oauth->ma = ma;

	// XXX: Should we put this other places instead?
	srand(time(NULL));
}

void mb_oauth_free(struct _MbAccount * ma)
{
	MbOauth * oauth = &ma->oauth;
	if(oauth->c_key) g_free(oauth->c_key);
	if(oauth->c_secret) g_free(oauth->c_secret);

	if(oauth->request_token) g_free(oauth->request_token);
	if(oauth->request_secret) g_free(oauth->request_secret);

	if(oauth->pin) g_free(oauth->pin);

	if(oauth->access_token) g_free(oauth->access_token);
	if(oauth->access_secret) g_free(oauth->access_secret);

	oauth->c_key = NULL;
	oauth->c_secret = NULL;
	oauth->request_token = NULL;
	oauth->request_secret = NULL;
	oauth->access_token = NULL;
	oauth->access_secret = NULL;
}

static MbConnData * mb_oauth_init_connection(MbAccount * ma, const gchar * path)
{
	MbConnData * conn_data = NULL;
	gboolean use_https = purple_account_get_bool(ma->account, mc_name(TC_USE_HTTPS), mc_def_bool(TC_USE_HTTPS));
	gint port;
	gchar * user = NULL, * host = NULL;

	if(use_https) {
		port = TW_HTTPS_PORT;
	} else {
		port = TW_HTTP_PORT;
	}

	twitter_get_user_host(ma, &user, &host);
	conn_data = mb_conn_data_new(ma, host, port, handler, use_https);
	mb_conn_data_set_retry(conn_data, 2);

	conn_data->request->type = type;
	conn_data->request->port = port;

	mb_http_data_set_host(conn_data->request, host);
	mb_http_data_set_path(conn_data->request, path);

	// XXX: Use global here -> twitter_fixed_headers
	mb_http_data_set_fixed_headers(conn_data->request, fixed_headers);
	mb_http_data_set_header(conn_data->request, "Host", host);

	if(user) g_free(user);
	if(host) g_free(host);

	return conn_data;

}

/**
 * generate a random string between 15 and 32 chars length
 * and return a pointer to it. The value needs to be freed by the
 * caller
 *
 * @return zero terminated random string.
 * @note this function was taken from by liboauth
 */
static gchar * mb_oauth_gen_nounce(void) {
	char *nc;
	const char *chars = "abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ" "0123456789_";
	unsigned int max = strlen( chars );
	int i, len;

	len = 15 + floor(rand()*16.0/(double)RAND_MAX);
	nc = g_new(char, len+1);
	for(i=0;i<len; i++) {
		nc[i] = chars[ rand() % max ];
	}
	nc[i]='\0';
	return (nc);
}

/**
 * Generate signature from data and key
 *
 * @param data to generate signature from
 * @param key key (secret)
 * @return string represent signature (must be freed by caller)
 */
static gchar * mb_oauth_sign_signature(const gchar * data, const gchar * key) {

}

/**
 * Generate signature base text
 *
 * @param ma MbAccount in action
 * @param url path part of URL
 * @param type HTTP_GET or HTTP_POST
 * @param params list of PurpleKeyValuePair
 */
static gchar * mb_oauth_gen_sigbase(const gchar * url, int type, const GList * params) {
	gchar * type_str = NULL, * url = NULL, * proto = NULL, * param_str = NULL, * retval = NULL;
	GList * it;
	GString * param_string = g_string_new(url);
	int len;

	// Type
	if(type == HTTP_GET) {
		type_str = "GET";
	} else {
		type_str = "POST";
	}

	// Concatenate all parameter
	g_string_append_c(param_string, '/');
    g_list_sort(params, strcmp);
    for(it = g_list_first(params); it; it = g_list_next(it)) {
    	g_string_append_printf(param_string, "%s=%s&");
    }
    len = param_string->len;
    param_str = g_string_free(param_string);
    if( (len >= 1) && (param_str[len - 1] == '&')) {
    	param_str[len - 1] = '\0';
    }

    retval = g_strdup_printf("%s&%s", type_str, purple_url_encode(param_str));

    g_free(param_str);

    return retval;
}

void mb_oauth_requst_token(struct _MbAccount * ma, const gchar * url, int type, MbOauthUserInput func, gpointer data) {
	MbConnData * conn_data = mb_oauth_init_connection(ma, url);
	gchar * sig_base = NULL;mb_oauth_gen_sigbase(url, type, params);

	// Create signature
	sig_base = mb_oauth_gen_sigbase(url, type, params);

	// Attach OAuth data


	ma->oauth.input_func = func;

	// Initiate connection
}

void mb_oauth_request_token_cb(MbConnData * conn_data, gpointer data) {
	MbAccount * ma = (MbAccount *)data;

	// now call the user-defined function to get PIN or whatever we need to request for access token
	// will not call request_access by ourself, let the func do the job, since the caller should know the URL
	ma->oauth.input_func(ma, data);
}

void mb_oauth_requst_access(struct _MbAccount * ma, const gchar * url, int type, MbOauthResponse func, gpointer data) {

}
