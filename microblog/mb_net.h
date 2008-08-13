/*
	Microblog network processing (mostly for HTTP data)
 */
 
#ifndef __MB_NET__
#define __MB_NET__

#include "mb_http.h"
#include "twitter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TwitterAccount MBAccount //< for the sake of simplicity for now

// if handler return
// 0 - Everything's ok
// -1 - Requeue the whole process again
struct _MbConnData;

typedef gint (*MbHandlerFunc)(struct _MbConnData * , gpointer );

typedef struct _MbConnData {
	MBAccount * ta;
	gchar * error_message;
	MbHttpData * request;
	MbHttpData * response;
	gint retry;
	gint max_retry;
	MbHandlerFunc handler;
	gpointer handler_data;
	gint action_on_error;
	PurpleSslConnection * conn_data;
	gboolean is_ssl;
	gint conn_id;
} MbConnData;

extern MbConnData * mb_conn_data_new(MBAccount * ta);
extern void mb_conn_data_free(MbConnData * data);
extern void mb_conn_process_request(gpointer data);
extern void mb_conn_post_ssl_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_get_ssl_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_connect_ssl_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data);

#ifdef __cplusplus
}
#endif

#endif