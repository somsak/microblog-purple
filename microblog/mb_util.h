/**
 *  Utility for microblog plug-in
 */
#ifndef __MB_UTIL__
#define __MB_UTIL__

#ifdef __cplusplus
extern "C" {
#endif

#include "account.h"

extern const char * mb_get_uri_txt(PurpleAccount * pa);
extern time_t mb_mktime(char * time_str);
extern void mb_account_set_ull(PurpleAccount * account, const char * name, unsigned long long value);
extern unsigned long long mb_account_get_ull(PurpleAccount * account, const char * name, unsigned long long default_value);
extern void mb_account_set_idhash(PurpleAccount * account, const char * name, GHashTable * id_hash);
extern void mb_account_get_idhash(PurpleAccount * account, const char * name, GHashTable * id_hash);
extern gchar * mb_url_unparse(const char * host, int port, const char * path, const char * params, gboolean use_https);

#ifdef __cplusplus
}
#endif

#endif
