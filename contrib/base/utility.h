
/*
 * Copyright (C) lcinx
 * lcinx@163.com 
*/

#ifndef _H_UTILITY_H_
#define _H_UTILITY_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stddef.h>
#include "alway_inline.h"
#include "configtype.h"

/**
 * create guid string.
 * if bufsize not enough, return false, else return true.
 * note:
 * guid string has '\0'.
 * */
bool create_guid (char *buf, size_t bufsize);

/**
 * get file full name.
 * It is best to use the large enough buffer, such as 4 KB.
 * */
void getfilefullname (const char *filename, char *buf, size_t bufsize);

/**
 * get current path.
 * It is best to use the large enough buffer, such as 4 KB.
 * */
void getcurrentpath (char *buf, size_t bufsize);

/**
 * get the directory available disk size, the unit: byte.
 * */
int64 getdirectory_freesize (const char *dirpath);

#ifdef __cplusplus
}
#endif
#endif

