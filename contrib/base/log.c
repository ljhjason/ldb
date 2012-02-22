
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include "log.h"
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef WIN32
#include <direct.h>
#include <io.h>
#define my_mkdir _mkdir
#define snprintf _snprintf
#else
#include <sys/stat.h>
#include <sys/types.h>
#define my_mkdir(a) mkdir((a), 0777)
#endif

struct fileinfo
{
	FILE *fp;
	bool logtime;
	char last_filename[256];
};

struct filelog
{
	bool isfree;
	char directname[512];		/*log file root directory*/
	struct fileinfo loggroup[enum_log_type_max];
};

struct logmgr
{
	bool isinit;
	struct filelog logobj;
};
#ifdef __GNUC__
static struct logmgr s_log = {.isinit = false};
#elif defined(_MSC_VER)
static struct logmgr s_log = {false};
#endif

/* default root directory and default file name*/
static const char *s_default_filename[enum_log_type_max] = {"_assert_assert.txt", "_error_log_.txt", ""};
static const char *s_default_direct_name = "log_log";

static bool s_show[enum_debug_max] = {true, false, false, false};

static void filelog_init (struct filelog *self)
{
	int i;
	self->isfree = false;
	strncpy(self->directname, s_default_direct_name, sizeof(self->directname)-1);
	self->directname[sizeof(self->directname)-1] = '\0';
	for (i = 0; i < enum_log_type_max; ++i)
	{
		self->loggroup[i].fp = NULL;
		self->loggroup[i].logtime = true;
		memset(self->loggroup[i].last_filename, 0, sizeof(self->loggroup[i].last_filename));
	}
}

struct filelog *filelog_create ()
{
	struct filelog *self = (struct filelog *)malloc(sizeof(struct filelog));
	if (!self)
		return NULL;
	filelog_init(self);
	return self;
}

void filelog_release (struct filelog *self)
{
	int i;
	if (!self)
		return;
	assert(!self->isfree);
	for (i = 0; i < enum_log_type_max; ++i)
	{
		if (self->loggroup[i].fp)
		{
			fclose(self->loggroup[i].fp);
			self->loggroup[i].fp = NULL;
		}
	}
	self->isfree = true;
	free(self);
}

bool filelog_logtime (struct filelog *self, bool flag)
{
	bool prev;
	if (!self)
		return true;

	prev = self->loggroup[enum_log_type_log].logtime;
	self->loggroup[enum_log_type_log].logtime = flag;
	return prev;
}

int mymkdir (const char *directname)
{
	return my_mkdir(directname);
}

/* log write file spend time */
/*#define _TEST_WRITE_LOG_NEED_TIME*/

#ifdef _TEST_WRITE_LOG_NEED_TIME
#include "crosslib.h"
#endif

void _log_printf_ (unsigned int type, const char *filename, const char *func, long line, const char *fmt, ...)
{
	va_list args;
	assert(type < enum_debug_max);
	assert(fmt != NULL);
	if (type >= enum_debug_max)
		return;
	if (!s_show[type])
		return;
	if (!fmt)
		return;
	if (enum_debug_for_assert == type)
	{
		printf("file:%s, function:%s, line:%ld ", filename, func, line);
	}
	else if (enum_debug_for_time_debug == type)
	{
		time_t tval;
		struct tm *currTM;
		time(&tval);
		currTM = localtime(&tval);
		printf("[%04d-%02d-%02d %02d:%02d:%02d]:", currTM->tm_year+1900, currTM->tm_mon+1, currTM->tm_mday, 
				currTM->tm_hour, currTM->tm_min, currTM->tm_sec);
	}
	else
	{
	}
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}

/* sshow/hide log_debug function out*/
void log_setdebug (bool show)
{
	s_show[enum_debug_for_debug] = show;
}

/* sshow/hide log_showlog function out*/
void log_setshow (bool show)
{
	s_show[enum_debug_for_log] = show;
}

/* show/hide log_timelog function out*/
void log_settimelog (bool show)
{
	s_show[enum_debug_for_time_debug] = show;
}

static inline void logmgr_init ()
{
	if (s_log.isinit)
		return;
	s_log.isinit = true;
	filelog_init(&s_log.logobj);
}

void _log_write_ (struct filelog *log, unsigned int type, const char *filename, const char *func, long line, const char *fmt, ...)
{
	const char *directname;
	struct fileinfo *info;
	time_t tval;
	struct tm *currTM;
	char szDate[512] = {0};
	char szFile[512] = {0};
	char szPath[512] = {0};
#ifdef _TEST_WRITE_LOG_NEED_TIME
	int64 begin, end;
#endif

	if (!log)
	{
		if (!s_log.isinit)
			logmgr_init();
		info = &s_log.logobj.loggroup[type];
		directname = s_log.logobj.directname;
	}
	else
	{
		info = &log->loggroup[type];
		directname = log->directname;
	}

	if (!fmt)
		return;
	time(&tval);
	currTM = localtime(&tval);
	switch (type)
	{
	case enum_log_type_assert:
		{
			my_mkdir(directname);
			snprintf(szFile, sizeof(szFile)-1, "%s/%s", directname, s_default_filename[type]);
		}
		break;
	case enum_log_type_error:
		{
			my_mkdir(directname);
			snprintf(szFile, sizeof(szFile)-1, "%s/%s", directname, s_default_filename[type]);
		}
		break;
	case enum_log_type_log:
		{
			snprintf(szDate, sizeof(szDate)-1, "%04d-%02d-%02d", currTM->tm_year+1900, currTM->tm_mon+1, currTM->tm_mday);
			snprintf(szPath, sizeof(szPath)-1, "%s/%s", directname, szDate);
			my_mkdir(directname);
			my_mkdir(szPath);
			snprintf(szFile, sizeof(szFile)-1, "%s/%02d.log", szPath, currTM->tm_hour);
		}
		break;
	default:
		{
			assert(false && "_log_write_ error type...");
			return;
		}
	}

	if (strcmp(info->last_filename, szFile) != 0)
	{
		strncpy(info->last_filename, szFile, sizeof(info->last_filename)-1);
		info->last_filename[sizeof(info->last_filename)-1] = 0;
		if (info->fp)
		{
			fclose(info->fp);
			info->fp = NULL;
		}	
	}
	if (!info->fp)
		info->fp = fopen(szFile, "a");

#ifdef _TEST_WRITE_LOG_NEED_TIME
	begin = get_microsecond();
#endif
	if (info->fp)
	{
		va_list args;
		if (enum_log_type_log == type)
		{
			if (info->logtime)
				fprintf(info->fp, "[%s %02d:%02d:%02d]  ",	szDate,
				currTM->tm_hour, currTM->tm_min, currTM->tm_sec);
		}
		else
		{
			fprintf(info->fp, "[%d/%02d/%02d %02d:%02d:%02d] [file:%s, function:%s, line:%ld] ", 
			currTM->tm_year+1900, currTM->tm_mon+1, currTM->tm_mday, currTM->tm_hour, currTM->tm_min, currTM->tm_sec, 
			filename, func,	line);
		}
		va_start(args, fmt);
		vfprintf(info->fp, fmt, args);
		va_end(args);
		fprintf(info->fp, "\n");

#ifdef _TEST_WRITE_LOG_NEED_TIME
		end = get_microsecond();
		fprintf(info->fp, "need: %ld us\n", (long)(end - begin));
		fflush(info->fp);
		begin = get_microsecond();
		fprintf(info->fp, "fflush need:%ld us\n", (long)(begin - end));
#endif	
		fflush(info->fp);
	}
}

bool log_logtime (bool flag)
{
	bool prev;
	if (!s_log.isinit)
		logmgr_init();

	prev = s_log.logobj.loggroup[enum_log_type_log].logtime;
	s_log.logobj.loggroup[enum_log_type_log].logtime = flag;
	return prev;
}

static void _setloginfo_ (struct filelog *log, const char *directname)
{
	int i;
	if (!directname)
		return;
	if (strcmp(directname, "") == 0)
		return;
	if (strcmp(directname, log->directname) == 0)
		return;
	strncpy(log->directname, directname, sizeof(log->directname)-1);
	log->directname[sizeof(log->directname)-1] = 0;
	for (i = 0; i < enum_log_type_max; ++i)
	{
		if (log->loggroup[i].fp)
		{
			fclose(log->loggroup[i].fp);
		}
		log->loggroup[i].fp = NULL;
		memset(log->loggroup[i].last_filename, 0, sizeof(log->loggroup[i].last_filename));
	}
}

void _log_setdirect_ (struct filelog *log, const char *directname)
{
	/* default log. */
	if (!log)
	{
		if (!s_log.isinit)
			logmgr_init();
		_setloginfo_(&s_log.logobj, directname);
	}
	else
	{
		_setloginfo_(log, directname);
	}
}


