/**
 * Command handler for twitter-based protocol
 */

#ifndef __TW_CMD__
#define __TW_CMD__

#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <time.h>

#ifndef G_GNUC_NULL_TERMINATED
#  if __GNUC__ >= 4
#    define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#  else
#    define G_GNUC_NULL_TERMINATED
#  endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#include <sslconn.h>
#include <cmds.h>

#include "twitter.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _TwCmdArg;

typedef PurpleCmdRet (* TwCmdFunc)(PurpleConversation *, const gchar *, gchar **, gchar **, struct _TwCmdArg *);

typedef struct _TwCmdArg {
	MbAccount * ma;
	TwCmdFunc func;
	void * data;
} TwCmdArg;

typedef struct {
	char * protocol_id;
	PurpleCmdId * cmd_id;
	TwCmdArg ** cmd_args;
	int cmd_id_num;
} TwCmd;


extern TwCmd * tw_cmd_init(const char * protocol_id);
extern void tw_cmd_finalize(TwCmd * tc);


#ifdef __cplusplus
}
#endif

#endif
