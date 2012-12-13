
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#ifndef _H_NODEID_H_
#define _H_NODEID_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "alway_inline.h"
#include "configtype.h"

/**
 * init nodeid module by node id.
 * valid node id from 1 to 10000.
 * */
bool nodeid_init (int nodeid);

/**
 * check id is valid.
 * */
bool nodeid_isvalid (int64 id);

/**
 * test whether id for node id.
 * */
bool nodeid_isnode (int64 id);

/**
 * test whether id for worker id.
 * */
bool nodeid_isworker (int64 id);

/**
 * test two id whether to belong to the same node.
 * */
bool nodeid_isinsamenode (int64 ida, int64 idb);

/**
 * create worker id, 0 is not valid.
 * */
int64 nodeid_createworker ();

/**
 * get node part.
 * */
int nodeid_nodepart (int64 id);

/**
 * get worker part.
 * */
int nodeid_workerpart (int64 id);

/**
 * get timestamp part.
 * */
int64 nodeid_timestamp (int64 id);

/**
 * get max nodeid.
 * */
int nodeid_maxnodeid ();

#ifdef __cplusplus
}
#endif
#endif

