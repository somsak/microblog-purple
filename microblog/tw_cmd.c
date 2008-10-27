/**
 * Command support for twitter
 */

#include <debug.h>
#include "tw_cmd.h"

#define DBGID "tw_cmd"

typedef struct {
	const gchar * cmd;
	const gchar * args;
	PurpleCmdPriority prio;
	PurpleCmdFlag flag;
	TwCmdFunc func;
	void * data;
	const gchar * help;
} TwCmdEnum ;

static PurpleCmdRet tw_cmd_replies(PurpleConversation * conv, const gchar * cmd, gchar ** args, gchar ** error, TwCmdArg * data);
static PurpleCmdRet tw_cmd_refresh(PurpleConversation * conv, const gchar * cmd, gchar ** args, gchar ** error, TwCmdArg * data);
static PurpleCmdRet tw_cmd_caller(PurpleConversation * conv, const gchar * cmd, gchar ** args, gchar ** error, void * data);

static TwCmdEnum tw_cmd_enum[] = {
	{"replies", "", PURPLE_CMD_P_PRPL, 0, tw_cmd_replies, NULL,
		"get replies timeline. /replies 10 to get latest 10 replies"},
	{"refresh", "w", PURPLE_CMD_P_PRPL, 0, tw_cmd_refresh, NULL,
		"refresh now. /refresh 120 temporarily change refresh rate to 120 seconds"},
};

PurpleCmdRet tw_cmd_replies(PurpleConversation * conv, const gchar * cmd, gchar ** args, gchar ** error, TwCmdArg * data)
{
	const gchar * path;
	TwitterTimeLineReq * tlr;
	int count;

	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	path = purple_account_get_string(data->ma->account, tc_name(TC_REPLIES_TIMELINE), tc_def(TC_REPLIES_TIMELINE));
	count = purple_account_get_int(data->ma->account, tc_name(TC_INITIAL_TWEET), tc_def_int(TC_INITIAL_TWEET));
	tlr = twitter_new_tlr(path, tc_def(TC_REPLIES_USER), TL_REPLIES, count);
	twitter_fetch_new_messages(data->ma, tlr);

	return PURPLE_CMD_RET_OK;
}


PurpleCmdRet tw_cmd_refresh(PurpleConversation * conv, const gchar * cmd, gchar ** args, gchar ** error, TwCmdArg * data)
{
	return PURPLE_CMD_RET_OK;
}

/*
 * Convenient proxy for calling real function
 */
PurpleCmdRet tw_cmd_caller(PurpleConversation * conv, const gchar * cmd, gchar ** args, gchar ** error, void * data)
{
	TwCmdArg * tca = data;

	purple_debug_info(DBGID, "%s called for cmd = %s\n", __FUNCTION__, cmd);
	tca->ma = conv->account->gc->proto_data;
	return tca->func(conv, cmd, args, error, tca);
}

TwCmd * tw_cmd_init(const char * protocol_id)
{
	int i, len = sizeof(tw_cmd_enum)/sizeof(TwCmdEnum);
	TwCmd * tw_cmd;
	
	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	tw_cmd = g_new(TwCmd, 1);
	tw_cmd->protocol_id = g_strdup(protocol_id);
	tw_cmd->cmd_id_num = len;
	tw_cmd->cmd_args = g_malloc0(sizeof(TwCmdArg *) * tw_cmd->cmd_id_num);
	tw_cmd->cmd_id = g_malloc(sizeof(PurpleCmdId) * tw_cmd->cmd_id_num);
	for(i = 0; i < len; i++) {
		tw_cmd->cmd_args[i] = g_new0(TwCmdArg, 1);
		tw_cmd->cmd_args[i]->func = tw_cmd_enum[i].func;
		tw_cmd->cmd_args[i]->data = tw_cmd_enum[i].data;
		tw_cmd->cmd_id[i] = purple_cmd_register(tw_cmd_enum[i].cmd,
				tw_cmd_enum[i].args,
				tw_cmd_enum[i].prio,
				tw_cmd_enum[i].flag | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PRPL_ONLY,
				protocol_id,
				tw_cmd_caller,
				tw_cmd_enum[i].help,
				tw_cmd->cmd_args[i]
		);
		purple_debug_info(DBGID, "command %s registered\n", tw_cmd_enum[i].cmd);
	}
	return tw_cmd;
}

void tw_cmd_finalize(TwCmd * tc)
{
	int i;

	purple_debug_info(DBGID, "%s called\n", __FUNCTION__);
	for(i = 0; i < tc->cmd_id_num; i++) {
		purple_cmd_unregister(tc->cmd_id[i]);
		g_free(tc->cmd_args[i]);
	}
	g_free(tc->protocol_id);
	g_free(tc);
}
