
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <assert.h>
#include <time.h>
#include <string.h>
#include "onetimeid.h"
#include "crosslib.h"

static const int max_idpart = 0x1fffffff;

struct onetimeidmgr
{
	bool isinit;
	int64 lasttimestamp;
	int64 lastid;
};

#ifdef __GNUC__
static struct onetimeidmgr s_mgr = {.isinit = false};
#elif defined(_MSC_VER)
static struct onetimeidmgr s_mgr = {false};
#endif

/**
 * onetimeid bit of information
 * 64-62	62-30		30-1
 *			32bit		30bit
 *			timestamp	id
 * */

/**
 * init onetimeid module.
 * valid id is big to 0.
 * */
bool onetimeid_init ()
{
	if (s_mgr.isinit)
		return false;

	delay_delay(1100);

	s_mgr.isinit = true;
	s_mgr.lasttimestamp = 1;
	s_mgr.lastid = 1;
	return true;
}

/**
 * create new onetimeid.
 * */
int64 onetimeid_create ()
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
		s_mgr.lastid = 1;
	}
	else
	{
		if (s_mgr.lastid + 1 >= max_idpart)
		{
			delay_delay(30);
			goto rebegin;
		}
	}

	id |= (s_mgr.lasttimestamp << 30);
	id |= s_mgr.lastid;

	s_mgr.lastid++;
	return id;
}

static int onetimeid_idpart (int64 id)
{
	return (int)(id & 0x3fffffff);
}

/**
 * check id is valid.
 * */
bool onetimeid_isvalid (int64 id)
{
	if (id > 0 && 
		((id >> 62) == 0) &&
		onetimeid_idpart(id) < max_idpart)
		return true;

	return false;
}

