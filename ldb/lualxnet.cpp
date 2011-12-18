
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifdef WIN32
#include <windows.h>
#endif
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
};

#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "crosslib.h"
#include "log.h"
#include "idmgr.h"
#include "lxnet.h"
#include "msgbase.h"
#include "utf8code.h"
#include "utility.h"

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
	luaL_argcheck(L, smallbufnum >= 1, 2, "buf num is must greater than 1");
	if (smallbufnum < 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int listenernum = (int)luaL_checkinteger(L, 3);
	luaL_argcheck(L, listenernum >= 1, 3, "listen object num is must greater than 1");
	if (listenernum < 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	int socketnum = (int)luaL_checkinteger(L, 4);
	luaL_argcheck(L, socketnum >= 1, 4, "socket object num is must greater than 1");
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

static int lua_hasstr (lua_State *L)
{
	const char *src = luaL_checkstring(L, 1);
	const char *path = luaL_checkstring(L, 2);
	char *res = (char *)strstr(src, path);
	if (!res)
		lua_pushboolean(L, 0);
	else
		lua_pushboolean(L, 1);
	return 1;
}

static int lua_create_guid (lua_State *L)
{
	char buf[128] = {0};
	if (create_guid(buf, sizeof(buf)))
		lua_pushstring(L, buf);
	else
		lua_pushnil(L);
	return 1;
}

static int lua_delay (lua_State *L)
{
	int millisecond = (int)luaL_checkinteger(L, 1);
	luaL_argcheck(L, millisecond >= 0, 1, "argv must >= 0");

	delay_delay(millisecond);
	return 0;
}

static int krand (int min, int max)
{
	int temp = min;
	min = temp > max ? max: temp;
	max = max < temp ? temp: max;
	return (min + rand() % (max - min  + 1));
}

static int lua_krand (lua_State *L)
{
	int arg1 = luaL_checkinteger(L, 1);
	int arg2 = luaL_checkinteger(L, 2);
	lua_pushinteger(L, krand(arg1, arg2));
	return 1;
}

static int lua_get_millisecond (lua_State *L)
{
	lua_pushnumber(L, (lua_Number )get_millisecond());
	return 1;
}

static int lua_getfullname (lua_State *L)
{
	char buf[1024*8] = {0};
	const char *name = luaL_checkstring(L, 1);
	getfilefullname(name, buf, sizeof(buf) - 1);
	lua_pushstring(L, buf);
	return 1;
}

static int lua_getcurrentpath (lua_State *L)
{
	char buf[1024*8] = {0};
	getcurrentpath(buf, sizeof(buf) - 1);
	lua_pushstring(L, buf);
	return 1;
}

static int lua_iswindows (lua_State *L)
{

#ifdef WIN32
	lua_pushboolean(L, 1);
#else
	lua_pushboolean(L, 0);
#endif

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

static int lua_makeint64by32 (lua_State *L)
{
	int32 high = luaL_checkinteger(L, 1);
	int32 low = luaL_checkinteger(L, 2);
	int64 res = 0;
	res |= high;
	res <<=32;
	res |= low;
	lua_pushnumber(L, (lua_Number) res);
	return 1;
}

static int lua_parseint64 (lua_State *L)
{
	lua_Number resf = luaL_checknumber(L, 1);
	int64 res = (int64)resf;
	
	int32 low = (int32)res;
	int32 high = (int32)(res >> 32);
	lua_pushinteger(L, high);
	lua_pushinteger(L, low);
	return 2;
}

static const struct luaL_reg g_function[] = {
	{"create_guid", lua_create_guid},
	{"hasstr", lua_hasstr},
	{"rand", lua_krand},
	{"delay", lua_delay},
	{"getmillisecond", lua_get_millisecond},
	{"getfilefullname", lua_getfullname},
	{"getcurrentpath", lua_getcurrentpath},
	{"iswindows", lua_iswindows},
	{"log_setdirectory", lua_log_setdirectory},
	{"log_writelog", lua_log_writelog},
	{"log_error", lua_log_error},
	{"makeint64by32", lua_makeint64by32},
	{"parseint64", lua_parseint64},
	{0, 0}
};

static int lualistener_create (lua_State *L)
{
	Listener *lis = Listener_create();
	if (!lis)
		lua_pushnil(L);
	else
		lua_pushlightuserdata(L, lis);
	return 1;
}

static Listener *get_listener (lua_State *L, int idx)
{
	Listener *lis = (Listener *)lua_touserdata(L, idx);
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
		lua_pushlightuserdata(L, sock);
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
		lua_pushlightuserdata(L, sock);
	return 1;
}

static Socketer *get_socketer (lua_State *L, int idx)
{
	Socketer *sock = (Socketer *)lua_touserdata(L, idx);
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
	Msg *pMsg = (Msg *)lua_touserdata(L, 2);
	luaL_argcheck(L, pMsg != NULL, 2, "Msg expected");
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
		lua_pushlightuserdata(L, pMsg);
	return 1;
}

static int luasocketer_getmsg_ldb (lua_State *L)
{
	static char s_buf[1024*256];
	Socketer *sock = get_socketer(L, 1);
	Msg *pMsg = sock->GetMsg(s_buf, sizeof(s_buf));
	if (!pMsg)
		lua_pushnil(L);
	else
		lua_pushlightuserdata(L, pMsg);
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
	{"getmsg_ldb", luasocketer_getmsg_ldb},
	{"realsend", luasocketer_realsend},
	{"realrecv", luasocketer_realrecv},
	{0, 0}
};

static MessagePack s_anyfor_ldb;
static MessagePack s_anyfirst;
static MessagePack s_anysecond;

static int luapacket_anyfor_ldb (lua_State *L)
{
	MessagePack *pack = &s_anyfor_ldb;
	pack->ResetMsgLength();
	lua_pushlightuserdata(L, pack);
	return 1;
}

static int luapacket_anyfirst (lua_State *L)
{
	MessagePack *pack = &s_anyfirst;
	pack->ResetMsgLength();
	lua_pushlightuserdata(L, pack);
	return 1;
}

static int luapacket_anysecond (lua_State *L)
{
	MessagePack *pack = &s_anysecond;
	pack->ResetMsgLength();
	lua_pushlightuserdata(L, pack);
	return 1;
}

static MessagePack *get_messagepack (lua_State *L, int idx)
{
	MessagePack *pack = (MessagePack *)lua_touserdata(L, idx);
	luaL_argcheck(L, pack != NULL, idx, "packet expected");
	return pack;
}

static int luapacket_getheadlen (lua_State *L)
{
	lua_pushinteger(L, (int)sizeof(Msg));
	return 1;
}

static int luapacket_canpush (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int16 trypushnum = luaL_checkinteger(L, 2);
	luaL_argcheck(L, trypushnum > 0, 2, "canpush, trypushnum need greater than to zero");
	luaL_argcheck(L, trypushnum <= SHRT_MAX - 7, 2, "canpush, trypushnum need less than or equal to 32760");
	lua_pushboolean(L, pack->CanPush(trypushnum));
	return 1;
}

static int luapacket_reset (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	pack->ResetMsgLength();
	return 0;
}

static int luapacket_setindex (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int idx = (int)luaL_checkinteger(L, 2);
	luaL_argcheck(L, idx >= 0 && (idx < MessagePack::e_thismessage_max_size), 2, "msg index need greater than zero, less than MessagePack::e_thismessage_max_size");
	pack->SetIndex(idx);
	return 0;
}

static int luapacket_setlen (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int len = (int)luaL_checkinteger(L, 2);
	luaL_argcheck(L, len >= (int)sizeof(Msg) && (len <= 1024*128 + (int)sizeof(Msg)), 2, "msg new length less than min length or greater than 1024*128 + (int)sizeof(Msg)");
	pack->SetLength(len);
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
	int16 needwrite = luaL_optinteger(L, 3, SHRT_MAX - 3);

	if (needwrite < 0 || needwrite > SHRT_MAX - 3)
		needwrite = SHRT_MAX - 3;
	pack->PushString(str, needwrite);
	return 0;
}

static int luapacket_getstring (lua_State *L)
{
	static char buf[33*1024];

	MessagePack *pack = get_messagepack(L, 1);
	int16 needread = luaL_optinteger(L, 2, SHRT_MAX);
	if (needread <= 0 || needread > SHRT_MAX)
		needread = SHRT_MAX;
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

static int luapacket_pushint8toindex (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int index = luaL_checkinteger(L, 2);
	luaL_argcheck(L, index >= 0, 2, "pushint8toindex, index need greater than or equal to zero");
	int8 value = (int8)luaL_checkinteger(L, 3);
	pack->PutDataNotAddLength(index, &value, sizeof(value));
	return 0;
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

static int luapacket_pushint32toindex (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int index = luaL_checkinteger(L, 2);
	luaL_argcheck(L, index >= 0, 2, "pushint32toindex, index need greater than or equal to zero");
	int32 value = (int32)luaL_checkinteger(L, 3);
	pack->PutDataNotAddLength(index, &value, sizeof(value));
	return 0;
}

static int luapacket_pushint64toindex (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int index = luaL_checkinteger(L, 2);
	luaL_argcheck(L, index >= 0, 2, "pushint64toindex, index need greater than or equal to zero");
	int64 value = (int64)luaL_checknumber(L, 3);
	pack->PutDataNotAddLength(index, &value, sizeof(value));
	return 0;
}

static int luapacket_pushfixedstringtoindex (lua_State *L)
{
	MessagePack *pack = get_messagepack(L, 1);
	int index = luaL_checkinteger(L, 2);
	luaL_argcheck(L, index >= 0, 2, "pushfixedstringtoindex, index need greater than or equal to zero");
	const char *str = luaL_checkstring(L, 3);
	int16 needwrite = luaL_checkinteger(L, 4);
	luaL_argcheck(L, needwrite >= 0, 4, "pushfixedstringtoindex, fixed length need greater than or equal to zero");
	luaL_argcheck(L, needwrite <= 512, 4, "pushfixedstringtoindex, fixed length need less than or equal to 512");

	size_t strsize = strlen(str);
	if (strsize > (size_t )needwrite)
		strsize = needwrite;

	int16 secondpush = needwrite - (int16)strsize;
	pack->PutDataNotAddLength(index, &needwrite, sizeof(needwrite));
	index += sizeof(needwrite);
	
	if (strsize > 0)
	{
		pack->PutDataNotAddLength(index, (void *)str, strsize);
		index += strsize;
	}

	static char s_buf[513] = {0};

	if (secondpush > 0)
		pack->PutDataNotAddLength(index, (void *)s_buf, (size_t)secondpush);
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
	{"anyfor_ldb", luapacket_anyfor_ldb},
	{"anyfirst", luapacket_anyfirst},
	{"anysecond", luapacket_anysecond},

	{"getheadlen", luapacket_getheadlen},
	{"canpush", luapacket_canpush},
	{"reset", luapacket_reset},
	{"setindex", luapacket_setindex},
	{"setlen", luapacket_setlen},
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
	{"pushint8toindex", luapacket_pushint8toindex},
	{"pushint16toindex", luapacket_pushint16toindex},
	{"pushint32toindex", luapacket_pushint32toindex},
	{"pushint64toindex", luapacket_pushint64toindex},
	{"pushfixedstringtoindex", luapacket_pushfixedstringtoindex},
	{"push32k", luapacket_push32k},
	{0, 0}
};

static int luafilelog_create (lua_State *L)
{
	struct filelog *log = filelog_create();
	if (!log)
		lua_pushnil(L);
	else
		lua_pushlightuserdata(L, log);
	return 1;
}

static struct filelog *get_filelog (lua_State *L, int idx)
{
	struct filelog *log = (struct filelog *)lua_touserdata(L, idx);
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

static int luafilelog_mkdir (lua_State *L)
{
	const char *dirname = luaL_checkstring(L, 1);
	lua_pushboolean(L, mymkdir(dirname) == 0);
	return 1;
}

static const struct luaL_reg class_filelog_function[] = {
	{"create", luafilelog_create},
	{"release", luafilelog_release},
	{"setdirectory", luafilelog_setdirectory},
	{"writelog", luafilelog_writelog},
	{"error", luafilelog_error},
	{"mkdir", luafilelog_mkdir},
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
		lua_pushlightuserdata(L, mgr);
	return 1;
}

static struct idmgr *get_idmgr (lua_State *L, int idx)
{
	struct idmgr *mgr = (struct idmgr *)lua_touserdata(L, idx);
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

static int lua_utf8_toansi (lua_State *L)
{
	const char *src = luaL_checkstring(L, 1);
	char *out = Utf8ToAnsi(src);
	if (!out)
	{
		lua_pushstring(L, "");
	}
	else
	{
		lua_pushstring(L, out);
		free(out);
	}
	return 1;
}

static int lua_ansi_toutf8 (lua_State *L)
{
	const char *src = luaL_checkstring(L, 1);
	char *out = AnsiToUtf8(src);
	if (!out)
	{
		lua_pushstring(L, "");
	}
	else
	{
		lua_pushstring(L, out);
		free(out);
	}
	return 1;
}

static const struct luaL_reg class_encode_function[] = {
	{"utf8_toansi", lua_utf8_toansi},
	{"ansi_toutf8", lua_ansi_toutf8},
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

	luaL_register(L, "encode", class_encode_function);

	lua_pop(L, 8);

	s_L = L;
	signal(SIGINT, on_ctrl_hander);

	srand(time(NULL));
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


