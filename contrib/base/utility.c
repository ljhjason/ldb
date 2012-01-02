
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <stdio.h>
#include "utility.h"

#ifdef WIN32
#include <objbase.h>
#define snprintf _snprintf
#else
#include <unistd.h>
#include <fcntl.h>
#include <uuid/uuid.h>
typedef struct _GUID
{
	unsigned int Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
}GUID, UUID;
#endif

/**
 * create guid string.
 * if bufsize not enough, return false, else return true.
 * note:
 * guid string has '\0'.
 * */
bool create_guid (char *buf, size_t bufsize)
{
	GUID guid;
	if (!buf || bufsize < 37)
		return false;

#ifdef WIN32
	CoCreateGuid(&guid);
#else
	uuid_generate((unsigned char *)(&guid));
#endif

	snprintf(buf, bufsize, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", 
			guid.Data1, guid.Data2, guid.Data3, 
			guid.Data4[0], guid.Data4[1],
			guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5],
			guid.Data4[6], guid.Data4[7]);
	
	buf[bufsize - 1] = '\0';
	return true;
}

/**
 * get file full name.
 * It is best to use the large enough buffer, such as 4 KB.
 * */
void getfilefullname (const char *filename, char *buf, size_t bufsize)
{
#ifdef WIN32
	FILE *fp;
#endif
	if (!filename || !buf || bufsize < 1)
		return;
#ifdef WIN32
	GetFullPathName(filename, bufsize - 1, buf, NULL);
	fp = fopen(filename, "r");
	if (!fp)
		buf[0] = '\0';
	else
		fclose(fp);
#else
	int fd = open(filename, O_RDWR);
	char srcname[1024*8];
	snprintf(srcname, sizeof(srcname)-1, "/proc/%ld/fd/%d", (long)getpid(), fd);
	readlink(srcname, buf, bufsize - 1);
	close(fd);
#endif

	buf[bufsize - 1] = '\0';
}

/**
 * get current path.
 * It is best to use the large enough buffer, such as 4 KB.
 * */
void getcurrentpath (char *buf, size_t bufsize)
{
	if (!buf || bufsize < 1)
		return;
#ifdef WIN32
	GetCurrentDirectory(bufsize - 1, buf);
#else
	getcwd(buf, bufsize - 1);
#endif

	buf[bufsize - 1] = '\0';
}

