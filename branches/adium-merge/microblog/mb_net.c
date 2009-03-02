/*
	Microblog network processing (mostly for HTTP data)
 */

#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>


#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#ifdef _WIN32
#	include <win32dep.h>
#else
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#endif

#include <debug.h>
#include "mb_net.h"

static void mb_conn_connect_cb(gpointer data, int source, const gchar * error_message);
static void mb_conn_post_request(gpointer data, gint source, PurpleInputCondition cond);
static void mb_conn_post_ssl_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
static void mb_conn_get_result(gpointer data, gint source, PurpleInputCondition cond);
static void mb_conn_get_ssl_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond);
static void mb_conn_connect_ssl_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data);

 
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
	//conn_data->conn_data = NULL;
	conn_data->conn_event_handle = 0;
	conn_data->ssl_conn_data = NULL;
	conn_data->is_ssl = is_ssl;
	conn_data->request = mb_http_data_new();
	conn_data->response = mb_http_data_new();
	if(conn_data->is_ssl) {
		conn_data->request->proto = MB_HTTPS;
	} else {
		conn_data->request->proto = MB_HTTP;
	}
	
	purple_debug_info(MB_NET, "new: create conn_data = %p\n", conn_data);
	return conn_data;
}

void mb_conn_data_free(MbConnData * conn_data)
{
	purple_debug_info(MB_NET, "free: conn_data = %p\n", conn_data);
	if(conn_data->conn_event_handle) {
		//purple_debug_info(MB_NET, "removing connection %p from conn_hash\n", conn_data->conn_data);
		//g_hash_table_remove(conn_data->ta->conn_hash, conn_data->conn_data);
		purple_debug_info(MB_NET, "removing event handle, event_handle = %u\n", conn_data->conn_event_handle);
		purple_input_remove(conn_data->conn_event_handle);	
		// should we cancel it here? There's chance in connect_cb that conn_event_handle is not non-zero yet
		purple_proxy_connect_cancel_with_handle(conn_data);
	}
	purple_debug_info(MB_NET, "removing conn_data\n");
	if(conn_data->ssl_conn_data != NULL) {
		purple_debug_info(MB_NET, "ssl_conn_data = %p\n", conn_data->ssl_conn_data);
		//purple_debug_info(MB_NET, "removing connection %p from ssl_conn_hash\n", conn_data->ssl_conn_data);
		//g_hash_table_remove(conn_data->ta->ssl_conn_hash, conn_data->ssl_conn_data);
		purple_debug_info(MB_NET, "removing SSL event\n");
		purple_input_remove(conn_data->ssl_conn_data->inpa);
		purple_debug_info(MB_NET, "closing SSL connection\n");
		purple_ssl_close(conn_data->ssl_conn_data);
	}
	purple_debug_info(MB_NET, "freeing the rest of data\n");
	
	if(conn_data->host) {
		purple_debug_info(MB_NET, "freeing host name\n");
		g_free(conn_data->host);
	}
	purple_debug_info(MB_NET, "freeing HTTP data->response\n");
	mb_http_data_free(conn_data->response);
	purple_debug_info(MB_NET, "freeing HTTP data->request\n");
	mb_http_data_free(conn_data->request);
	purple_debug_info(MB_NET, "freeing error message\n");
	if(conn_data->error_message) {
		g_free(conn_data->error_message);
	}
	purple_debug_info(MB_NET, "freeing self at %p\n", conn_data);
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
	
	purple_debug_info(MB_NET, "mb_conn_process_request, conn_data = %p\n", data);

	purple_debug_info(MB_NET, "connecting to %s on port %hd\n", data->host, data->port);
	if(data->is_ssl) {
		purple_debug_info(MB_NET, "connecting using SSL connection\n");
		data->ssl_conn_data = purple_ssl_connect(ta->account, data->host, data->port, mb_conn_post_ssl_request, mb_conn_connect_ssl_error, data);
		purple_debug_info(MB_NET, "after connect\n");
		/*
		if(data->ssl_conn_data != NULL) {
			purple_debug_info(MB_NET, "connect (seems to) success\n");
		}
		*/
	} else {
		purple_debug_info(MB_NET, "connecting using non-SSL connection to %s, %d\n", data->host, data->port);
		purple_proxy_connect(data, ta->account, data->host, data->port, mb_conn_connect_cb, data);
		purple_debug_info(MB_NET, "after connect\n");
	}
}

void mb_conn_post_request(gpointer data, gint source, PurpleInputCondition cond)
{
	MbConnData * conn_data = data;
	MbAccount * ta = conn_data->ta;
	gint res, cur_error;
	
	purple_debug_info(MB_NET, "mb_conn_post_request, source = %d, conn_data = %p\n", source, conn_data);
	if(conn_data->conn_event_handle > 0) {
		purple_input_remove(conn_data->conn_event_handle);
		conn_data->conn_event_handle = 0;
	}
	
	/* XXX Hmm... should we have to check for disconnect here?
	if (!ta || ta->state == PURPLE_DISCONNECTED || !ta->account || ta->account->disconnecting)
	{
		purple_debug_info(MB_NET, "we're going to be disconnected?\n");
		g_hash_table_remove(ta->conn_hash, &source);
		purple_proxy_connect_cancel_with_handle(conn_data);
		return;
	}
	*/
	
	/*
	purple_debug_info(MB_NET, "test reading something\n");
	res = read(source, buf, sizeof(buf));
	purple_debug_info(MB_NET, "res = %d\n", res);
	*/
	purple_debug_info(MB_NET, "posting request\n");
	res = mb_http_data_write(source, conn_data->request);
	cur_error = errno;
	
	purple_debug_info(MB_NET, "res = %d\n", res);
	if(res < 0) {
		if(cur_error == EAGAIN) {
				// try again
			purple_debug_info(MB_NET, "data is not yet wholely written, retry again, res = %d\n", res);
			conn_data->conn_event_handle = purple_input_add(source, PURPLE_INPUT_WRITE, mb_conn_post_request, conn_data);
		} else {
			// error connecting
			purple_debug_info(MB_NET, "error while posting request, error = %s\n", strerror(cur_error));
			purple_connection_error(ta->gc, _(conn_data->error_message));
		}
	} else if(conn_data->request->state == MB_HTTP_STATE_FINISHED) {
		purple_debug_info(MB_NET, "write success, adding eventloop for fd = %d\n", source);
		conn_data->conn_event_handle = purple_input_add(source, PURPLE_INPUT_READ, mb_conn_get_result, conn_data);
	} else {
		// try again
		purple_debug_info(MB_NET, "data is not yet wholely written, retry again, res = %d\n", res);
		conn_data->conn_event_handle = purple_input_add(source, PURPLE_INPUT_WRITE, mb_conn_post_request, conn_data);
	}
}

void mb_conn_connect_cb(gpointer data, int source, const gchar * error_message)
{
	MbConnData * conn_data = data;
	MbAccount * ta = conn_data->ta;

	purple_debug_info(MB_NET, "mb_conn_connect_cb, source = %d, conn_data = %p\n", source, conn_data);

	if (!ta || ta->state == PURPLE_DISCONNECTED || !ta->account || ta->account->disconnecting)
	{
		purple_debug_info(MB_NET, "we're going to be disconnected?\n");
		purple_proxy_connect_cancel_with_handle(conn_data);
		//conn_data->conn_data = NULL;
		return;
	}

	//conn_data->conn_data = NULL;
	if( (source < 0) ||  error_message) {
		// if connection error, source == -1 and error_message is not null!
		purple_debug_info(MB_NET, "error_messsage = %s\n", error_message);
		purple_connection_error(ta->gc, _(error_message));
		return;
	} else {
		gint * tmp;
		
		tmp = g_new(gint, 1);
		(*tmp) = source;
		purple_debug_info(MB_NET, "adding connection %p to conn_hash with key = %d (%p) \n", conn_data, (*tmp), tmp);
		g_hash_table_insert(ta->conn_hash, tmp, conn_data);
		purple_debug_info(MB_NET, "adding fd = %d to write event loop\n", source);
		conn_data->conn_event_handle = purple_input_add(source, PURPLE_INPUT_WRITE, mb_conn_post_request, conn_data);
	}
}

void mb_conn_post_ssl_request(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond)
{
	MbConnData * conn_data = data;
	MbAccount *ta = conn_data->ta;
	gint res = 0;
	
	purple_debug_info(MB_NET, "mb_conn_post_ssl_request, conn_data = %p\n", conn_data);
	
	//XXX: disconnect here seems to be the right thing
	if (!ta || ta->state == PURPLE_DISCONNECTED || !ta->account || ta->account->disconnecting)
	{
		purple_debug_info(MB_NET, "we're going to be disconnected?\n");
		purple_ssl_close(ssl);
		conn_data->ssl_conn_data = NULL;
		return;
	}
	// add this to internal hash table
	purple_debug_info(MB_NET, "adding SSL connection %p to ssl_conn_hash with key = %p\n", conn_data, conn_data->ssl_conn_data);
	g_hash_table_insert(ta->ssl_conn_hash, conn_data->ssl_conn_data, conn_data);
	
	purple_debug_info(MB_NET, "mb_conn posting request\n");
	while(conn_data->request->state != MB_HTTP_STATE_FINISHED) {
		res = mb_http_data_ssl_write(ssl, conn_data->request);
		purple_debug_info(MB_NET, "sub-request posted\n");
		if(res <= 0) {
			break;
		}
	}
	
	purple_debug_info(MB_NET, "request posted\n");
	if(res < 0) {
		// error connecting
		purple_debug_info(MB_NET, "error while posting request %s\n", conn_data->request->content->str);
		purple_connection_error(ta->gc, _(conn_data->error_message ? conn_data->error_message : "error while sending request"));
	} else if(conn_data->request->state == MB_HTTP_STATE_FINISHED) {
		purple_debug_info(MB_NET, "request posting success\n");
		purple_ssl_input_add(ssl, mb_conn_get_ssl_result, conn_data);
	} else {
		purple_debug_info(MB_NET, "can not send request in single chunk!\n");
		purple_connection_error(ta->gc, _(conn_data->error_message ? conn_data->error_message : "sending request error, too little packet?"));
	}
}

void mb_conn_connect_ssl_error(PurpleSslConnection *ssl, PurpleSslErrorType errortype, gpointer data)
{
	MbConnData * conn_data = data;
	MbAccount *ta = conn_data->ta;
	const gchar * error;

	//ssl error is after 2.3.0
	//purple_connection_ssl_error(fba->gc, errortype);
	/*
	purple_debug_info(MB_NET, "ssl_error\n");
	if(ta->gc != NULL) {
		//XXX: what happened this line? SIGSEGV here a lot!
		purple_connection_error(ta->gc, _("Connection Error"));
	} else {
		purple_debug_info(MB_NET, "gc is null for some reason\n");
	}
	if(conn_data->ssl_conn_data) {
		//purple_ssl_close(tpd->conn_data); //< Pidgin will free this for us after this
		conn_data->ssl_conn_data = NULL;
	}
	//XXX: Should we free it here?
	mb_conn_data_free(conn_data);
	*/
	error = purple_ssl_strerror(errortype);
	if(error) {
		purple_connection_error(ta->gc, purple_ssl_strerror(errortype));
	} else {
		purple_connection_error(ta->gc, _("SSL Connection error"));
	}
}

void mb_conn_get_result(gpointer data, gint source, PurpleInputCondition cond)
{

	MbConnData * conn_data = data;
	MbAccount *ta = conn_data->ta;
	MbHttpData * response = conn_data->response;
	gint res, cur_error;
	gboolean call_handler = FALSE;
	
	purple_debug_info(MB_NET, "mb_conn_get_result, conn_data = %p\n", conn_data);

	//purple_debug_info(MB_NET, "new cur_result_pos = %d\n", tpd->cur_result_pos);
	res = mb_http_data_read(source, response);
	purple_debug_info(MB_NET, "result from read = %d\n", res);
	cur_error = errno;
	if( (res < 0) && (cur_error != EAGAIN) ) {
		// error connecting or reading
		purple_input_remove(conn_data->conn_event_handle);
		// First, chec if we already have everythings
		if(response->state == MB_HTTP_STATE_FINISHED) {
			// All is fine, proceed to handler
			purple_input_remove(conn_data->conn_event_handle);
			if(conn_data->conn_event_handle >= 0) {
				g_hash_table_remove(ta->conn_hash, &source);
				purple_proxy_connect_cancel_with_handle(conn_data);
				//conn_data->conn_data = NULL;			
			}
			call_handler = TRUE;
		} else {
			// Free all data
			mb_http_data_truncate(response);

			if(conn_data->conn_event_handle >= 0) {
				g_hash_table_remove(ta->conn_hash, &source);
				purple_proxy_connect_cancel_with_handle(conn_data);
				//conn_data->conn_data = NULL;
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
		purple_input_remove(conn_data->conn_event_handle);
		conn_data->conn_event_handle = purple_input_add(source, PURPLE_INPUT_READ, mb_conn_get_result, conn_data);
	} else if(res == 0) {
		// we have all data
		purple_input_remove(conn_data->conn_event_handle);
		g_hash_table_remove(ta->conn_hash, &source);
		purple_proxy_connect_cancel_with_handle(conn_data);
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

void mb_conn_get_ssl_result(gpointer data, PurpleSslConnection * ssl, PurpleInputCondition cond)
{

	MbConnData * conn_data = data;
	MbAccount *ta = conn_data->ta;
	MbHttpData * response = conn_data->response;
	gint res, cur_error;
	gboolean call_handler = FALSE;
	
	purple_debug_info(MB_NET, "mb_conn_get_ssl_result, conn_data = %p\n", conn_data);

	//purple_debug_info(MB_NET, "new cur_result_pos = %d\n", tpd->cur_result_pos);
	res = mb_http_data_ssl_read(ssl, response);
	cur_error = errno;
	if( (res < 0) && (cur_error != EAGAIN) ) {
		purple_debug_info(MB_NET, "packet error\n");
		// error connecting or reading
		purple_input_remove(ssl->inpa);
		// First, chec if we already have everythings
		if(response->state == MB_HTTP_STATE_FINISHED) {
			purple_debug_info(MB_NET, "data is all here. state == finished\n");
			// All is fine, proceed to handler
			purple_input_remove(ssl->inpa);
			if(conn_data->ssl_conn_data) {
				g_hash_table_remove(ta->ssl_conn_hash, conn_data->ssl_conn_data);
				purple_ssl_close(conn_data->ssl_conn_data);
				conn_data->ssl_conn_data = NULL;			
			}
			call_handler = TRUE;
		} else {
			purple_debug_info(MB_NET, "really error\n");
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
		purple_debug_info(MB_NET, "connection closed, state = %d\n", response->state);
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
	purple_debug_info(MB_NET, "call_handler = %d\n", call_handler);
	if(call_handler) {
		// reassemble data
		//purple_debug_info(MB_NET, "got whole response = %s\n", response->content->str);
		if(conn_data->handler) {
			gint retval;
			
			purple_debug_info(MB_NET, "going to call handler\n");
			retval = conn_data->handler(conn_data, conn_data->handler_data);
			purple_debug_info(MB_NET, "handler returned, retval = %d\n", retval);
			if(retval == 0) {
				// Everything's good. Free data structure and go-on with usual works
				purple_debug_info(MB_NET, "everything's ok, freeing data\n");
				mb_conn_data_free(conn_data);
			} else if(retval == -1) {
				// Something's wrong. Requeue the whole process
				purple_debug_info(MB_NET, "handler return -1, retry again\n");
				mb_http_data_truncate(response);
				mb_conn_process_request(conn_data);
			}
		} // if handler != NULL
	}
	purple_debug_info(MB_NET, "end get_ssl_result\n");
}


