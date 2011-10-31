
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifdef WIN32

#include "ossome.h"
#include <assert.h>
#include <stdlib.h>
#include "net_eventmgr.h"
#include "socket_internal.h"
#include "_netsocket.h"
#include <process.h>

#ifdef _DEBUG_NETWORK
#define debuglog debug_log
#else
#define debuglog(...) ((void) 0)
#endif

/* event type. */
enum e_socket_ioevent
{
	e_socket_io_event_read_complete = 0,	/* read operate. */
	e_socket_io_event_write_end		= 1,	/* write operate. */
	e_socket_io_thread_shutdown		= 2,	/* stop iocp. */
};

struct iocpmgr
{
	bool isinit;
	bool isrun;
	int threadnum;
	HANDLE completeport;
};
static struct iocpmgr s_iocp = {false};

/* add socket to event manager. */
void socket_addto_eventmgr (struct socketer *self)
{
	/* socket object point into iocp. */
	CreateIoCompletionPort((HANDLE)self->sockfd, s_iocp.completeport, (ULONG_PTR)self, 0);
}

/* remove socket from event manager. */
void socket_removefrom_eventmgr (struct socketer *self)
{

}

/* set recv event. */
void socket_setup_recvevent (struct socketer *self)
{
	DWORD w_length = 0;
	DWORD flags = 0;
	WSABUF buf;
	buf.len = 0;
	buf.buf = NULL;

	assert(self != NULL);
	assert(self->recvlock == 1);

	memset(&self->recv_event, 0, sizeof(self->recv_event));
	self->recv_event.m_event = e_socket_io_event_read_complete;
	if (WSARecv(self->sockfd, &buf, 1, &w_length, &flags, &self->recv_event.m_overlap, 0) == SOCKET_ERROR)
	{
		/* overlapped operation failed to start. */
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			debuglog("socket_setup_recvevent error!, error:%d\n", WSAGetLastError());
			atom_dec(&self->recvlock);
			socketer_close(self);
		}
	}
}

/* remove recv event. */
void socket_remove_recvevent (struct socketer *self)
{
}

/* set send event. */
void socket_setup_sendevent (struct socketer *self)
{
	DWORD w_length = 0;
	DWORD flags = 0;
	WSABUF buf;
	buf.len = 0;
	buf.buf = NULL;

	assert(self != NULL);
	assert(self->sendlock == 1);

	memset(&self->send_event, 0, sizeof(self->send_event));
	self->send_event.m_event = e_socket_io_event_write_end;
	if (WSASend(self->sockfd, &buf, 1, &w_length, flags, &self->send_event.m_overlap, 0) == SOCKET_ERROR)
	{
		/* overlapped operation failed to start. */
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			debuglog("socket_setup_sendevent error!, error:%d\n", WSAGetLastError());
			atom_dec(&self->sendlock);
			socketer_close(self);
		}
	}
}

/* remove send event. */
void socket_remove_sendevent (struct socketer *self)
{
}

/* set send data. */
void socket_senddata (struct socketer *self, char *data, int len)
{
	DWORD w_length = 0;
	DWORD flags = 0;
	WSABUF buf;
	buf.len = len;
	buf.buf = data;
	
	assert(self != NULL);
	assert(self->sendlock == 1);
	assert(data != NULL);
	assert(len > 0);

	memset(&self->send_event, 0, sizeof(self->send_event));
	self->send_event.m_event = e_socket_io_event_write_end;
	if (WSASend(self->sockfd, &buf, 1, &w_length, flags, &self->send_event.m_overlap, 0) == SOCKET_ERROR)
	{
		/* overlapped operation failed to start. */
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			debuglog("socket_senddata error!, error:%d\n", WSAGetLastError());
			atom_dec(&self->sendlock);
			socketer_close(self);
		}
	}
}

/* iocp work thread function. */
static void _iocp_thread_run (void *data)
{
	struct iocpmgr *mgr = (struct iocpmgr *)data;
	HANDLE cp = mgr->completeport;	/* complete port handle. */
	DWORD len = 0;					/* len variable is real transfers byte size. */
	ULONG_PTR s = (ULONG_PTR)NULL;	/* call = CreateIoCompletionPort((HANDLE), self->sockfd, s_iocp.completeport, (ULONG_PTR)self, 0); transfers self pointer. */
	struct overlappedstruct *ov = NULL;
	LPOVERLAPPED ol_ptr = NULL;		/* ol_ptr variable is io handle the overlap result, this is actually a very important parameter, because it is used for each I/O data operation .*/

	/*
	 * 10000 --- wait time. ms.
	 * INFINITE --- wait forever. 
	 * */
	while (mgr->isrun)
	{
		GetQueuedCompletionStatus(cp, &len, &s, &ol_ptr, INFINITE/*10000*/);
		if ((ol_ptr)&&(s))		
		{
			ov = CONTAINING_RECORD(ol_ptr, struct overlappedstruct, m_overlap);
			switch(ov->m_event)
			{	
			case e_socket_io_event_read_complete:	/* recv. */
				{
					debuglog("read handle complete! line:%d  thread_id:%d\n", __LINE__, CURRENT_THREAD);
					socketer_on_recv((struct socketer *)s);
				}break;
			case e_socket_io_event_write_end:		/* send. */
				{
					debuglog("send handle complete! line:%d thread_id:%d\n", __LINE__, CURRENT_THREAD);
					socketer_on_send((struct socketer *)s, (int)len);
				}break;
			case e_socket_io_thread_shutdown:
				{
					Sleep(100);
					free((void*)s);
					free(ov);
					return;
				}break;
			}
			ol_ptr = (LPOVERLAPPED)NULL;
			s = (ULONG_PTR)NULL;
			len = 0;
		}
	}
}

/* 
 * initialize event manager. 
 * socketnum --- socket total number. must greater than 1.
 * threadnum --- thread number, if less than 0, then start by the number of cpu threads 
 * */
bool eventmgr_init (int socketnum, int threadnum)
{
	if (s_iocp.isinit)
		return false;
	if (socketnum < 1)
		return false;
	if (threadnum <= 0)
		threadnum = get_cpunum();
	s_iocp.threadnum = threadnum;

	/* create complete port, fourthly parameter is 0. */
	s_iocp.completeport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
	if (!s_iocp.completeport)
		return false;

	s_iocp.isinit = true;
	s_iocp.isrun = true;

	/* create iocp work thread. */
	for (; threadnum > 0; --threadnum)
		_beginthread(_iocp_thread_run, 0, &s_iocp);

	/* initialize windows socket dll*/
	{
		WSADATA ws;
		WSAStartup(MAKEWORD(2,2), &ws);
	}
	return true;
}

/* 
 * release event manager.
 * */
void eventmgr_release ()
{
	int i;
	if (!s_iocp.isinit)
		return;
	for (i = 0; i < s_iocp.threadnum; ++i)
	{
		struct overlappedstruct *cs;
		struct socketer *sock;
		cs = (struct overlappedstruct *)malloc(sizeof(struct overlappedstruct));
		sock = (struct socketer *)malloc(sizeof(struct socketer));
		cs->m_event = e_socket_io_thread_shutdown;

		/* let iocp work thread exit. */
		PostQueuedCompletionStatus(s_iocp.completeport, 0, (ULONG_PTR)sock, &cs->m_overlap);
	}
	s_iocp.isrun = false;
	s_iocp.isinit = false;
	Sleep(500);
	CloseHandle(s_iocp.completeport);
	WSACleanup();
}

#endif

