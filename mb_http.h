/**
*
* HTTP connection handling for Microblog
*
*/

#ifndef __MB_HTTP__
#define __MB_HTTP__

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

#include <sslconn.h>

#ifdef __cplusplus
extern "C" {
#endif

enum MbHttpStatus {
	HTTP_OK = 200,
	HTTP_MOVED_TEMPORARILY = 304,
	HTTP_NOT_FOUND = 404,
};

enum MbHttpRequestType{
	HTTP_GET = 1,
	HTTP_POST = 2,
};

enum MbHttpProto {
	MB_HTTP = 1,
	MB_HTTPS = 2,
	MB_PROTO_UNKNOWN = 100,
};

typedef struct _MbHttpData {
	gchar * host;
	gchar * path;
	gint port;
	gint proto;
	// url = proto://host:port/path
	GHashTable * headers;
	GList * params;
	GString * content;
	// For receiving side, content_len is the size of content
	// For sending side, setting content_len to -1 means automatically calculated
	gint status;
	gint type;
} MbHttpData;

typedef struct _MbHttpParam {
	gchar * key;
	gchar * value;
} MbHttpParam;

extern MbHttpData * mb_http_data_new(void);
extern void mb_http_data_free(MbHttpData * data);

extern gint mb_http_data_ssl_read(PurpleSslConnection * ssl, MbHttpData * data);
extern gint mb_http_data_ssl_write(PurpleSslConnection * ssl, MbHttpData * data);

extern void mb_http_data_set_url(MbHttpData * data, const gchar * url);
extern void mb_http_data_get_url(MbHttpData * data, gchar * url, gint url_len);

extern gint mb_http_data_set_header(MbHttpData* data, gchar * key, gchar * value);
extern gchar * mb_http_data_get_header(MbHttpData * data, gchar * key);

// function below might be static instead
extern MbHttpParam * mb_http_init_param(void);
extern void mb_http_free_param(MbHttpParam * param);

// Params is duplicable! Upto 
extern void mb_http_data_add_param(MbHttpData * data, gchar * key, gchar * value);
extern void mb_http_data_rm_param(MbHttpData * data, gchar * key);

// generic utility
extern gboolean mb_http_data_ok(MbHttpData * data);

#ifdef __cplusplus
}
#endif

#endif
