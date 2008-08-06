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

typedef struct _MbConnData {
	TwitterAccount * ta;
	gchar * error_message;
	MbHttpData * request;
	MbHttpData * response;
	gint retry;
	gint max_retry;
	TwitterHandlerFunc handler;
	gpointer handler_data;
	gint action_on_error;
	PurpleSslConnection * conn_data;
	gboolean is_ssl;
	gint conn_id;
} MbConnData;

extern MbConnData * mb_conn_data_new(void);
extern void mb_conn_process_request(gpointer data);
extern void mb_conn_post_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_get_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_connect_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data);

#ifdef __cplusplus
}
#endif

#endif