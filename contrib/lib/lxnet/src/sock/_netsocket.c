
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <assert.h>
#include <string.h>
#include "ossome.h"
#include "_netsocket.h"
#include "socket_internal.h"
#include "net_pool.h"
#include "net_buf.h"
#include "log.h"
#include "crosslib.h"
#include "net_eventmgr.h"

#ifdef _DEBUG_NETWORK
#define debuglog debug_log
#else
#define debuglog(...)
#endif



/*
(Note!)
  0 bytes WSASend delivery, will directly receive, he would not do to write test
  WSARecv delivery 0 bytes, received the test can be done.
 */

enum e_control_value
{
	enum_list_run_delay = 300,

#ifndef WIN32
	enum_reset_lock_inlinuxtime = 3000,
#endif

	enum_list_close_delaytime = 15000,
};

struct socketmgr
{
	bool isinit;

	int64 lastrun;			/* last run time. */

	struct socketer *head;
	struct socketer *tail;
	LOCK_struct mgr_lock;
};
static struct socketmgr s_mgr = {false};

/* add to delay close list. */
static void socketmgr_add_to_waite (struct socketer *self)
{
	if (self->deleted)
		return;
	self->deleted = true;
	assert(self->next == NULL);
	LOCK_LOCK(&s_mgr.mgr_lock);
	if (s_mgr.tail)
	{
		s_mgr.tail->next = self;
		self->next = NULL;
	}
	else
	{
		s_mgr.head = self;
	}
	s_mgr.tail = self;
	LOCK_UNLOCK(&s_mgr.mgr_lock);
	self->closetime = get_millisecond();
}

/* pop from close list. */
static struct socketer *socketmgr_pop_front ()
{
	struct socketer *so = NULL;
	LOCK_LOCK(&s_mgr.mgr_lock);
	so = s_mgr.head;
	if (!so)
	{
		LOCK_UNLOCK(&s_mgr.mgr_lock);
		return NULL;
	}
	
	s_mgr.head = so->next;
	if (s_mgr.tail == so)
	{
		s_mgr.tail = NULL;
		assert(s_mgr.head == NULL);
	}
	LOCK_UNLOCK(&s_mgr.mgr_lock);
	return so;
}

/* get socket object size. */
size_t socketer_getsize ()
{
	return (sizeof(struct socketer));
}

/* default encrypt/decrypt function key. */
const char chKey = 0xae;
static void default_decrypt_func (void *logicdata, char *buf, int len)
{
	int i;
	if (len <= 0)
		return;
	for (i = 0; i < len; i++) 
	{
		buf[i] ^= chKey;
	}
}

static void default_encrypt_func (void *logicdata, char *buf, int len)
{
	int i;
	if (len <= 0)
		return;
	for (i = 0; i < len; i++)
	{
		buf[i] ^= chKey;
	}
}

static void socketer_initrecvbuf (struct socketer *self)
{
	if (!self->recvbuf)
	{
		self->recvbuf = buf_create(self->bigbuf);
		buf_setdofunc(self->recvbuf, default_decrypt_func, NULL);
	}
}

static void socketer_initsendbuf (struct socketer *self)
{
	if (!self->sendbuf)
	{
		self->sendbuf = buf_create(self->bigbuf);
		buf_setdofunc(self->sendbuf, default_encrypt_func, NULL);
	}
}

static bool socketer_init (struct socketer *self, bool bigbuf)
{

#ifdef WIN32
	memset(&self->recv_event, 0, sizeof(self->recv_event));
	memset(&self->send_event, 0, sizeof(self->send_event));
#else
	self->alreadyresetlock = false;
	self->events = 0;
#endif

	self->sockfd = NET_INVALID_SOCKET;
	self->closetime = 0;
	self->next = NULL;
	self->recvbuf = NULL;
	self->sendbuf = NULL;

	self->already_event = 0;
	self->sendlock = 0;
	self->recvlock = 0;
	self->deleted = false;
	self->connected = false;
	self->bigbuf = bigbuf;
	return true;
}

static void socketer_addto_eventlist (struct socketer *self)
{
	if (atom_compare_and_swap(&self->already_event, 0, 1) == 0)	/* if 0, then set 1, and add to event manager. */
	{
		self->connected = true;
		socket_addto_eventmgr(self);
	}
}

/* 
 * create socketer. 
 * bigbuf --- if is true, then is bigbuf; or else is smallbuf.
 */
struct socketer *socketer_create (bool bigbuf)
{
	struct socketer *self = (struct socketer *)netpool_createsocket();
	if (!self)
	{
		log_error("	struct socketer *self = (struct socketer *)netpool_createsocket();");
		return NULL;
	}
	if (!socketer_init(self, bigbuf))
	{
		netpool_releasesocket(self);
		log_error("	if (!socketer_init(self, bigbuf))");
		return NULL;
	}
	return self;
}

struct socketer *socketer_create_for_accept (bool bigbuf, void *sockfd)
{
	struct socketer *self;
	if (!socket_setopt_for_connect(*((net_socket *)sockfd)))
		return NULL;
	self = socketer_create(bigbuf);
	if (!self)
		return NULL;
	self->sockfd = *((net_socket *)sockfd);
	socketer_addto_eventlist(self);
	return self;
}

static void socketer_real_release (struct socketer *self)
{
	self->next = NULL;
	buf_release(self->recvbuf);
	buf_release(self->sendbuf);
	self->recvbuf = NULL;
	self->sendbuf = NULL;
	netpool_releasesocket(self);
}

/* release socketer */
void socketer_release (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	assert(!self->deleted);
	if (self->deleted)
		return;

	socketer_close(self);
	socketmgr_add_to_waite(self);
}

bool socketer_connect (struct socketer *self, const char *ip, short port)
{
	struct sockaddr_in soaddr;
	int lasterror;
	assert(self != NULL);
	assert(ip != NULL);
	if (!self || !ip || 0 == port)
		return false;
	assert(!self->connected);
	if (self->connected)
		return false;
	if (self->sockfd == NET_INVALID_SOCKET)
	{
		self->sockfd = socket_create();
		if (self->sockfd == NET_INVALID_SOCKET)
		{
			log_error("self->sockfd = socket_create(); is invalid socket!, error!");
			return false;
		}
		socket_setopt_for_connect(self->sockfd);
	}

	soaddr.sin_family = AF_INET;
	soaddr.sin_addr.s_addr = inet_addr(ip);
	if (soaddr.sin_addr.s_addr == INADDR_NONE)
	{
		struct hostent *lphost = gethostbyname(ip);
		if (lphost != NULL)
			memcpy(&(soaddr.sin_addr), *(lphost->h_addr_list), sizeof(struct in_addr));
		else
			return false;
	}
	soaddr.sin_port = htons(port);
	if (connect(self->sockfd, (struct sockaddr *)&soaddr, (net_sock_len)sizeof(soaddr)) < 0)
	{
		lasterror = NET_GetLastError();
		if (SOCKET_ERR_CONNECT_RETRIABLE(lasterror) || 
				SOCKET_ERR_CONNECT_REFUSED(lasterror))
			return false;
		if (SOCKET_ERR_CONNECT_ALREADY(lasterror))
		{
			socketer_addto_eventlist(self);
			return true;
		}

		return false;
	}
	else
	{
		socketer_addto_eventlist(self);
		return true;
	}
}

void socketer_close (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	if (self->sockfd != NET_INVALID_SOCKET)
	{
		if (atom_compare_and_swap(&self->already_event, 1, 0) == 1)	/*if 1, then set 0, and remove from event manager. */
		{
			socket_removefrom_eventmgr(self);
		}
		socket_close(&self->sockfd);
	}
	self->connected = false;
}

bool socketer_isclose (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return true;
	return (self->sockfd == NET_INVALID_SOCKET);
}

void socketer_getip (struct socketer *self, char *ip, size_t len)
{
	struct sockaddr_in localaddr;
	net_sock_len isize = sizeof(localaddr);
	assert(self != NULL);
	assert(ip != NULL);
	if (!self || !ip)
		return;
	if (0 == getpeername(self->sockfd, (struct sockaddr*)&localaddr, &isize))
	{
		strncpy(ip, inet_ntoa(localaddr.sin_addr), len-1);
	}
	ip[len-1] = '\0';
}

bool socketer_sendmsg (struct socketer *self, void *data, int len)
{
	assert(self != NULL);
	assert(data != NULL);
	assert(len > 0);
	if (!self || !data || len <= 0)
		return false;
	if (self->deleted || !self->connected)
		return false;
	socketer_initsendbuf(self);
	return buf_pushmessage(self->sendbuf, (char *)data, len);
}

/* 
 * when sending data. test send limit as len.
 * if return true, close this connect.
 */
bool socketer_send_islimit (struct socketer *self, size_t len)
{
	assert(self != NULL);
	assert(len < _MAX_MSG_LEN);
	if (!self)
		return true;
	socketer_initsendbuf(self);
	return buf_add_islimit(self->sendbuf, len);
}

/* set send event. */
void socketer_checksend (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	if (self->deleted || !self->connected)
		return;
	socketer_initsendbuf(self);

	/* if not has data for send. */
	if (buf_can_not_send(self->sendbuf))
		return;
	if (atom_compare_and_swap(&self->sendlock, 0, 1) == 0)	/* if 0, then set 1, and set sendevent. */
	{
		socket_setup_sendevent(self);
	}
}

void *socketer_getmsg (struct socketer *self, char *buf, size_t bufsize)
{
	void *msg;
	bool needclose;
	assert(self != NULL);
	if (!self)
		return NULL;
	socketer_initrecvbuf(self);
	msg = buf_getmessage(self->recvbuf, &needclose, buf, bufsize);
	if (needclose)
		socketer_close(self);
	return msg;
}

/* set recv event. */
void socketer_checkrecv (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	if (self->deleted || !self->connected)
		return;		
	socketer_initrecvbuf(self);

	/* if not recv, because limit. */
	if (buf_can_not_recv(self->recvbuf))
		return;
	if (atom_compare_and_swap(&self->recvlock, 0, 1) == 0)	/* if 0, then set 1, and set recvevent.*/
	{
		socket_setup_recvevent(self);
	}
}

/* set recv data limit. */
void socketer_set_recv_critical (struct socketer *self, long size)
{
	assert(self != NULL);
	if (!self)
		return;
	if (size > 0)
	{
		socketer_initrecvbuf(self);
		buf_set_limitsize(self->recvbuf, size);
	}
}

/* set send data limit.*/
void socketer_set_send_critical (struct socketer *self, long size)
{
	assert(self != NULL);
	if (!self)
		return;
	if (size > 0)
	{
		socketer_initsendbuf(self);
		buf_set_limitsize(self->sendbuf, size);
	}
}

void socketer_use_compress (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	socketer_initsendbuf(self);
	buf_usecompress(self->sendbuf);
}

void socketer_use_uncompress (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	socketer_initrecvbuf(self);
	buf_useuncompress(self->recvbuf);
}

/* set encrypt and decrypt function and logic data. */
void socketer_set_other_do_function (struct socketer *self, dofunc_f encrypt_func, dofunc_f decrypt_func, void *logicdata)
{
	assert(self != NULL);
	if (!self)
		return;
	if (encrypt_func)
	{
		socketer_initsendbuf(self);
		buf_setdofunc(self->sendbuf, encrypt_func, logicdata);
	}

	if (decrypt_func)
	{
		socketer_initrecvbuf(self);
		buf_setdofunc(self->recvbuf, decrypt_func, logicdata);
	}
}

void socketer_use_encrypt (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	socketer_initsendbuf(self);
	buf_useencrypt(self->sendbuf);
}

void socketer_use_decrypt (struct socketer *self)
{
	assert(self != NULL);
	if (!self)
		return;
	socketer_initrecvbuf(self);
	buf_usedecrypt(self->recvbuf);
}

/* interface for event mgr. */

void socketer_on_recv (struct socketer *self)
{
	int res;
	struct bufinfo writebuf;
	
	debuglog("on recv\n");

	assert(self->recvlock == 1);
	for (;;)
	{
		writebuf = buf_getwritebufinfo(self->recvbuf);
		assert(writebuf.len >= 0);
		if (writebuf.len <= 0)
		{
			if (!buf_recv_end_do(self->recvbuf))
			{
				/* uncompress error, close socket. */
				socketer_close(self);
				atom_dec(&self->recvlock);
				return;
			}

#ifndef WIN32
			/* remove recv event. */
			socket_remove_recvevent(self);
#endif

			atom_dec(&self->recvlock);
			return;
		}

		res = recv(self->sockfd, writebuf.buf, writebuf.len, 0);
		if (res > 0)
		{
			buf_addwrite(self->recvbuf, writebuf.buf, res);
			debuglog("recv :%d size\n", res);
		}
		else
		{
			if (!buf_recv_end_do(self->recvbuf))
			{
				/* uncompress error, close socket. */
				socketer_close(self);
				atom_dec(&self->recvlock);
				return;
			}

			if ((!SOCKET_ERR_RW_RETRIABLE(NET_GetLastError())) || (res == 0))
			{
				/* error, close socket. */
				socketer_close(self);
				atom_dec(&self->recvlock);
				debuglog("recv func, socket is error!, so close it!\n");
			}
			else
			{
#ifdef WIN32
				/* set recv event. */
				socket_setup_recvevent(self);
				debuglog("setup recv event...\n");
#endif
			}
			/* return. !!! */
			return;
		}
		
	}
}


void socketer_on_send (struct socketer *self, int len)
{
	int res;
	struct bufinfo readbuf;
	debuglog("on send\n");

	assert(self->sendlock == 1);
	assert(len >= 0);
	if (len > 0)
	{
		buf_addread(self->sendbuf, len);
		debuglog("send :%d size\n", len);
	}
	
	/* do something before real send. */
	buf_send_before_do(self->sendbuf);

	for (;;)
	{
		readbuf = buf_getreadbufinfo(self->sendbuf);
		assert(readbuf.len >= 0);
		if (readbuf.len <= 0)
		{

#ifndef WIN32
			/* remove send event. */
			socket_remove_sendevent(self);
#endif

			atom_dec(&self->sendlock);
			return;
		}

		res = send(self->sockfd, readbuf.buf, readbuf.len, 0);
		if (res > 0)
		{
			buf_addread(self->sendbuf, res);
			debuglog("send :%d size\n", res);
		}
		else
		{
			if (!SOCKET_ERR_RW_RETRIABLE(NET_GetLastError()))
			{
				/* error, close socket. */
				socketer_close(self);
				atom_dec(&self->sendlock);
				debuglog("send func, socket is error!, so close it!\n");
			}
			else
			{
#ifdef WIN32
				/* if > 512, then set is 512. */
				if (readbuf.len > 512)
					readbuf.len = 512;

				/* set send data, because WSASend 0 size data, not check can send. */
				socket_senddata(self, readbuf.buf, readbuf.len);
				debuglog("setup send event...\n");
#endif
			}
			/* return. !!! */
			return;
		}
	}
}

/* create and init socketer manager. */
bool socketmgr_init ()
{
	if (s_mgr.isinit)
		return false;
	s_mgr.isinit = true;
	s_mgr.lastrun = 0;
	s_mgr.head = NULL;
	s_mgr.tail = NULL;
	LOCK_INIT(&s_mgr.mgr_lock);
	return true;
}

/* run socketer manager. */
void socketmgr_run ()
{
	int64 current;
	if (!s_mgr.head)
		return;
	current = get_millisecond();
	if (current - s_mgr.lastrun >= enum_list_run_delay)
	{
		struct socketer *sock, *resock;
		s_mgr.lastrun = current;
		for (;;)
		{
			sock = s_mgr.head;
			if (!sock)
				return;

#ifndef WIN32
			if (current - sock->closetime >= enum_reset_lock_inlinuxtime && !sock->alreadyresetlock)
			{
				atom_compare_and_swap(&sock->recvlock, 1, 0);
				atom_compare_and_swap(&sock->sendlock, 1, 0);
				sock->alreadyresetlock = true;
			}
#endif

			if (current - sock->closetime < enum_list_close_delaytime)
				return;
			resock = socketmgr_pop_front();
			assert(resock == sock);
			if (sock != resock)
			{
				log_error(" if (sock != resock)");
				if (resock)
				{
					if (resock->recvlock != 0 || resock->sendlock != 0)
						log_error("%x socket recvlock:%d, sendlock:%d", resock, (int)resock->recvlock, (int)resock->sendlock);
					socketer_real_release(resock);
				}
			}

			if (sock->recvlock != 0 || sock->sendlock != 0)
				log_error("%x socket recvlock:%d, sendlock:%d", sock, (int)sock->recvlock, (int)sock->sendlock);
			socketer_real_release(sock);
		}
	}
}

/* release socketer manager. */
void socketmgr_release ()
{
	struct socketer *sock, *next;
	if (!s_mgr.isinit)
		return;
	s_mgr.isinit = false;
	LOCK_LOCK(&s_mgr.mgr_lock);
	for (sock = s_mgr.head; sock; sock = next)
	{
		next = sock->next;
		socketer_real_release(sock);
	}
	LOCK_UNLOCK(&s_mgr.mgr_lock);
	LOCK_DELETE(&s_mgr.mgr_lock);
	s_mgr.head = NULL;
	s_mgr.tail = NULL;
}


