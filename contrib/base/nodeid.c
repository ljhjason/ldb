
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <assert.h>
#include <time.h>
#include <string.h>
#include "nodeid.h"
#include "crosslib.h"

static const int max_nodeid = 10000;
static const int max_workerid = 2000;

struct nodeidmgr
{
	bool isinit;
	int nodeid;
	int64 lasttimestamp;
	int64 lastworkerid;
};

#ifdef __GNUC__
static struct nodeidmgr s_mgr = {.isinit = false};
#elif defined(_MSC_VER)
static struct nodeidmgr s_mgr = {false};
#endif

/**
 * nodeid bit of information
 * 64-62	62-30		30-28	28-16		16-1
 *			32bit		2bit	12bit		16bit
 *			timestamp	idtype	workerid	nodeid
 * */

enum
{
	nodeid_type_node = 0,	//is node
	nodeid_type_worker = 1,	//is worker
};

/**
 * init nodeid module by node id.
 * valid node id from 1 to 10000.
 * */
bool nodeid_init (int nodeid)
{
	if (s_mgr.isinit)
		return false;
	if (nodeid <= 0 || nodeid > max_nodeid)
		return false;

	delay_delay(2100);

	s_mgr.isinit = true;
	s_mgr.nodeid = nodeid;
	s_mgr.lasttimestamp = 1;
	s_mgr.lastworkerid = 1;
	return true;
}

/**
 * get id type.
 * */
static int nodeid_gettype (int64 id)
{
	int64 tmp = id >> 28;
	return (int)(tmp & 0x3);
}

/**
 * check id is valid.
 * */
bool nodeid_isvalid (int64 id)
{
	int tp = nodeid_gettype(id);
	int nodepart = nodeid_nodepart(id);
	int workerpart = nodeid_workerpart(id);
	int timestamp = nodeid_timestamp(id);
	if (id <= 0)
		return false;
	if (tp != nodeid_type_node &&
		tp != nodeid_type_worker)
		return false;
	if (nodepart <= 0 || nodepart > max_nodeid)
		return false;
	if (tp == nodeid_type_node)
	{
		if (workerpart != 0 ||
			timestamp != 0)
			return false;
	}
	else if (tp == nodeid_type_worker)
	{
		if (workerpart <= 0 ||
			timestamp < 0)
			return false;
	}
	else
	{
		return false;
	}

	return true;
}

/**
 * test whether id for node id.
 * */
bool nodeid_isnode (int64 id)
{
	if (nodeid_isvalid(id) && (nodeid_gettype(id) == nodeid_type_node))
		return true;

	return false;
}

/**
 * test whether id for worker id.
 * */
bool nodeid_isworker (int64 id)
{
	if (nodeid_isvalid(id) && (nodeid_gettype(id) == nodeid_type_worker))
		return true;

	return false;
}

/**
 * test two id whether to belong to the same node.
 * */
bool nodeid_isinsamenode (int64 ida, int64 idb)
{
	if (nodeid_isvalid(ida) && nodeid_isvalid(idb) 
			&& (nodeid_nodepart(ida) == nodeid_nodepart(idb)))
		return true;

	return false;
}

/**
 * create worker id, 0 is invalid.
 * */
int64 nodeid_createworker ()
{
	int64 id;
	int64 curtm;
	assert(s_mgr.isinit);
	if (!s_mgr.isinit)
		return 0;
rebegin:

	id = 0;
	curtm = time(NULL) & 0xffffffff;
	if (s_mgr.lasttimestamp != curtm)
	{
		s_mgr.lasttimestamp = curtm;
		s_mgr.lastworkerid = 1;
	}
	else
	{
		if (s_mgr.lastworkerid + 1 >= max_workerid)
		{
			delay_delay(30);
			goto rebegin;
		}
	}

	id |= nodeid_type_worker;
	id <<= 28;
	id |= s_mgr.lastworkerid << 16;
	id |= s_mgr.lasttimestamp << 30;
	id |= s_mgr.nodeid;

	s_mgr.lastworkerid++;
	return id;
}

/**
 * get node part.
 * */
int nodeid_nodepart (int64 id)
{
	return (int)(id & 0xffff);
}

/**
 * get worker part.
 * */
int nodeid_workerpart (int64 id)
{
	int64 tmp = id >> 16;
	return (int)(tmp & 0xfff);
}

/**
 * get timestamp part.
 * */
int nodeid_timestamp (int64 id)
{
	int64 tmp = id >> 30;
	return (int)(tmp & 0xffffffff);
}

/**
 * get max nodeid.
 * */
int nodeid_maxnodeid ()
{
	return max_nodeid;
}

