
/*
 * Copyright (C) lcinx
 * lcinx@163.com 
*/

#ifndef _H_PROCESS_INFO_H_
#define _H_PROCESS_INFO_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "alway_inline.h"

struct processinfo
{
	double cur_mem;
	double max_mem;
	double cur_vm;
	double max_vm;
	int cur_cpu;
	int max_cpu;
	int cpunum;
	int threadnum;
};

/**
 * get the current process info.
 * note: memory unit: KB.
 * */
void processinfo_get (struct processinfo *state);

/**
 * update current process info.
 * */
void processinfo_update ();

#ifdef __cplusplus
}
#endif

#endif

