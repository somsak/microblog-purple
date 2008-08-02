/**
  * HTTP data implementation
  */


#include "mb_http.h"

MbHttpData * mb_http_data_new(void)
{
	MbHttpData * data = g_new(MbHttpData, 1);
	
	// URL part, default to HTTP with port 80
	data->host = NULL;
	data->path = NULL;
	data->proto = MB_HTTP;
	data->port = 80;
	
	data->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	data->params = NULL;
	data->content = NULL;
	data->status = -1;
	data->type = HTTP_GET; //< default is get
	return data;
}
 void mb_http_data_free(MbHttpData * data) {
	if(data->host) {
		g_free(data->host);
	}
	if(data->path) {
		g_free(data->path);
	}
	if(data->headers) {
		g_hash_table_destroy(data->headers);
	}
	if(data->params) {
		GList * it;
		MbHttpParam * p;
		
		for(it = g_list_first(data->params); it; it = g_list_next(it)) {
			p = it->data;
			g_free(p->key);
			g_free(p->value);
			g_free(p);
		}
		g_list_free(data->params);
	}
	if(data->content) {
		g_string_free(data->content, TRUE);
	}
	g_free(data);
 }
 
 void mb_http_data_set_url(MbHttpData * data, const gchar * url)
 {
	gchar * tmp_url = g_strdup(url);
	gchar * cur_it = NULL, *tmp_it = NULL, *host_and_port = NULL;
	
	cur_it = tmp_url;
	for(;;) {
		// looking for ://
		if( (tmp_it = strstr(cur_it, "://")) == NULL) {
			break;
		}
		(*tmp_it) = '\0';
		if(strcmp(cur_it, "http") == 0) {
			data->proto = MB_HTTP;
		} else if(strcmp(cur_it, "https") == 0) {
			data->proto = MB_HTTPS;
		} else {
			data->proto = MB_PROTO_UNKNOWN;
		}
		cur_it = tmp_it + 3;
		
		//now looking for host part
		if( (tmp_it = strchr(cur_it, '/')) == NULL) {
			break;
		}
		(*tmp_it) = '\0';
		host_and_port = cur_it;
		cur_it = tmp_it; //< save cur_it for later use
		if( (tmp_it = g_strrstr(host_and_port, ":")) == NULL) {
			// host without port
			if(data->host) g_free(data->host);
			data->host = g_strdup(host_and_port);
			switch(data->proto) {
				case MB_HTTP :
					data->port = 80;
					break;
				case MB_HTTPS :
					data->port = 443;
					break;
				default :
					data->port = 80; //< all other protocol assume that it's http, caller should specified port
					break;
			}
		} else {
			(*tmp_it) = '\0';
			if(data->host) g_free(data->host);
			data->host = g_strdup(host_and_port);
			data->port = (gint)strtoul(tmp_it + 1, NULL, 10);
		}
		
		// now the path
		(*cur_it) = '/';
		if(data->path) g_free(data->path);
		data->path = g_strdup(cur_it);
	
		break;
	}

	g_free(tmp_url);
 }
 
 void mb_http_data_get_url(MbHttpData * data, gchar * url, gint url_len)
 {
	gchar proto_str[10];
	if(data->proto == MB_HTTP) {
		strcpy(proto_str, "http");
	} else if(data->proto == MB_HTTPS) {
		strcpy(proto_str, "https");
	} else {
		strcpy(proto_str, "unknown");
	}
	snprintf(url, url_len, "%s://%s:%d%s", proto_str, data->host, data->port, data->path);
 }
 
 /*
 
 gint mb_http_data_ssl_read(PurpleSslConnection * ssl, MbHttpData * data)
 {
 }
 gint mb_http_data_ssl_write(PurpleSslConnection * ssl, MbHttpData * data)
 {
 }
 
 extern gint mb_http_data_set_header(MbHttpData* data, gchar * key, gchar * value);
 extern gchar * mb_http_data_get_header(MbHttpData * data, gchar * key);
 
 // function below might be static instead
 extern MbHttpParam * mb_http_init_param(void);
 extern void mb_http_free_param(MbHttpParam * param);
 
// Params is duplicable! Upto 
extern void mb_http_data_add_param(MbHttpData * data, gchar * key, gchar * value);
extern void mb_http_data_rm_param(MbHttpData * data, gchar * key);

// generic utility
extern gbool mb_http_data_ok(MbHttpData * data);
*/

#ifdef UTEST

int main(int argc, char * argv[])
{
	MbHttpData * hdata = mb_http_data_new();
	
	mb_http_data_set_url(hdata, "https://twitter.com/statuses/friends_timeline.xml");
	
	printf("host = %s\n", hdata->host);
	printf("port = %d\n", hdata->port);
	printf("proto = %d\n", hdata->proto);
	printf("path = %s\n", hdata->path);
	
	mb_http_data_set_url(hdata, "http://twitter.com/statuses/update.atom");
	
	printf("host = %s\n", hdata->host);
	printf("port = %d\n", hdata->port);
	printf("proto = %d\n", hdata->proto);
	printf("path = %s\n", hdata->path);
	
	mb_http_data_free(hdata);
	
	return 0;
}
#endif
