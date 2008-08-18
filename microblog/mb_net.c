/*
	Microblog network processing (mostly for HTTP data)
 */
 
#include "mb_net.h"

#include <debug.h>
 
MbConnData * mb_conn_data_new(MbAccount * ta, const gchar * host, gint port, MbHandlerFunc handler, gboolean is_ssl)
{
	MbConnData * conn_data = NULL;
	
	conn_data = g_new(MbConnData, 1);
	
	conn_data->host = g_strdup(host);
	conn_data->port = port;
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
	g_free(conn_data->host);
	mb_http_data_free(conn_data->response);
	mb_http_data_free(conn_data->request);
	if(conn_data->error_message) {
		g_free(conn_data->error_message);
	}
	/*
	if(conn_data->handler_data) {
		conn_data->handler_data_free(conn_data->handler_data);
	}
	*/
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

void mb_conn_process_request(MbConnData * data)
{
	MbAccount *ta = data->ta;
	
	purple_debug_info(MB_NET, "mb_conn_process_request\n");

	purple_debug_info(MB_NET, "connecting to %s on port %hd\n", data->host, data->port);
	if(data->is_ssl) {
		data->ssl_conn_data = purple_ssl_connect(ta->account, data->host, data->port, mb_conn_post_ssl_request, mb_conn_connect_ssl_error, data);
		purple_debug_info(MB_NET, "after connect\n");
		if(data->ssl_conn_data != NULL) {
			// add this to internal hash table
			g_hash_table_insert(ta->ssl_conn_hash, data->ssl_conn_data, data);
			purple_debug_info(MB_NET, "connect (seems to) success\n");
		}
	} else {
		// Ignore for now
	}
}

void mb_conn_post_ssl_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond)
{
	MbConnData * conn_data = data;
	MbAccount *ta = conn_data->ta;
	gint res;
	
	purple_debug_info(MB_NET, "mb_conn_post_ssl_request\n");
	
	if (!ta || ta->state == PURPLE_DISCONNECTED || !ta->account || ta->account->disconnecting)
	{
		purple_debug_info(MB_NET, "we're going to be disconnected?\n");
		purple_ssl_close(ssl);
		conn_data->ssl_conn_data = NULL;
		return;
	}
	
	purple_debug_info(MB_NET, "mb_conn posting request\n");
	while(conn_data->request->state != MB_HTTP_STATE_FINISHED) {
		res = mb_http_data_ssl_write(ssl, conn_data->request);
		if(res <= 0) break;
	}
	
	if(res <= 0) {
		// error connecting
		purple_debug_info(MB_NET, "error while posting request %s\n", conn_data->request->content->str);
		purple_connection_error(ta->gc, _(conn_data->error_message));
	} else {
		purple_ssl_input_add(ssl, mb_conn_get_ssl_result, conn_data);
	}
}

void mb_conn_connect_ssl_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data)
{
	MbConnData * conn_data = data;
	MbAccount *ta = conn_data->ta;

	//ssl error is after 2.3.0
	//purple_connection_ssl_error(fba->gc, errortype);
	purple_connection_error(ta->gc, _("Connection Error"));
	if(conn_data->ssl_conn_data) {
		purple_debug_info(MB_NET, "removing conn_data from hash table\n");
		g_hash_table_remove(ta->ssl_conn_hash, conn_data->ssl_conn_data);
		//purple_ssl_close(tpd->conn_data); //< Pidgin will free this for us after this
		conn_data->ssl_conn_data = NULL;
	}
	mb_conn_data_free(conn_data);
}

void mb_conn_get_ssl_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond)
{

	MbConnData * conn_data = data;
	MbAccount *ta = conn_data->ta;
	MbHttpData * response = conn_data->response;
	gint res, cur_error;
	gboolean call_handler = FALSE;
	
	purple_debug_info(MB_NET, "mb_conn_get_ssl_result\n");

	//purple_debug_info(MB_NET, "new cur_result_pos = %d\n", tpd->cur_result_pos);
	res = mb_http_data_ssl_read(ssl, response);
	cur_error = errno;
	if( (res < 0) && (cur_error != EAGAIN) ) {
		// error connecting or reading
		purple_input_remove(ssl->inpa);
		// First, chec if we already have everythings
		if(response->state == MB_HTTP_STATE_FINISHED) {
			// All is fine, proceed to handler
			purple_input_remove(ssl->inpa);
			if(conn_data->ssl_conn_data) {
				g_hash_table_remove(ta->ssl_conn_hash, conn_data->ssl_conn_data);
				purple_ssl_close(conn_data->ssl_conn_data);
				conn_data->ssl_conn_data = NULL;			
			}
			call_handler = TRUE;
		} else {
			// Free all data
			mb_http_data_truncate(response);

			if(conn_data->ssl_conn_data) {
				g_hash_table_remove(ta->ssl_conn_hash, conn_data->ssl_conn_data);
				purple_ssl_close(conn_data->ssl_conn_data);
				conn_data->ssl_conn_data = NULL;
			}
			conn_data->retry += 1;
			if(conn_data->retry <= conn_data->max_retry) {
				// process request will reconnect and exit
				// FIXME: should we add it to timeout here instead?
				purple_debug_info(MB_NET, "retrying request\n");
				mb_conn_process_request(conn_data);
				return;
			} else {
				purple_debug_info(MB_NET, "error while reading data, res = %d, retry = %d, error = %s\n", res, conn_data->retry, strerror(cur_error));
				if(conn_data->action_on_error == MB_ERROR_RAISE_ERROR) {
					purple_connection_error(ta->gc, _(conn_data->error_message));
				}
				mb_conn_data_free(conn_data);
			}
		}
	} else if( ((res < 0) && (cur_error == EAGAIN)) ||
			(res > 0) )
	{
		if(res < 0) {
			purple_debug_info(MB_NET, "error with EAGAIN\n");
		} else {
			purple_debug_info(MB_NET, "need more data\n");
		}
		// do we need to remove and add here? I just want to make sure everything's working
		purple_input_remove(ssl->inpa);
		purple_ssl_input_add(ssl, mb_conn_get_ssl_result, conn_data);
	} else if(res == 0) {
		// we have all data
		purple_input_remove(ssl->inpa);
		if(conn_data->ssl_conn_data) {
			g_hash_table_remove(ta->ssl_conn_hash, conn_data->ssl_conn_data);
			purple_ssl_close(conn_data->ssl_conn_data);
			conn_data->ssl_conn_data = NULL;			
		}
		call_handler = TRUE;
	} // global if else for connection state
	
	// Call handler here
	if(call_handler) {
		// reassemble data
		purple_debug_info(MB_NET, "got whole response = %s\n", response->content->str);
		if(conn_data->handler) {
			gint retval;
			
			retval = conn_data->handler(conn_data, conn_data->handler_data);
			if(retval == 0) {
				// Everything's good. Free data structure and go-on with usual works
				mb_conn_data_free(conn_data);
			} else if(retval == -1) {
				// Something's wrong. Requeue the whole process
				mb_http_data_truncate(response);
				mb_conn_process_request(conn_data);
			}
		} // if handler != NULL
	}
}


