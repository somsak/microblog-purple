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

typedef TwitterAccount MbAccount; //< for the sake of simplicity for now

enum mb_error_action {
	MB_ERROR_NOACTION = 0,
	MB_ERROR_RAISE_ERROR = 1,
};

// if handler return
// 0 - Everything's ok
// -1 - Requeue the whole process again
struct _MbConnData;

typedef gint (*MbHandlerFunc)(struct _MbConnData * , gpointer );
typedef void (*MbHandlerDataFreeFunc)(gpointer);

typedef struct _MbConnData {
	gchar * host;
	gint port;
	MbAccount * ta;
	gchar * error_message;
	MbHttpData * request;
	MbHttpData * response;
	gint retry;
	gint max_retry;
	MbHandlerFunc handler;
	gpointer handler_data;
	MbHandlerDataFreeFunc handler_data_free;
	gint action_on_error;
	PurpleSslConnection * ssl_conn_data;
	gboolean is_ssl;
} MbConnData;

/*
	Create new connection data
	
	@param ta MbAccount instance
	@param handler handler function for this request
	@param is_ssl whether this is SSL or not
	@return new MbConnData
*/
extern MbConnData * mb_conn_data_new(MbAccount * ta, const gchar * host, gint port, MbHandlerFunc handler, gboolean is_ssl);

/*
	Free an instance of MbConnData
	
	@param conn_data conn_data to free
	@note connection will be closed if it's still open
*/
extern void mb_conn_data_free(MbConnData * conn_data);

extern void mb_conn_data_set_error(MbConnData * data, const gchar * msg, gint action);
extern void mb_conn_data_set_retry(MbConnData * data, gint retry);
extern void mb_conn_process_request(MbConnData * data);
extern void mb_conn_post_ssl_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_get_ssl_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_connect_ssl_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data);

#ifdef __cplusplus
}
#endif

#endif
