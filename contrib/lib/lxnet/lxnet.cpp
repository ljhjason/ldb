
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <stdio.h>
#include <string.h>
#include "ossome.h"
#include "net_module.h"
#include "lxnet.h"
#include "net_buf.h"
#include "pool.h"
#include "msgbase.h"
#include "log.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

struct infomgr
{
	bool isinit;
	struct poolmgr *socket_pool;
	LOCK_struct socket_lock;
	
	struct poolmgr *listen_pool;
	LOCK_struct listen_lock;
};
static struct infomgr s_infomgr = {false};

static bool infomgr_init (size_t socketnum, size_t listennum)
{
	if (s_infomgr.isinit)
		return false;
	s_infomgr.socket_pool = poolmgr_create(sizeof(lxnet::Socketer), 8, socketnum, 1, "Socketer obj pool");
	s_infomgr.listen_pool = poolmgr_create(sizeof(lxnet::Listener), 8, listennum, 1, "Listen obj pool");
	if (!s_infomgr.socket_pool || !s_infomgr.listen_pool)
	{
		poolmgr_release(s_infomgr.socket_pool);
		poolmgr_release(s_infomgr.listen_pool);
		return false;
	}
	LOCK_INIT(&s_infomgr.socket_lock);
	LOCK_INIT(&s_infomgr.listen_lock);
	s_infomgr.isinit = true;
	return true;
}

static void infomgr_release ()
{
	if (!s_infomgr.isinit)
		return;
	s_infomgr.isinit = false;
	poolmgr_release(s_infomgr.socket_pool);
	poolmgr_release(s_infomgr.listen_pool);
	LOCK_DELETE(&s_infomgr.socket_lock);
	LOCK_DELETE(&s_infomgr.listen_lock);
}

namespace lxnet{

/* 监听*/
bool Listener::Listen (unsigned short port, int backlog)
{
	return listener_listen(m_self, port, backlog);
}

/* 关闭用于监听的套接字，停止监听*/
void Listener::Close ()
{
	listener_close(m_self);
}

/* 测试是否已关闭*/
bool Listener::IsClose ()
{
	return listener_isclose(m_self);
}

/* 在指定的监听socket上接受连接*/
Socketer *Listener::Accept (bool bigbuf)
{
	struct socketer *sock = listener_accept(m_self, bigbuf);
	if (!sock)
		return NULL;
	LOCK_LOCK(&s_infomgr.socket_lock);
	Socketer *self = (Socketer *)poolmgr_getobject(s_infomgr.socket_pool);
	LOCK_UNLOCK(&s_infomgr.socket_lock);
	if (!self)
	{
		socketer_release(sock);
		return NULL;
	}
	self->m_self = sock;
	return self;
}

/* 检测是否有新的连接*/
bool Listener::CanAccept ()
{
	return listener_can_accept(m_self);
}

/* 设置接收数据字节的临界值，超过此值，则停止接收，若小于等于0，则视为不限制*/
void Socketer::SetRecvCritical (long size)
{
	socketer_set_recv_critical(m_self, size);
}

/* 设置发送数据字节的临界值，若缓冲中数据长度大于此值，则断开此连接，若为0，则视为不限制*/
void Socketer::SetSendCritical (long size)
{
	socketer_set_send_critical(m_self, size);
}

/* （对发送数据起作用）设置启用压缩，若要启用压缩，则此函数在创建socket对象后即刻调用*/
void Socketer::UseCompress ()
{
	socketer_use_compress(m_self);
}

/* （慎用）（对接收的数据起作用）启用解压缩，网络库会负责解压缩操作，仅供客户端使用*/
void Socketer::UseUncompress ()
{
	socketer_use_uncompress(m_self);
}

/* 设置加密/解密函数， 以及特殊用途的参与加密/解密逻辑的数据。
 * 若加密/解密函数为NULL，则保持默认。
 * */
void Socketer::SetEncryptDecryptFunction (void (*encryptfunc)(void *logicdata, char *buf, int len), void (*decryptfunc)(void *logicdata, char *buf, int len), void *logicdata)
{
	socketer_set_other_do_function(m_self, encryptfunc, decryptfunc, logicdata);
}

/* （启用加密）*/
void Socketer::UseEncrypt ()
{
	socketer_use_encrypt(m_self);
}

/* （启用解密）*/
void Socketer::UseDecrypt ()
{
	socketer_use_decrypt(m_self);
}

/* 启用TGW接入 */
void Socketer::UseTGW ()
{
	socketer_use_tgw(m_self);
}

/* 关闭用于连接的socket对象*/
void Socketer::Close ()
{
	socketer_close(m_self);
}

/* 连接指定的服务器*/
bool Socketer::Connect (const char *ip, short port)
{
	return socketer_connect(m_self, ip, port);
}

/* 测试socket套接字是否已关闭*/
bool Socketer::IsClose ()
{
	return socketer_isclose(m_self);
}
/* 获取此客户端ip地址*/
void Socketer::GetIP (char *ip, size_t len)
{
	socketer_getip(m_self, ip, len);
}

/* 发送数据，仅仅是把数据压入包队列中，adddata为附加到pMsg后面的数据，当然会自动修改pMsg的长度，addsize指定adddata的长度*/
bool Socketer::SendMsg (Msg *pMsg, void *adddata, size_t addsize)
{
	if (!pMsg)
		return false;
	if (adddata && addsize == 0)
	{
		assert(false);
		return false;
	}
	if (!adddata && addsize != 0)
	{
		assert(false);
		return false;
	}
	if (pMsg->GetLength() <= (int)sizeof(int))
		return false;
	if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN)
	{
		assert(false && "if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN)");
		log_error("	if (pMsg->GetLength() + addsize >= _MAX_MSG_LEN)");
		return false;
	}
	if (socketer_send_islimit(m_self, pMsg->GetLength()+addsize))
	{
		Close();
		return false;
	}
	if (adddata && addsize != 0)
	{
		bool res1, res2;
		int onesend = pMsg->GetLength();
		pMsg->SetLength(onesend + addsize);
		res1 = socketer_sendmsg(m_self, pMsg, onesend);
		
		//这里切记要修改回去。 例：对于同一个包遍历发送给一个列表，然后每次都附带不同尾巴。。。这种情景，那么必须如此恢复。
		pMsg->SetLength(onesend);
		res2 = socketer_sendmsg(m_self, adddata, addsize);
		return (res1 && res2);
	}
	else
	{
		return socketer_sendmsg(m_self, pMsg, pMsg->GetLength());
	}
}

/* 对as3发送策略文件 */
bool Socketer::SendPolicyData ()
{
	//as3套接字策略文件
	char buf[512] = "<cross-domain-policy> <allow-access-from domain=\"*\" secure=\"false\" to-ports=\"*\"/> </cross-domain-policy> ";
	size_t datasize = strlen(buf);
	if (socketer_send_islimit(m_self, datasize))
	{
		Close();
		return false;
	}
	return socketer_sendmsg(m_self, buf, datasize + 1);
}

/* 发送TGW信息头 */
bool Socketer::SendTGWInfo (const char *domain, int port)
{
	char buf[1024] = {};
	size_t datasize;
	snprintf(buf, sizeof(buf) - 1, "tgw_l7_forward\r\nHost: %s:%d\r\n\r\n", domain, port);
	buf[sizeof(buf) - 1] = '\0';
	datasize = strlen(buf);
	if (socketer_send_islimit(m_self, datasize))
	{
		Close();
		return false;
	}
	socketer_set_raw_datasize(m_self, datasize);
	return socketer_sendmsg(m_self, buf, datasize);
}

/* 触发真正的发送数据*/
void Socketer::CheckSend ()
{
	socketer_checksend(m_self);
}

/* 尝试投递接收操作*/
void Socketer::CheckRecv ()
{
	socketer_checkrecv(m_self);
}

/* 接收数据*/
Msg *Socketer::GetMsg (char *buf, size_t bufsize)
{
	return (Msg *)socketer_getmsg(m_self, buf, bufsize);
}



/* 初始化网络，
 * bigbufsize指定大块的大小，bigbufnum指定大块的数目，
 * smallbufsize指定小块的大小，smallbufnum指定小块的数目
 * listen num指定用于监听的套接字的数目，socket num用于连接的总数目
 * threadnum指定网络线程数目，若设置为小于等于0，则会开启cpu个数的线程数目
 */
bool net_init (size_t bigbufsize, size_t bigbufnum, size_t smallbufsize, size_t smallbufnum,
		size_t listenernum, size_t socketnum, int threadnum)
{
	if (!infomgr_init(socketnum, listenernum))
		return false;
	if (!netinit(bigbufsize, bigbufnum, smallbufsize, smallbufnum,
			listenernum, socketnum, threadnum))
	{
		infomgr_release();
		return false;
	}
	return true;
}

/* 创建一个用于监听的对象*/
Listener *Listener_create ()
{
	if (!s_infomgr.isinit)
	{
		assert(false && "Listener_create not init!");
		return NULL;
	}
	struct listener *ls = listener_create();
	if (!ls)
		return NULL;
	LOCK_LOCK(&s_infomgr.listen_lock);
	Listener *self = (Listener *)poolmgr_getobject(s_infomgr.listen_pool);
	LOCK_UNLOCK(&s_infomgr.listen_lock);
	if (!self)
	{
		listener_release(ls);
		return NULL;
	}
	self->m_self = ls;
	return self;
}

/* 释放一个用于监听的对象*/
void Listener_release (Listener *self)
{
	if (!self)
		return;
	if (self->m_self)
	{
		listener_release(self->m_self);
		self->m_self = NULL;
	}
	LOCK_LOCK(&s_infomgr.listen_lock);
	poolmgr_freeobject(s_infomgr.listen_pool, self);
	LOCK_UNLOCK(&s_infomgr.listen_lock);
}

/* 创建一个Socketer对象*/
Socketer *Socketer_create (bool bigbuf)
{
	if (!s_infomgr.isinit)
	{
		assert(false && "Csocket_create not init!");
		return NULL;
	}
	struct socketer *so = socketer_create(bigbuf);
	if (!so)
		return NULL;
	LOCK_LOCK(&s_infomgr.socket_lock);
	Socketer *self = (Socketer *)poolmgr_getobject(s_infomgr.socket_pool);
	LOCK_UNLOCK(&s_infomgr.socket_lock);
	if (!self)
	{
		socketer_release(so);
		return NULL;
	}
	self->m_self = so;
	return self;
}

/* 释放Socketer对象，会自动调用关闭等善后操作*/
void Socketer_release (Socketer *self)
{
	if (!self)
		return;
	if (self->m_self)
	{
		socketer_release(self->m_self);
		self->m_self = NULL;
	}
	LOCK_LOCK(&s_infomgr.socket_lock);
	poolmgr_freeobject(s_infomgr.socket_pool, self);
	LOCK_UNLOCK(&s_infomgr.socket_lock);
}

/* 释放网络相关*/
void net_release ()
{
	infomgr_release();
	netrelease();
}

/* 执行相关操作，需要在主逻辑中调用此函数*/
void net_run ()
{
	netrun();
}

static char s_memory_info[2*1024*1024+1024];
/* 获取socket对象池，listen对象池，大块池，小块池的使用情况*/
const char *net_memory_info ()
{
	size_t index = 0;

	LOCK_LOCK(&s_infomgr.socket_lock);
	poolmgr_getinfo(s_infomgr.socket_pool, s_memory_info, sizeof(s_memory_info)-1);
	LOCK_UNLOCK(&s_infomgr.socket_lock);
	
	index = strlen(s_memory_info);

	LOCK_LOCK(&s_infomgr.listen_lock);
	poolmgr_getinfo(s_infomgr.listen_pool, &s_memory_info[index], sizeof(s_memory_info)-1-index);
	LOCK_UNLOCK(&s_infomgr.listen_lock);

	index = strlen(s_memory_info);
	netmemory_info(&s_memory_info[index], sizeof(s_memory_info)-1-index);
	return s_memory_info;
}

}


