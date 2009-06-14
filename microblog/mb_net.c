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

#include <util.h>
#include <debug.h>
#include "mb_net.h"

// Fetch URL callback
static void mb_conn_fetch_url_cb(PurpleUtilFetchUrlData * url_data, gpointer user_data, const gchar * url_text, gsize len, const gchar * error_message);
 
MbConnData * mb_conn_data_new(MbAccount * ta, const gchar * host, gint port, MbHandlerFunc handler, gboolean is_ssl)
{
	MbConnData * conn_data = NULL;
	
	conn_data = g_new(MbConnData, 1);
	
	conn_data->host = g_strdup(host);
	conn_data->port = port;
	conn_data->ta = ta;
	conn_data->handler = handler;
	conn_data->handler_data = NULL;
	conn_data->retry = 0;
	conn_data->max_retry = 0;
	//conn_data->conn_data = NULL;
	conn_data->is_ssl = is_ssl;
	conn_data->request = mb_http_data_new();
	conn_data->response = mb_http_data_new();
	if(conn_data->is_ssl) {
		conn_data->request->proto = MB_HTTPS;
	} else {
		conn_data->request->proto = MB_HTTP;
	}

	conn_data->fetch_url_data = NULL;
	
	purple_debug_info(MB_NET, "new: create conn_data = %p\n", conn_data);
	return conn_data;
}

void mb_conn_data_free(MbConnData * conn_data)
{
	purple_debug_info(MB_NET, "free: conn_data = %p\n", conn_data);

	if(conn_data->fetch_url_data) {
		purple_util_fetch_url_cancel(conn_data->fetch_url_data);
	}

	if(conn_data->host) {
		purple_debug_info(MB_NET, "freeing host name\n");
		g_free(conn_data->host);
	}
	purple_debug_info(MB_NET, "freeing HTTP data->response\n");
	mb_http_data_free(conn_data->response);
	purple_debug_info(MB_NET, "freeing HTTP data->request\n");
	mb_http_data_free(conn_data->request);
	purple_debug_info(MB_NET, "freeing self at %p\n", conn_data);
	g_free(conn_data);
}

void mb_conn_data_set_retry(MbConnData * data, gint retry)
{
	data->max_retry = retry;
}

gchar * mb_conn_url_unparse(MbConnData * data)
{
	gchar port_str[20];

	if( ((data->port == 80) && !(data->is_ssl)) ||
		((data->port == 443) && (data->is_ssl)))
	{
		port_str[0] = '\0';
	} else {
		snprintf(port_str, 19, ":%hd", data->port);
	}

	// parameter is ignored here since we handle header ourself
	return g_strdup_printf("%s%s%s/%s", 
			data->is_ssl ? "https://" : "http://",
			data->host,
			port_str,
			data->request->path
		);
}


void mb_conn_fetch_url_cb(PurpleUtilFetchUrlData * url_data, gpointer user_data, const gchar * url_text, gsize len, const gchar * error_message)
{
	MbConnData * conn_data = (MbConnData *)user_data;
	MbAccount * ma = conn_data->ta;

	// in whatever situation, url_data should be handled only by libpurple
	conn_data->fetch_url_data = NULL;

	if(url_text) {
		mb_http_data_post_read(conn_data->response, url_text, len);
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
				conn_data->retry++;
				if(conn_data->retry <= conn_data->max_retry) {
					purple_debug_info(MB_NET, "handler return -1, retry %d\n", conn_data->retry);
					mb_http_data_truncate(conn_data->response);
					mb_conn_process_request(conn_data);
				} else {
					purple_debug_info(MB_NET, "retry exceed %d > %d\n", conn_data->retry, conn_data->max_retry);
					mb_conn_data_free(conn_data);
				}
			} 
		}
	} else {
		// XXX: Crash here too
		purple_connection_error(ma->gc, _(error_message));
		mb_conn_data_free(conn_data);
	}
}

void mb_conn_process_request(MbConnData * data)
{
	gchar * url;

	purple_debug_info(MB_NET, "NEW mb_conn_process_request, conn_data = %p\n", data);

	purple_debug_info(MB_NET, "connecting to %s on port %hd\n", data->host, data->port);

	url = mb_conn_url_unparse(data);

	// we manage user_agent by ourself so ignore this completely
	mb_http_data_prepare_write(data->request);
	data->retry = 0;
	data->fetch_url_data = purple_util_fetch_url_request(url, TRUE, "", TRUE, data->request->packet, TRUE, mb_conn_fetch_url_cb, (gpointer)data);
	g_free(url);
}
