/*
=============================================
Webtokens Plugin
By: csnv
================================================
v1.0 Initial Release
Exposes webtoken to third party applications
*/
#include "common/hercules.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common/HPMi.h"
#include "common/mmo.h"
#include "common/socket.h"
#include "common/random.h"
#include "common/nullpo.h"
#include "common/strlib.h"
#include "common/showmsg.h"
#include "common/sql.h"
#include "common/timer.h"

#include "login/account.h"
#include "login/login.h"
#include "char/int_guild.h"
#include "char/mapif.h"
#include "map/battle.h"
#include "map/chrif.h"
#include "map/clif.h"
#include "map/guild.h"
#include "map/intif.h"
#include "map/pc.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

// Delay in milliseconds to wait before checking if the user is logged out
// before disabling its webtoken
const disable_token_timeout_ms = 60000;

HPExport struct hplugin_info pinfo = {
	"csnv_webtoken",			// Plugin name
	SERVER_TYPE_LOGIN | SERVER_TYPE_CHAR | SERVER_TYPE_MAP,	// Which server types this plugin works with?
	"1.0",				// Plugin version
	HPM_VERSION,		// HPM Version (don't change, macro is automatically updated)
};

// Ttoken generator
const unsigned char* gen_token(void)
{
	char values[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	static unsigned char buf[16] = { 0 };

	for (int i = 0; i < 16; ++i) {
		buf[i] = values[rnd() % 16];
	}

	// Ending byte
	buf[15] = '\0';

	return buf;
}

// Replacement for login->generate_token
static void login_generate_token_custom(struct login_session_data *sd, unsigned char *auth_token)
{
	AccountDB_SQL* db = (AccountDB_SQL*)login->accounts;
	const unsigned char *token = gen_token();

	if (SQL_SUCCESS == SQL->Query(db->accounts,
				"UPDATE `%s` SET `web_auth_token` = '%s', `web_auth_token_enabled` = '1' WHERE `account_id` = '%d'",
				db->account_db,
				token,
				sd->account_id)
	) {
		memcpy(auth_token, token, 16);
	}

	SQL->FreeResult(db->accounts);

}

// Remove all webtokens at startup
void remove_webtokens(void)
{
	AccountDB_SQL* db = (AccountDB_SQL*)login->accounts;

	if (SQL_ERROR == SQL->Query(db->accounts, "UPDATE `%s` SET `web_auth_token` = NULL, `web_auth_token_enabled` = '0'", db->account_db)) {
		Sql_ShowDebug(db->accounts);
		return;
	}

	SQL->FreeResult(db->accounts);
}

// Char receives guild emblem version changed packet from map
void map_to_char_emblem_changed(int fd)
{
	int guild_id = RFIFOL(fd, 2);
	int version = RFIFOL(fd, 6);

	struct guild *g = inter_guild->fromsql(guild_id);

	if (g == NULL)
		return;
	// Update in char
	g->emblem_id = version;
	g->emblem_len = 0;
	g->save_flag |= GS_EMBLEM;

	// Send back to map
	unsigned char buf[10];
	WBUFW(buf, 0) = 0x3841;
	WBUFL(buf, 2) = guild_id;
	WBUFL(buf, 6) = version;
	mapif->send(buf, 10);
}

// Char notifies map back
void char_to_map_emblem_changed(int fd)
{
	// Dummy guild emblem data
	char dummy_data[1] = {'0'};
	guild->emblem_changed(0, RFIFOL(fd, 2), RFIFOL(fd, 6), dummy_data);
}

// Client changed emblem, notify servers
void client_to_map_emblem_changed(int fd)
{
	struct map_session_data* sd = sockt->session[fd]->session_data;
	uint32 guild_id = RFIFOL(fd, 2);
	struct guild* g = sd->guild;

	if (g == NULL || g->guild_id != guild_id)
		return;

	if (!sd->state.gmaster_flag)
		return;

	uint32 version = RFIFOL(fd, 6);

	if (battle->bc->require_glory_guild &&
		!((g = sd->guild) && guild->checkskill(g, GD_GLORYGUILD) > 0)) {
		clif->skill_fail(sd, GD_GLORYGUILD, USESKILL_FAIL_LEVEL, 0, 0);
		return;
	}

	if (intif->CheckForCharServer())
		return;

	if (guild_id <= 0)
		return;

	// Notify char server
	WFIFOHEAD(chrif->fd, 10);
	WFIFOW(chrif->fd, 0) = 0x3042;
	WFIFOL(chrif->fd, 2) = guild_id;
	WFIFOL(chrif->fd, 6) = version;
	WFIFOSET(chrif->fd, 10);
}

// User logs in. Enable webtoken
static struct online_login_data* login_add_online_user_post(struct online_login_data* retVal, int char_server, int account_id)
{
	AccountDB_SQL* db = (AccountDB_SQL*)login->accounts;

	if (SQL_ERROR == SQL->Query(db->accounts, "UPDATE `%s` SET `web_auth_token_enabled` = '1' WHERE `account_id` = '%d'", db->account_db, account_id)) {
		Sql_ShowDebug(db->accounts);
		return retVal;
	}

	SQL->FreeResult(db->accounts);
	return retVal;
}

// Remove token when removing account
static void account_db_sql_destroy_post(AccountDB* self)
{
	AccountDB_SQL* db = (AccountDB_SQL*)self;

	if (SQL_ERROR == SQL->Query(db->accounts, "UPDATE `%s` SET `web_auth_token` = NULL", db->account_db)) {
		Sql_ShowDebug(db->accounts);
	}
	nullpo_retv(db);
	SQL->Free(db->accounts);
	db->accounts = NULL;
}

// Disable webtoken timer
static int login_remove_online_user_timer(int tid, int64 tick, int id, intptr_t data)
{
	AccountDB_SQL* db = (AccountDB_SQL*)login->accounts;
	struct online_login_data* p;
	p = (struct online_login_data*)idb_get(login->online_db, id);

	if (p == NULL) {
		return 0;
	}

	if (SQL_ERROR == SQL->Query(db->accounts, "UPDATE `%s` SET `web_auth_token_enabled` = '0' WHERE `account_id` = '%d'", db->account_db, id)) {
		Sql_ShowDebug(db->accounts);
		return 0;
	}
	SQL->FreeResult(db->accounts);

	return 1;
}

// Login removes user from pool, disable webtoken
static void login_remove_online_user_post(int account_id)
{
	timer->add(timer->gettick() + disable_token_timeout_ms, login_remove_online_user_timer, account_id, 0);
}

// Login server initialized
static void login_plugin_init(void)
{
	login->generate_token = login_generate_token_custom; // Replace generator function
	remove_webtokens(); // Remove all tokens on initialization

	addHookPost(login, add_online_user, login_add_online_user_post);
	addHookPost(login, remove_online_user, login_remove_online_user_post);
	addHookPost(account, db_sql_destroy, account_db_sql_destroy_post);
}

// Char server initialized
static void char_plugin_init(void)
{
	// Char notifies update to map
	addPacket(0x3042, 10, map_to_char_emblem_changed, hpParse_FromMap);
}

// Map server initialized
static void map_plugin_init(void)
{
	// Client notifies of emblem changed
	addPacket(0x0b46, 10, client_to_map_emblem_changed, hpClif_Parse);
	// Map notifies change to char
	addPacket(0x3841, 10, char_to_map_emblem_changed, hpChrif_Parse);
}

// Plugin initialization
HPExport void plugin_init(void)
{
#if PACKETVER >= 20190724
	switch (SERVER_TYPE) {
		case SERVER_TYPE_LOGIN: login_plugin_init(); break;
		case SERVER_TYPE_CHAR: char_plugin_init(); break;
		case SERVER_TYPE_MAP: map_plugin_init(); break;
		case SERVER_TYPE_UNKNOWN: break;
	}
#endif
}

// Show message on server startup
HPExport void server_online(void) {

#if PACKETVER >= 20190724
	ShowInfo("'%s' Plugin by csnv loaded. Version '%s' \n", pinfo.name, pinfo.version);
#endif
}
