
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_C_ID_MGR_H_
#define _H_C_ID_MGR_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "alway_inline.h"

struct idmgr;

/* 
 * maxid is total, valid id is 1 --- maxid.
 * delaytime is delay release time, when call idmgr_freeid, delay delaytime later, real free.
 * */
struct idmgr *idmgr_create (int maxid, int delaytime);

int idmgr_total (struct idmgr *self);

int idmgr_usednum (struct idmgr *self);

void idmgr_release (struct idmgr *self);

int idmgr_allocid (struct idmgr *self);

bool idmgr_freeid (struct idmgr *self, int id);

void idmgr_run (struct idmgr *self);

#ifdef __cplusplus
}
#endif
#endif

