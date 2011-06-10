
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/


#ifdef WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
};

#include <signal.h>
#include "crosslib.h"
#include "log.h"
#include "idmgr.h"
#include "lxnet.h"
#include "msgbase.h"

using namespace lxnet;

static int lualxnet_init(lua_State *L)
{
	int smallbufsize = (int)luaL_checkinteger(L, 1);
	luaL_argcheck(L, smallbufsize > 256, 1, "size is must greater than 256");
	if (smallbufsize <= 256)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int smallbufnum = (int)luaL_checkinteger(L, 2);
	luaL_argcheck(L, smallbufnum >= 1, 1, "buf num is must greater than 1");
	if (smallbufnum < 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int listenernum = (int)luaL_checkinteger(L, 3);
	luaL_argcheck(L, listenernum >= 1, 1, "listen object num is must greater than 1");
	if (listenernum < 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int socketnum = (int)luaL_checkinteger(L, 4);
	luaL_argcheck(L, socketnum >= 1, 1, "socket object num is must greater than 1");
	if (listenernum < 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int threadnum = (int)luaL_checkinteger(L, 5);
	lua_pushboolean(L, net_init(512, 1, smallbufsize, smallbufnum, listenernum, socketnum, threadnum));
	return 1;
}

static int lualxnet_release(lua_State *L)
{
	net_release();
	return 0;
}

static int lualxnet_run(lua_State *L)
{
	net_run();
	return 0;
}

static int lualxnet_meminfo(lua_State *L)
{
	lua_pushstring(L, net_memory_info());
	return 1;
}

static const struct luaL_reg class_lxnet_function[] = {
	{"init", lualxnet_init},
	{"release", lualxnet_release},
	{"run", lualxnet_run},
	{"meminfo", lualxnet_meminfo},
	{0, 0}
};

static int lua_delay (lua_State *L)
{
	int millisecond = (int)luaL_checkinteger(L, 1);
	luaL_argcheck(L, millisecond >= 0, 1, "argv must >= 0");

	delay_delay(millisecond);
	return 0;
}

static int lua_get_millisecond (lua_State *L)
{
	lua_pushnumber(L, (lua_Number )get_millisecond());
	return 1;
}

static int lua_getfullname (lua_State *L)
{
	char buf[1024*2] = {0};
	const char *name = luaL_checkstring(L, 1);
#ifdef WIN32
	GetFullPathName(name, sizeof(buf)-1, buf, NULL);
#else
	int fd = open(name, O_RDWR);
	char srcname[1024*2];
	snprintf(srcname, sizeof(srcname)-1, "/proc/%ld/fd/%d", (long)getpid(), fd);
	readlink(srcname, buf, sizeof(buf)-1);
#endif
	lua_pushstring(L, buf);
	return 1;
}

static int lua_log_setdirectory (lua_State *L)
{
	const char *directory = luaL_checkstring(L ,1);
	log_setdirect(directory);
	return 0;
}

static int lua_log_writelog (lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	log_writelog("%s", str);
	return 0;
}

static int lua_log_error (lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	log_error("%s", str);
	return 0;
}

static const struct luaL_reg g_function[] = {
	{"delay", lua_delay},
	{"getmillisecond", lua_get_millisecond},
	{"getfilefullname", lua_getfullname},
	{"log_setdirectory", lua_log_setdirectory},
	{"log_writelog", lua_log_writelog},
	{"log_error", lua_log_error},
	{0, 0}
};

static int lualistener_create (lua_State *L)
{
	Listener *lis = Listener_create();
	if (!lis)
		lua_pushnil(L);
	else
		lua_pushnumber(L, (lua_Number )(intptr_t )lis);
	return 1;
}

static Listener *get_listener (lua_State *L, int idx)
{
	Listener *lis = (Listener *)(intptr_t)luaL_checknumber(L, idx);
	luaL_argcheck(L, lis != NULL, idx, "listener expected");
	return lis;
}

static int lualistener_release (lua_State *L)
{
	Listener *lis = get_listener(L, 1);
	Listener_release(lis);
	return 0;
}

static int lualistener_listen (lua_State *L)
{
	Listener *lis = get_listener(L, 1);
	int port = luaL_checkinteger(L, 2);
	int backlog = luaL_optinteger(L, 3, 5);
	lua_pushboolean(L, lis->Listen(port, backlog));
	return 1;
}

static int lualistener_close (lua_State *L)
{
	Listener *lis = get_listener(L, 1);
	lis->Close();
	return 0;
}

static int lualistener_isclose (lua_State *L)
{
	Listener *lis = get_listener(L, 1);
	lua_pushboolean(L, lis->IsClose());
	return 1;
}

static int lualistener_accept (lua_State *L)
{
	Listener *lis = get_listener(L, 1);
	Socketer *sock = lis->Accept();
	if (!sock)
		lua_pushnil(L);
	else
		lua_pushnumber(L, (lua_Number )(intptr_t )sock);
	return 1;
}

static int lualistener_canaccept (lua_State *L)
{
	Listener *lis = get_listener(L, 1);
	lua_pushboolean(L, lis->CanAccept());
	return 1;
}

static const struct luaL_reg class_listener_function[] = {
	{"create", lualistener_create},
	{"release", lualistener_release},
	{"listen", lualistener_listen},
	{"close", lualistener_close},
	{"isclose", lualistener_isclose},
	{"accept", lualistener_accept},
	{"canaccept", lualistener_canaccept},
	{0, 0}
};

static int luasocketer_create (lua_State *L)
{
	Socketer *sock = Socketer_create();
	if (!sock)
		lua_pushnil(L);
	else
		lua_pushnumber(L, (lua_Number )(intptr_t )sock);
	return 1;
}

static Socketer *get_socketer (lua_State *L, int idx)
{
	Socketer *sock = (Socketer *)(intptr_t)luaL_checknumber(L, idx);
	luaL_argcheck(L, sock != NULL, idx, "socketer expected");
	return sock;
}

static int luasocketer_release (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	Socketer_release(sock);
	return 0;
}

static int luasocketer_setrecvlimit (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	int recvlimit = luaL_checkinteger(L, 2);
	sock->SetRecvCritical(recvlimit);
	return 0;
}

static int luasocketer_setsendlimit (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	int sendlimit = luaL_checkinteger(L, 2);
	sock->SetSendCritical(sendlimit);
	return 0;
}

static int luasocketer_use_compress (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	sock->UseCompress();
	return 0;
}

static int luasocketer_use_uncompress (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	sock->UseUncompress();
	return 0;
}

static int luasocketer_use_encrypt (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	sock->UseEncrypt();
	return 0;
}

static int luasocketer_use_decrypt (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	sock->UseDecrypt();
	return 0;
}

static int luasocketer_close (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	sock->Close();
	return 0;
}

static int luasocketer_connect (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	const char *ip = luaL_checkstring(L, 2);
	int port = luaL_checkinteger(L, 3);
	luaL_argcheck(L, port >= 0, 3, "port need greater than or equal to zero");
	lua_pushboolean(L, sock->Connect(ip, port));
	return 1;
}

static int luasocketer_isclose (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	lua_pushboolean(L, sock->IsClose());
	return 1;
}

static int luasocketer_getip (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	char ip[128];
	sock->GetIP(ip, sizeof(ip));
	ip[sizeof(ip)-1] = 0;
	lua_pushstring(L, ip);
	return 1;
}

static int luasocketer_sendmsg (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	Msg *pMsg = (Msg *)(intptr_t)luaL_checknumber(L, 2);
	lua_pushboolean(L, sock->SendMsg(pMsg));
	return 1;
}

static int luasocketer_getmsg (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	Msg *pMsg = sock->GetMsg();
	if (!pMsg)
		lua_pushnil(L);
	else
		lua_pushnumber(L, (lua_Number )(intptr_t )pMsg);
	return 1;
}

static int luasocketer_realsend (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	sock->CheckSend();
	return 0;
}

static int luasocketer_realrecv (lua_State *L)
{
	Socketer *sock = get_socketer(L, 1);
	sock->CheckRecv();
	return 0;
}

static const struct luaL_reg class_socketer_function[] = {
	{"create", luasocketer_create},
	{"release", luasocketer_release},
	{"setrecvlimit", luasocketer_setrecvlimit},
	{"setsendlimit", luasocketer_setsendlimit},
	{"use_compress", luasocketer_use_compress},
	{"use_uncompress", luasocketer_use_uncompress},
	{"use_encrypt", luasocketer_use_encrypt},
	{"use_decrypt", luasocketer_use_decrypt},
	{"close", luasocketer_close},
	{"connect", luasocketer_connect},
	{"isclose", luasocketer_isclose},
	{"getip", luasocketer_getip},
	{"sendmsg", luasocketer_sendmsg},
	{"getmsg", luasocketer_getmsg},
	{"realsend", luasocketer_realsend},
	{"realrecv", luasocketer_realrecv},
	{0, 0}
};

static MessagePack s_anyfirst;
static MessagePack s_anysecond;

static int luapacket_anyfirst (lua_State *L)
{
	MessagePack *pack = &s_anyfirst;
	pack->ResetMsgLength();
	lua_pushnumber(L, (lua_Number)(intptr_t)pack);
	return 1;
}

static int luapacket_anysecond (lua_State *L)
{
	MessagePack *pack = &s_anysecond;
	pack->ResetMsgLength();
	lua_pushnumber(L, (lua_Number)(intptr_t)pack);
	return 1;
}

static MessagePack *get_messagepack (lua_State *L, int idx)
{
	MessagePack *pack = (MessagePack *)(intptr_t)luaL_checknumber(L, idx);
	luaL_argcheck(L, pack != NULL, idx, "packet expected");
	return pack;
}

static int luapacket_reset (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	pack->ResetMsgLength();
	return 0;
}

static int luapacket_getlen (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_pushinteger(L, (lua_Integer)pack->GetLength());
	return 1;
}

static int luapacket_gettype (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_pushinteger(L, (lua_Integer)pack->GetType());
	return 1;
}

static int luapacket_settype (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int type = luaL_checkinteger(L, 2);
	pack->SetType(type);
	return 0;
}

static int luapacket_pushstring (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	const char *str = luaL_checkstring(L, 2);
	pack->PushString(str);
	return 0;
}

static int luapacket_getstring (lua_State *L)
{
#define MAX_STRING_SIZE (31*1024)
	static char buf[33*1024];
	assert(MAX_STRING_SIZE <= sizeof(buf));

	MessagePack *pack = get_messagepack(L, 1);
	int16 needread = luaL_optinteger(L, 2, MAX_STRING_SIZE);
	luaL_argcheck(L, needread > 0, 2, "getstring, size need greater than zero");
	if (needread <= 0 || needread > MAX_STRING_SIZE)
		needread = MAX_STRING_SIZE;
	pack->GetString(buf, needread);
	lua_pushstring(L, buf);
	return 1;
}

static int luapacket_begin (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	pack->Begin();
	return 0;
}

static int luapacket_pushboolean (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	luaL_argcheck(L, lua_isboolean(L, 2), 2, "boolean expected");
	lua_Integer value = lua_toboolean(L, 2);
	pack->PushInt8((int8)value);
	return 0;
}

static int luapacket_getboolean (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_pushboolean(L, (lua_Integer)pack->GetInt8());
	return 1;
}

static int luapacket_pushint8 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_Integer value = luaL_checkinteger(L, 2);
	pack->PushInt8((int8)value);
	return 0;
}

static int luapacket_getint8 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_pushinteger(L, (lua_Integer)pack->GetInt8());
	return 1;
}

static int luapacket_pushint16 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_Integer value = luaL_checkinteger(L, 2);
	pack->PushInt16((int16)value);
	return 0;
}

static int luapacket_getint16 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_pushinteger(L, (lua_Integer)pack->GetInt16());
	return 1;
}

static int luapacket_pushint32 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_Integer value = luaL_checkinteger(L, 2);
	pack->PushInt32((int32)value);
	return 0;
}

static int luapacket_getint32 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_pushinteger(L, (lua_Integer)pack->GetInt32());
	return 1;
}

static int luapacket_pushint64 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_Number value = luaL_checknumber(L, 2);
	pack->PushInt64((int64)value);
	return 0;
}

static int luapacket_getint64 (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	lua_pushnumber(L, (lua_Number)pack->GetInt64());
	return 1;
}

static int luapacket_pushint16toindex (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int index = luaL_checkinteger(L, 2);
	luaL_argcheck(L, index >= 0, 2, "pushint16toindex, index need greater than or equal to zero");
	int16 value = (int16)luaL_checkinteger(L, 3);
	pack->PutDataNotAddLength(index, &value, sizeof(value));
	return 0;
}

static int luapacket_push32k (lua_State *L)
{
	char buf[32*1024];
	MessagePack *pack = get_messagepack(L, 1);
	pack->PushBlock(buf, sizeof(buf));
	return 0;
}

static const struct luaL_reg class_packet_function[] = {
	{"anyfirst", luapacket_anyfirst},
	{"anysecond", luapacket_anysecond},

	{"reset", luapacket_reset},
	{"getlen", luapacket_getlen},
	{"gettype", luapacket_gettype},
	{"settype", luapacket_settype},
	{"pushstring", luapacket_pushstring},
	{"getstring", luapacket_getstring},
	{"begin", luapacket_begin},
	{"pushboolean", luapacket_pushboolean},
	{"getboolean", luapacket_getboolean},
	{"pushint8", luapacket_pushint8},
	{"getint8", luapacket_getint8},
	{"pushint16", luapacket_pushint16},
	{"getint16", luapacket_getint16},
	{"pushint32", luapacket_pushint32},
	{"getint32", luapacket_getint32},
	{"pushint64", luapacket_pushint64},
	{"getint64", luapacket_getint64},
	{"pushint16toindex", luapacket_pushint16toindex},
	{"push32k", luapacket_push32k},
	{0, 0}
};

static int luafilelog_create (lua_State *L)
{
	struct filelog *log = filelog_create();
	if (!log)
		lua_pushnil(L);
	else
		lua_pushnumber(L, (lua_Number)(intptr_t)log);
	return 1;
}

static struct filelog *get_filelog (lua_State *L, int idx)
{
	struct filelog *log = (struct filelog *)(intptr_t)luaL_checknumber(L, idx);
	luaL_argcheck(L, log != NULL, idx, "filelog expected");
	return log;
}

static int luafilelog_release (lua_State *L)
{
	struct filelog *log = get_filelog(L, 1);
	filelog_release(log);
	return 0;
}

static int luafilelog_setdirectory (lua_State *L)
{
	struct filelog *log = get_filelog(L, 1);
	const char *directory = luaL_checkstring(L, 2);
	filelog_setdirect(log, directory);
	return 0;
}

static int luafilelog_writelog (lua_State *L)
{
	struct filelog *log = get_filelog(L, 1);
	const char *str = luaL_checkstring(L, 2);
	filelog_writelog(log, "%s", str);
	return 0;
}

static int luafilelog_error (lua_State *L)
{
	struct filelog *log = get_filelog(L, 1);
	const char *str = luaL_checkstring(L, 2);
	filelog_error(log, "%s", str);
	return 0;
}

static const struct luaL_reg class_filelog_function[] = {
	{"create", luafilelog_create},
	{"release", luafilelog_release},
	{"setdirectory", luafilelog_setdirectory},
	{"writelog", luafilelog_writelog},
	{"error", luafilelog_error},
	{0, 0}
};

static int luaidmgr_create (lua_State *L)
{
	lua_Integer maxid = luaL_checkinteger(L, 1);
	luaL_argcheck(L, maxid > 0, 1, "maxid need greater than zero!");
	lua_Integer delaytime = luaL_checkinteger(L, 2);
	luaL_argcheck(L, delaytime > 0, 2, "delaytime need greater than zero!");
	struct idmgr *mgr = idmgr_create(maxid, delaytime);
	if (!mgr)
		lua_pushnil(L);
	else
		lua_pushnumber(L, (lua_Number)(intptr_t)mgr);
	return 1;
}

static struct idmgr *get_idmgr (lua_State *L, int idx)
{
	struct idmgr *mgr = (struct idmgr *)(intptr_t)luaL_checknumber(L, idx);
	luaL_argcheck(L, mgr != NULL, idx, "idmgr expected");
	return mgr;
}

static int luaidmgr_release (lua_State *L)
{
	struct idmgr *mgr = get_idmgr(L, 1);
	idmgr_release(mgr);
	return 0;
}

static int luaidmgr_total (lua_State *L)
{
	struct idmgr *mgr = get_idmgr(L, 1);
	lua_pushinteger(L, idmgr_total(mgr));
	return 1;
}

static int luaidmgr_usednum (lua_State *L)
{
	struct idmgr *mgr = get_idmgr(L, 1);
	lua_pushinteger(L, idmgr_usednum(mgr));
	return 1;
}

static int luaidmgr_allocid (lua_State *L)
{
	struct idmgr *mgr = get_idmgr(L, 1);
	int id = idmgr_allocid(mgr);
	if (id <= 0)
		lua_pushnil(L);
	else
		lua_pushinteger(L, id);
	return 1;
}

static int luaidmgr_freeid (lua_State *L)
{
	struct idmgr *mgr = get_idmgr(L, 1);
	int id = luaL_checkinteger(L, 2);
	luaL_argcheck(L, id > 0, 2, "why id not greater than zero");
	lua_pushboolean(L, idmgr_freeid(mgr, id));
	return 1;
}

static int luaidmgr_run (lua_State *L)
{
	struct idmgr *mgr = get_idmgr(L, 1);
	idmgr_run(mgr);
	return 0;
}

static const struct luaL_reg class_idmgr_function[] = {
	{"create", luaidmgr_create},
	{"release", luaidmgr_release},
	{"total", luaidmgr_total},
	{"usednum", luaidmgr_usednum},
	{"allocid", luaidmgr_allocid},
	{"freeid", luaidmgr_freeid},
	{"run", luaidmgr_run},
	{0, 0}
};

static lua_State *s_L = NULL;

static void on_ctrl_hander (int sig)
{
	if (!s_L)
		return;
	const char *functionname = "onctrl_hander";
	lua_getglobal(s_L, functionname);
	if (lua_pcall(s_L, 0, 0, 0) != 0)
	{
		//log_error("do lua function failed!, function:[%s] error:%s", functionname, luaL_checkstring(s_L, -1));
		lua_pop(s_L, 1);
	}
	signal(SIGINT, on_ctrl_hander);
}

#ifdef WIN32
extern "C" int __declspec(dllexport) luaopen_lxnet(lua_State* L)
#else
extern "C" int luaopen_lxnet(lua_State* L)
#endif
{
	luaL_register(L, "_G", g_function);

	luaL_register(L, "lxnet", class_lxnet_function);

	luaL_register(L, "listener", class_listener_function);

	luaL_register(L, "socketer", class_socketer_function);

	luaL_register(L, "packet", class_packet_function);

	luaL_register(L, "filelog", class_filelog_function);

	luaL_register(L, "idmgr", class_idmgr_function);

	lua_pop(L, 7);

	s_L = L;
	signal(SIGINT, on_ctrl_hander);

	return 1;
}

#ifdef WIN32
BOOL WINAPI 
DllMain(HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#endif


