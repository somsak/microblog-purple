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

#ifdef __cplusplus
}
#endif

#endif
