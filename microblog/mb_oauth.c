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
 * Implementation of OAuth support
 */

#include <stdlib.h>
#include <glib.h>
#include <math.h>

#include <debug.h>
#include <cipher.h>

#include "twitter.h"
#include "mb_net.h"
#include "mb_http.h"
#include "mb_util.h"

#include "mb_oauth.h"

#define DBGID "mboauth"

static const char fixed_headers[] = "User-Agent:" TW_AGENT "\r\n" \
"Accept: */*\r\n" \
"X-Twitter-Client: " TW_AGENT_SOURCE "\r\n" \
"X-Twitter-Client-Version: 0.1\r\n" \
"X-Twitter-Client-Url: " TW_AGENT_DESC_URL "\r\n" \
"Connection: Close\r\n" \
"Pragma: no-cache\r\n";


static MbConnData * mb_oauth_init_connection(MbAccount * ma, int type, const gchar * path, MbHandlerFunc handler, gchar ** full_url);
static gchar * mb_oauth_gen_nonce(void);
static gchar * mb_oauth_sign_hmac_sha1(const gchar * data, const gchar * key);
static gchar * mb_oauth_gen_sigbase(const gchar * url, int type, GList * params);
static gint mb_oauth_request_token_cb(MbConnData * conn_data, gpointer data);

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

/**
 * Initialize conn_data structure
 *
 * @param ma MbAccount in action
 * @param type type (HTTP_GET or HTTP_POST)
 * @param path path in URL
 * @param handler callback when connection established
 * @param full_url if not NULL, create the full url in this variable (need to be freed)
 */
static MbConnData * mb_oauth_init_connection(MbAccount * ma, int type, const gchar * path, MbHandlerFunc handler, gchar ** full_url)
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

	if(full_url) {
		(*full_url) = mb_url_unparse(host, 0, path, NULL, use_https);
	}

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
static gchar * mb_oauth_gen_nonce(void) {
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
//	return g_strdup("F_2urGzJ0q8Alzgbllio");
}

/**
 * Generate signature from data and key
 *
 * @param data to generate signature from
 * @param key key (secret)
 * @return string represent signature (must be freed by caller)
 */
static gchar * mb_oauth_sign_hmac_sha1(const gchar * data, const gchar * key) {
	PurpleCipherContext * context = NULL;
	size_t out_len;
	guchar digest[1024];
	gchar * retval = NULL;

	purple_debug_info(DBGID, "signing data \"%s\"\n with key \"%s\"\n", data, key);
	if( (context = purple_cipher_context_new_by_name("hmac", NULL)) == NULL) {
		purple_debug_info(DBGID, "couldn't find HMAC cipher, upgrade Pidgin?\n");
		return NULL;
	}
	purple_cipher_context_set_option(context, "hash", "sha1");
	purple_cipher_context_set_key(context, (guchar *)key);
	purple_cipher_context_append(context, (guchar *)data, strlen(data));

	if(purple_cipher_context_digest(context, sizeof(digest), digest, &out_len)) {
		//purple_debug_info(DBGID, "got digest = %s, out_len = %d\n", digest, (int)out_len);
		retval = purple_base64_encode(digest, out_len);
		purple_debug_info(DBGID, "got digest = %s, out_len = %d\n", retval, (int)out_len);
	} else {
		purple_debug_info(DBGID, "couldn't digest signature\n");
	}

	purple_cipher_context_destroy(context);

	return retval;
}

static int _string_compare_key(gconstpointer a, gconstpointer b) {
	const MbHttpParam * param_a = (MbHttpParam *)a;
	const MbHttpParam * param_b = (MbHttpParam *)b;

	return strcmp(param_a->key, param_b->key);
}

/**
 * Generate signature base text
 *
 * @param ma MbAccount in action
 * @param url path part of URL
 * @param type HTTP_GET or HTTP_POST
 * @param params list of PurpleKeyValuePair
 */
static gchar * mb_oauth_gen_sigbase(const gchar * url, int type, GList * params) {
	gchar * type_str = NULL, * param_str = NULL, * retval = NULL, *encoded_url, *encoded_param;
	MbHttpParam * param;
	GList * it;
	GString * param_string = g_string_new(NULL);
	int len;

	// Type
	if(type == HTTP_GET) {
		type_str = "GET";
	} else {
		type_str = "POST";
	}

	// Concatenate all parameter
    for(it = g_list_first(params); it; it = g_list_next(it)) {
		param = (MbHttpParam *)it->data;
    	g_string_append_printf(param_string, "%s=%s&", param->key, param->value);
    }
    purple_debug_info(DBGID, "merged param string = %s\n", param_string->str);
    len = param_string->len;
    param_str = g_string_free(param_string, FALSE);
    if( (len >= 1) && (param_str[len - 1] == '&')) {
    	param_str[len - 1] = '\0';
    }
    purple_debug_info(DBGID, "final merged param string = %s\n", param_str);

    encoded_url = g_strdup(purple_url_encode(url));
    encoded_param = g_strdup(purple_url_encode(param_str));
    retval = g_strdup_printf("%s&%s&%s", type_str, encoded_url, encoded_param);
    g_free(encoded_url);
    g_free(encoded_param);

    g_free(param_str);

    return retval;
}

void mb_oauth_request_token(struct _MbAccount * ma, const gchar * path, int type, MbOauthUserInput func, gpointer data) {
	MbConnData * conn_data = NULL;
	gchar * secret = NULL, * sig_base = NULL, * signature = NULL, * nonce = NULL;
	gchar * full_url = NULL;

	conn_data = mb_oauth_init_connection(ma, type, path, mb_oauth_request_token_cb, &full_url);

	// Attach OAuth data
	mb_http_data_add_param(conn_data->request, "oauth_consumer_key", ma->oauth.c_key);

	nonce = mb_oauth_gen_nonce();
	mb_http_data_add_param(conn_data->request, "oauth_nonce", nonce);
	g_free(nonce);

	mb_http_data_add_param(conn_data->request, "oauth_signature_method", "HMAC-SHA1");
	mb_http_data_add_param_ull(conn_data->request, "oauth_timestamp", time(NULL));
//	mb_http_data_add_param_ull(conn_data->request, "oauth_timestamp", 1274351275);
	mb_http_data_add_param(conn_data->request, "oauth_version", "1.0");
	conn_data->request->params = g_list_sort(conn_data->request->params, _string_compare_key);

	// Create signature
	sig_base = mb_oauth_gen_sigbase(full_url, type, conn_data->request->params);
	purple_debug_info(DBGID, "got signature base = %s\n", sig_base);

	secret = g_strdup_printf("%s&%s", ma->oauth.c_secret, ma->oauth.request_secret ? ma->oauth.request_secret : "");

	signature = mb_oauth_sign_hmac_sha1(sig_base, secret);
	g_free(secret);
	g_free(sig_base);
	purple_debug_info(DBGID, "signed signature = %s\n", signature);

	// Attach to parameter
	mb_http_data_add_param(conn_data->request, "oauth_signature", signature);
	g_free(signature);

	// Set the call back function
	ma->oauth.input_func = func;

	// Initiate connection
	conn_data->handler_data = ma;
	mb_conn_process_request(conn_data);
}

static gint mb_oauth_request_token_cb(MbConnData * conn_data, gpointer data) {
	MbAccount * ma = (MbAccount *)data;

	purple_debug_info(DBGID, "got response %s\n", conn_data->response->content->str);
	// now call the user-defined function to get PIN or whatever we need to request for access token
	// will not call request_access by ourself, let the func do the job, since the caller should know the URL
	if(ma && ma->oauth.input_func) {
		ma->oauth.input_func(ma, data);
	}

	return 0;
}

void mb_oauth_request_access(struct _MbAccount * ma, const gchar * url, int type, MbOauthResponse func, gpointer data) {

}
