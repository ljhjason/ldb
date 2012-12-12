
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_ONETIMEID_H_
#define _H_ONETIMEID_H_
#ifdef __cplusplus
extern "C"{
#endif

#include "alway_inline.h"
#include "configtype.h"

/**
 * init onetimeid module.
 * valid id is big to 0.
 * */
bool onetimeid_init ();

/**
 * create new onetimeid.
 * */
int64 onetimeid_create ();

/**
 * check id is valid.
 * */
bool onetimeid_isvalid (int64 id);

#ifdef __cplusplus
}
#endif
#endif
