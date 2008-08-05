/**
* HTTP data implementation
*/

#include <stdio.h>
#include <ctype.h>

#include <purple.h>

#include "mb_http.h"

// function below might be static instead
static MbHttpParam * mb_http_param_new(void)
{
	MbHttpParam * p = g_new(MbHttpParam, 1);
	
	p->key = NULL;
	p->value = NULL;
	return p;
}
static void mb_http_param_free(MbHttpParam * param)
{
	if(param->key) g_free(param->key);
	if(param->value) g_free(param->value);
	g_free(param);
}

static guint mb_strnocase_hash(gconstpointer a)
{
	gint len = strlen(a);
	gint i;
	gchar * tmp = g_strdup(a);
	guint retval;
	
	for(i = 0; i < len; i++) {
		tmp[i] = tolower(tmp[i]);
	}
	retval = g_str_hash(tmp);
	g_free(tmp);
	return retval;
}

static gboolean mb_strnocase_equal(gconstpointer a, gconstpointer b)
{
	if(strcasecmp((const gchar *)a, (const gchar *)b) == 0) {
		return TRUE;
	}
	return FALSE;
}

MbHttpData * mb_http_data_new(void)
{
	MbHttpData * data = g_new(MbHttpData, 1);
	
	// URL part, default to HTTP with port 80
	data->host = NULL;
	data->path = NULL;
	data->proto = MB_HTTP;
	data->port = 80;
	
	data->headers = g_hash_table_new_full(mb_strnocase_hash, mb_strnocase_equal, g_free, g_free);
	data->params = NULL;
	data->params_len = 0;
	data->content = NULL;
	data->content_len = 0;
	data->status = -1;
	data->type = HTTP_GET; //< default is get
	data->state = MB_HTTP_STATE_INIT;
	
	data->packet = NULL;
	data->cur_packet = NULL;
	
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
	data->headers_len = 0;
	
	if(data->params) {
		GList * it;
		MbHttpParam * p;
		
		for(it = g_list_first(data->params); it; it = g_list_next(it)) {
			p = it->data;
			mb_http_param_free(p);
		}
		g_list_free(data->params);
	}
	if(data->content) {
		g_string_free(data->content, TRUE);
	}
	
	if(data->packet) {
		g_free(data->packet);
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

void mb_http_data_set_path(MbHttpData * data, const gchar * path)
{
	if(data->path) {
		g_free(data->path);
	}
	data->path = g_strdup(path);
}

void mb_http_data_set_content(MbHttpData * data, const gchar * content)
{
	if(data->content) {
		g_string_truncate(data->content, 0);
	} else {
		data->content = g_string_new(content);
	}
}

void mb_http_data_set_header(MbHttpData* data, const gchar * key, const gchar * value)
{
	gint len = strlen(key) + strlen(value) + 10; //< pad ':' and null terminate string and \r\n

	g_hash_table_insert(data->headers, g_strdup(key), g_strdup(value));
	data->headers_len += len;
}

gchar * mb_http_data_get_header(MbHttpData * data, const gchar * key)
{
	return (gchar *)g_hash_table_lookup(data->headers, key);
}

static gint mb_http_data_param_key_pred(gconstpointer a, gconstpointer key)
{
	const MbHttpParam * p = (const MbHttpParam *)a;
	
	if(strcmp(p->key, (gchar *)key) == 0) {
		return 0;
	}
	return -1;
}

void mb_http_data_add_param(MbHttpData * data, const gchar * key, const gchar * value)
{
	MbHttpParam * p = mb_http_param_new();
	
	p->key = g_strdup(purple_url_encode(key));
	p->value = g_strdup(purple_url_encode(value));
	data->params = g_list_append(data->params, p);
	data->params_len += strlen(p->key) + strlen(p->value) + 2; //< length of key + value + "& or ?"
}

const gchar * mb_http_data_find_param(MbHttpData * data, const gchar * key)
{
	GList * retval;
	MbHttpParam * p;
	
	retval = g_list_find_custom(data->params, key, mb_http_data_param_key_pred);
	
	if(retval) {
		p = retval->data;
		return p->value;
	} else {
		return NULL;
	}
}

gboolean mb_http_data_rm_param(MbHttpData * data, const gchar * key)
{
	MbHttpParam * p;
	GList *it, *found = NULL;
	
	for(it = g_list_first(data->params); it; it = g_list_next(it)) {
		p = it->data;
		if(strcmp(p->key, key) == 0) {
			found = it;
			break;
		}
	}
	if(found) {
		p = found->data;
		data->params_len -= strlen(p->key) + strlen(p->value) - 2;
		data->params = g_list_delete_link(data->params, found);
		mb_http_param_free(p);
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
// generic utility
extern gbool mb_http_data_ok(MbHttpData * data);
*/

static void mb_http_data_header_assemble(gpointer key, gpointer value, gpointer udata)
{
	MbHttpData * data = udata;
	gint len;
	
	len = sprintf(data->cur_packet, "%s: %s\r\n", key, value);
	data->cur_packet += len;
}

static void mb_http_data_prepare_write(MbHttpData * data)
{
	GList * it;
	MbHttpParam * p;
	gchar * cur_packet;
	gint packet_len, len;

	if(data->path == NULL) return;

	// assemble all headers
	// I don't sure how hash table will behave, so assemple everything should be better
	packet_len = data->headers_len + data->params_len + strlen(data->path) + 100; //< for \r\n\r\n and GET|POST and other stuff
	if(data->content) {
		packet_len += data->content->len;
	}
	data->packet = g_malloc0(packet_len);
	cur_packet = data->packet;
	
	// GET|POST and parameter part
	if(data->type == HTTP_GET) {
		len = sprintf(cur_packet, "GET %s", data->path);
	} else {
		len = sprintf(cur_packet, "POST %s", data->path);
	}
	//printf("cur_packet = %s\n", cur_packet);
	cur_packet += len;
	if(data->params) {
		(*cur_packet) = '?';
		cur_packet++;
		for(it = g_list_first(data->params); it; it = g_list_next(it)) {
			p = it->data;
			len = sprintf(cur_packet, "%s=%s&", p->key, p->value);
			cur_packet += len;
		}
		cur_packet--;
	}
	(*cur_packet) = ' ';
	len = sprintf(cur_packet, " HTTP/1.1\r\n");
	cur_packet += len;
	
	// headers part
	data->cur_packet = cur_packet;
	g_hash_table_foreach(data->headers, mb_http_data_header_assemble, data);
	cur_packet = data->cur_packet;
	
	// content-length, if needed
	if(data->content) {
		len = sprintf(cur_packet, "Content-Length: %d\r\n", data->content->len);
		cur_packet += len;
	}
	
	// end header part
	len = sprintf(cur_packet, "\r\n\r\n");
	cur_packet += len;
	
	// Content part
	if(data->content) {
		sprintf(cur_packet, "%s", data->content->str);
		cur_packet += len;
	}
	
	data->packet_len = cur_packet - data->packet;
	
	// reset back to head of packet, ready to transfer
	data->cur_packet = data->packet;
	
	//printf("current data = #%s#\n", data->packet);
}

static void mb_http_data_post_read(MbHttpData * data, gchar * buf, gint buf_len)
{
	gint packet_len = (buf_len > MB_MAXBUFF) ? buf_len : MB_MAXBUFF;
	gint cur_pos_len, whole_len;
	gchar * delim, * cur_pos, *content_start = NULL;
	gchar * key, *value, *key_value_sep;
	
	if(buf_len <= 0) return;
	switch(data->state) {
		case MB_HTTP_STATE_INIT :
			if(data->packet) {
				g_free(data->packet);
			}
			data->packet = g_malloc0(packet_len);
			data->packet_len = packet_len;
			data->cur_packet = data->packet;
			data->state = MB_HTTP_STATE_HEADER;
			//break; //< purposely move to next step
		case MB_HTTP_STATE_HEADER :	
			//printf("processing header\n");
			// at this state, no data at all, so this should be the very first chunk of data
			// reallocate buffer if we don't have enough
			cur_pos_len = data->cur_packet - data->packet;
			if( (data->packet_len - cur_pos_len) < buf_len) {
				//printf("reallocate buffer\n");
				data->packet_len += (buf_len * 2);
				data->packet = g_realloc(data->packet, data->packet_len);
				data->cur_packet = data->packet + cur_pos_len;
			}
			memcpy(data->cur_packet, buf, buf_len);
			whole_len = (data->cur_packet - data->packet) + buf_len;
			
			// decipher header
			cur_pos = data->packet;
			//printf("initial_cur_pos = #%s#\n", cur_pos);
			while( (delim = strstr(cur_pos, "\r\n")) != NULL) {
				if( strncmp(delim, "\r\n\r\n", 4) == 0 ) {
					// we reach the content now
					content_start = delim + 4;
					//printf("found content = %s\n", content_start);
				}
				(*delim) = '\0';
				//printf("cur_pos = %s\n", cur_pos);
				if(strncmp(cur_pos, "HTTP/1.1", 8) == 0) {
					// first line
					data->status = (gint)strtoul(&cur_pos[9], NULL, 10);
					//printf("data->status = %d\n", data->status);
				} else {
					//Header line
					if( (key_value_sep = strchr(cur_pos, ':')) != NULL) {
						//printf("header line\n");
						(*key_value_sep) = '\0';
						key = cur_pos;
						value = key_value_sep + 1;
						key = g_strstrip(key);
						value = g_strstrip(value);
						
						if(strcasecmp(key, "Content-Length") == 0) {
							data->content_len = (gint)strtoul(value, NULL, 10);
						}
						mb_http_data_set_header(data, key, value);
					} else {
						// invalid header?
						// do nothing for now
						purple_debug_info("microblog", "an invalid line? line = #%s#", cur_pos);
					}
				}
				if(content_start) {
					break;
				}
				cur_pos = delim + 2;
			}
			if(content_start) {
				// copy the rest of data as content and free everything
				if(data->content != NULL) {
					g_string_free(data->content, TRUE);
				}
				data->content = g_string_new_len(content_start, whole_len - (content_start - data->packet));
				g_free(data->packet);
				data->cur_packet = data->packet = NULL;
				data->packet_len = 0;
				data->state = MB_HTTP_STATE_CONTENT;
			} else {			
				// copy the rest of string to the beginning
				if( (cur_pos - data->packet) < whole_len) {
					gint tmp_len = whole_len - (cur_pos - data->packet);
					gchar * tmp = g_malloc(tmp_len);
					
					memcpy(tmp, cur_pos, tmp_len);
					memcpy(data->packet, tmp, tmp_len);
					g_free(tmp);
					
					data->cur_packet = data->packet + tmp_len;
				}
			}
			break;
		case MB_HTTP_STATE_CONTENT :
			g_string_append_len(data->content, buf, buf_len);
			if(data->content->len >= data->content_len) {
				data->state = MB_HTTP_STATE_FINISHED;
			}
			break;
		case MB_HTTP_STATE_FINISHED :
			break;
	}
}

void mb_http_data_set_basicauth(MbHttpData * data, const gchar * user, const gchar * passwd)
{
	gchar * merged_tmp, *encoded_tmp, *value_tmp;
	gsize authen_len;
	
	authen_len = strlen(user) + strlen(passwd) + 1;
	merged_tmp = g_strdup_printf("%s:%s", user, passwd);
	encoded_tmp = purple_base64_encode((const guchar *)merged_tmp, authen_len);
	//g_strlcpy(output, encoded_temp, len);
	g_free(merged_tmp);
	value_tmp = g_strdup_printf("Basic %s", encoded_tmp);
	g_free(encoded_tmp);
	mb_http_data_set_header(data, "Authorization", value_tmp);
	g_free(value_tmp);
}

gint mb_http_data_ssl_read(PurpleSslConnection * ssl, MbHttpData * data)
{
	
}

gint mb_http_data_ssl_write(PurpleSslConnection * ssl, MbHttpData * data)
{
	gint retval;
	
	if(data->packet == NULL) {
		mb_http_data_prepare_write(data);
	}
	// Do SSL-write, then update cur_packet to proper position. Exit if already exceeding the length
	retval = purple_ssl_write(ssl, data->cur_packet, MB_MAXBUFF);
	if(retval >= data->packet_len) {
		// everything is written
		g_free(data->packet);
		data->cur_packet = data->packet = NULL;
		data->packet_len = 0;
		return retval;
	} else if( (retval > 0) && (retval < data->packet_len)) {
		data->cur_packet = data->cur_packet + retval;
	}
	return retval;
}

#ifdef UTEST

static void print_hash_value(gpointer key, gpointer value, gpointer udata)
{
	printf("key = %s, value = %s\n", (char *)key, (char *)value);
}

int main(int argc, char * argv[])
{
	MbHttpData * hdata = NULL;
	GList * it;
	char buf[512];
	FILE * fp;
	size_t retval;
	
	g_mem_set_vtable(glib_mem_profiler_table);
	// URL

	hdata = mb_http_data_new();
	mb_http_data_set_url(hdata, "https://twitter.com/statuses/friends_timeline.xml");
	printf("URL to set = %s\n", "https://twitter.com/statuses/friends_timeline.xml");
	printf("host = %s\n", hdata->host);
	printf("port = %d\n", hdata->port);
	printf("proto = %d\n", hdata->proto);
	printf("path = %s\n", hdata->path);
	

	mb_http_data_set_url(hdata, "http://twitter.com/statuses/update.atom");
	printf("URL to set = %s\n", "http://twitter.com/statuses/update.atom");
	printf("host = %s\n", hdata->host);
	printf("port = %d\n", hdata->port);
	printf("proto = %d\n", hdata->proto);
	printf("path = %s\n", hdata->path);
	
	// header
	mb_http_data_set_header(hdata, "User-Agent", "CURL");
	mb_http_data_set_header(hdata, "X-Twitter-Client", "1024");
	mb_http_data_set_basicauth(hdata, "user1", "passwd1");
	printf("Header %s = %s\n", "User-Agent", mb_http_data_get_header(hdata, "User-Agent"));
	printf("Header %s = %s\n", "Content-Length", mb_http_data_get_header(hdata, "X-Twitter-Client"));
	printf("Header %s = %s\n", "XXX", mb_http_data_get_header(hdata, "XXX"));
	

	// param
	mb_http_data_add_param(hdata, "key1", "valuevalue1");
	mb_http_data_add_param(hdata, "key2", "valuevalue1 bcadf");
	mb_http_data_add_param(hdata, "key1", "valuevalue1");
	
	printf("Param %s = %s\n", "key1", mb_http_data_find_param(hdata, "key1"));
	printf("Param %s = %s\n", "key2", mb_http_data_find_param(hdata, "key2"));
	
	for(it = g_list_first(hdata->params); it; it = g_list_next(it)) {
		MbHttpParam * p = it->data;
		
		printf("Key = %s, Value = %s\n", p->key, p->value);
	}

	printf("Before remove\n");
	mb_http_data_rm_param(hdata, "key1");

	printf("After remove\n");
	for(it = g_list_first(hdata->params); it; it = g_list_next(it)) {
		MbHttpParam * p = it->data;
		
		printf("Key = %s, Value = %s\n", p->key, p->value);
	}
	printf("data->path = %s\n", hdata->path);
	mb_http_data_prepare_write(hdata);
	
	printf("data prepared to write\n");
	printf("%s\n", hdata->packet);
	printf("packet_len = %d, strlen = %d\n", hdata->packet_len, strlen(hdata->packet));
//	printf("%s\n", buf);
	mb_http_data_free(hdata);
	hdata =mb_http_data_new();

	fp = fopen("input1-2.xml", "r");
	while(!feof(fp)) {
		retval = fread(buf, sizeof(char), sizeof(buf), fp);
		mb_http_data_post_read(hdata, buf, retval);
	}
	fclose(fp);
	printf("http status = %d\n", hdata->status);
	g_hash_table_foreach(hdata->headers, print_hash_value, NULL);
	printf("http content length = %d\n", hdata->content_len);
	printf("http content = %s\n", hdata->content->str);
	mb_http_data_free(hdata);

	g_mem_profile();
	
	return 0;
}
#endif
