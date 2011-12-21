
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <stdlib.h>
#include <assert.h>
#include "idmgr.h"
#include "log.h"
#include "crosslib.h"

enum enum_id_use_type
{
	enum_run_time_distance = 300,			/* run delay time. --- ms */

#ifndef NDEBUG
	enum_id_flag_id_un_used = 0x54354398,	/* un used. */
	enum_id_flag_id_is_used = 0x13213289,	/* useing. */
	enum_id_flag_id_waite_free = 0x76876801,/* waite real free. */
#endif
};

struct idnode
{
	int id;

#ifndef NDEBUG
	unsigned long usetype;		/* used type. */
#endif

	struct idnode *next;
	int64 realfreetime;
};

struct idmgr
{
	int usesize;				/* alloced num. */
	int maxid;					/* id total. */
	int delaytime;				/* delay release time. */
	int64 lastrun;				/* last run time. */
	struct idnode *array;
	struct idnode *freelist;

	struct idnode *waite_head;
	struct idnode *waite_tail;
};

static inline void idnode_init (struct idnode *self, int id)
{
	self->id = id;

#ifndef NDEBUG
	self->usetype = enum_id_flag_id_is_used;
#endif

	self->realfreetime = 0;
	self->next = NULL;
}

#ifndef NDEBUG
static bool idmgr_findid_infreelist (struct idmgr *self, int id)
{
	if (self->array[id].usetype == enum_id_flag_id_un_used)
		return true;
	return false;
}
#endif

static inline void idmgr_pushto_freelist (struct idmgr *self, struct idnode *node)
{
	assert(!idmgr_findid_infreelist(self, node->id));
	node->next = self->freelist;
	self->freelist = node;
#ifndef NDEBUG
	node->usetype = enum_id_flag_id_un_used;
#endif
}

/* 
 * maxid is total, valid id is 1 --- maxid.
 * delaytime is delay release time, when call idmgr_freeid, delay delaytime later, real free.
 * */
struct idmgr *idmgr_create (int maxid, int delaytime)
{
	int i = 0;
	struct idmgr *self = NULL;
	assert(maxid > 0 && "idmgr_create maxid need greater than zero!");
	assert(delaytime > 0 && "idmgr_create delaytime need greater than zero!");
	if (maxid <= 0 || delaytime <= 0)
		return NULL;
	self = (struct idmgr *)malloc(sizeof(struct idmgr));
	if (!self)
	{
		assert(false && "idmgr_create malloc memory failed!");
		return NULL;
	}
	self->usesize = 0;
	self->maxid = maxid;
	self->delaytime = delaytime;
	self->lastrun = 0;
	self->array = NULL;
	self->freelist = NULL;
	self->waite_head = NULL;
	self->waite_tail = NULL;

	maxid += 1;
	self->array = (struct idnode*)malloc(sizeof(struct idnode) * maxid);
	if (!self->array)
	{
		assert(false && "idmgr_create malloc memory failed!");
		log_error("idmgr_create malloc memory failed!");
		return NULL;
	}
	
	for (i = maxid-1; i > 0; --i)
	{
		idnode_init(&self->array[i], i);
		idmgr_pushto_freelist(self, &self->array[i]);
	}
	return self;
}

int idmgr_total (struct idmgr *self)
{
	assert(self != NULL);
	if (!self)
		return 0;
	return self->maxid;
}

int idmgr_usednum (struct idmgr *self)
{
	assert(self != NULL);
	if (!self)
		return 0;
	return self->usesize;
}

void idmgr_release (struct idmgr *self)
{
	assert(self != NULL);
	if (!self)
		return;
	assert(self->array != NULL);
	free(self->array);
	self->array = NULL;
	self->freelist = NULL;
	self->waite_head = NULL;
	self->waite_tail = NULL;
	assert((0 == self->usesize) && "idmgr_release has id not free!");
	free(self);
}

int idmgr_allocid (struct idmgr *self)
{
	int temp = 0;
	struct idnode *id = NULL;
	assert(self != NULL);
	if (!self)
		return temp;
	id = self->freelist;
	if (id != NULL)
	{
		temp = id->id;
		assert(temp > 0);
		assert(enum_id_flag_id_un_used == id->usetype);
		self->freelist = id->next;
		++self->usesize;

#ifndef NDEBUG
		id->usetype = enum_id_flag_id_is_used;
#endif
		
	}
	return temp;
}

static inline void idmgr_pushto_waitelist (struct idmgr *self, struct idnode *node)
{
	assert(self != NULL);
	assert(node != NULL);
	node->next = NULL;

#ifndef NDEBUG
	node->usetype = enum_id_flag_id_waite_free;
#endif

	node->realfreetime = get_millisecond() + self->delaytime;
	if (!self->waite_tail)
	{
		self->waite_head = node;
		self->waite_tail = node;
	}
	else
	{
		self->waite_tail->next = node;
		self->waite_tail = node;
	}
}

bool idmgr_freeid (struct idmgr *self, int id)
{
	struct idnode *temp = NULL;
	assert(self != NULL);
	assert((id > 0 && id <= self->maxid) && "idmgr_freeid this id is not in idmgr!");
	if (!self || id <= 0 || id > self->maxid)
		return false;
	assert((!idmgr_findid_infreelist(self, id)) && "idmgr_freeid repeat free id!");
	temp = &self->array[id];

	assert((enum_id_flag_id_un_used == temp->usetype || enum_id_flag_id_is_used == temp->usetype || enum_id_flag_id_waite_free == temp->usetype) && "idmgr_freeid flag is bad!");
	assert((enum_id_flag_id_is_used == temp->usetype) && "idmgr_freeid repeat free id!");
	idmgr_pushto_waitelist(self, temp);
	--self->usesize;
	return true;
}

void idmgr_run (struct idmgr *self)
{
	int64 currenttime = 0;
	struct idnode *temp = NULL;
	assert(self != NULL);
	if (!self || !self->waite_head)
		return;
	currenttime = get_millisecond();
	if (currenttime - self->lastrun < enum_run_time_distance)
		return;
	self->lastrun = currenttime;
	for (;;)
	{
		if (!self->waite_head)
			break;
		if (currenttime < self->waite_head->realfreetime)
			break;
		
		temp = self->waite_head;
		self->waite_head = temp->next;
		if (!self->waite_head)
		{
			self->waite_head = NULL;
			self->waite_tail = NULL;
		}
		idmgr_pushto_freelist(self, temp);
	}
}



