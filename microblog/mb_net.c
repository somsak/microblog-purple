/*
	Microblog network processing (mostly for HTTP data)
 */
 
#include "mb_net.h"
 
MbConnData * mb_conn_data_new(MBAccount * ta, MbHandlerFunc handler, gboolean is_ssl)
{
	MbConnData * conn_data = NULL;
	
	conn_data = g_new(MbConnData, 1);
	
	conn_data->ta = ta;
	conn_data->error_message = NULL;
	conn_data->handler = handler;
	conn_data->handler_data = NULL;
	conn_data->retry = 0;
	conn_data->max_retry = 0;
	conn_data->action_on_error = MB_ERROR_NOACTION;
	conn_data->ssl_conn_data = NULL;
	conn_data->is_ssl = TRUE; //< True for now
	conn_data->request = mb_http_data_new();
	conn_data->response = mb_http_data_new();
	
	return conn_data;
}

void mb_conn_data_free(MbConnData * conn_data)
{
	mb_http_data_free(conn_data->response);
	mb_http_data_free(conn_data->request);
	if(conn_data->error_message) {
		g_free(conn_data->error_message);
	}
	if(conn_data->handler_data) {
		conn_data->handler_data_free(conn_data->handler_data);
	}
	if(conn_data->ssl_conn_data) {
		purple_ssl_close(conn_data->ssl_conn_data);
	}
	g_free(conn_data);
}

void mb_conn_data_set_error(MbConnData * data, const gchar * msg, gint action)
{
	if(data->error_message) {
		g_free(data->error_message);
	}
	data->error_message = g_strdup(msg);
	data->action_on_error = action;
}

void mb_conn_data_set_retry(MbConnData * data, gint retry)
{
	data->max_retry = retry;
}

extern void mb_conn_process_request(MbConnData * data);
extern void mb_conn_post_ssl_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_get_ssl_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
extern void mb_conn_connect_ssl_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data);
