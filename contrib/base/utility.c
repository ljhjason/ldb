
/*
 * Copyright (C) lcinx
 * lcinx@163.com
*/

#include <stdio.h>
#include "utility.h"
#include "ossome.h"
#include "crosslib.h"

#ifdef WIN32
#include <objbase.h>
#include <psapi.h>
#define snprintf _snprintf
#else
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
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
	snprintf(srcname, sizeof(srcname)-1, "/proc/%d/fd/%d", getpid(), fd);
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

/**
 * get the directory available disk size, the unit: byte.
 * */
int64 getdirectory_freesize (const char *dirpath)
{
	int64 freesize = 0;
	if (!dirpath)
		return 0;

#ifdef WIN32
	{
		ULARGE_INTEGER   lpuse;   
		ULARGE_INTEGER   lptotal;   
		ULARGE_INTEGER   lpfree;
		if (GetDiskFreeSpaceEx(dirpath, &lpuse, &lptotal, &lpfree))
			freesize = lpfree.QuadPart;
	}
#else
	{
		struct statfs fs;
		if (!statfs(dirpath, &fs))
			freesize = fs.f_bsize * fs.f_bavail;
	}
#endif

	return freesize;
}

struct processinfomgr
{
	bool isinit;
	int cpunum;
	double currentmemsize;
	double maxmemsize;
	int cpurate;
	int maxcpurate;
	int threadnum;

	int64 lastupdatetime;
};

#ifdef __GNUC__
static struct processinfomgr s_mgr = {.isinit = false};
#elif defined(_MSC_VER)
static struct processinfomgr s_mgr = {false};
#endif

#ifdef WIN32
typedef struct _THREAD_INFO
{
	LARGE_INTEGER CreateTime;
	DWORD dwUnknown1;
	DWORD dwStartAddress;
	DWORD StartEIP;
	DWORD dwOwnerPID;
	DWORD dwThreadId;
	DWORD dwCurrentPriority;
	DWORD dwBasePriority;
	DWORD dwContextSwitches;
	DWORD Unknown;
	DWORD WaitReason;
}THREADINFO, *PTHREADINFO;

typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} UNICODE_STRING;

typedef struct _PROCESS_INFO
{
	DWORD dwOffset;
	DWORD dwThreadsCount;
	DWORD dwUnused1[6];
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ProcessName;
	DWORD dwBasePriority;
	DWORD dwProcessID;
	DWORD dwParentProcessId;
	DWORD dwHandleCount;
	DWORD dwUnused3[2];
	DWORD dwVirtualBytesPeak;
	DWORD dwVirtualBytes;
	ULONG dwPageFaults;
	DWORD dwWorkingSetPeak;
	DWORD dwWorkingSet;
	DWORD dwQuotaPeakPagedPoolUsage;
	DWORD dwQuotaPagedPoolUsage;
	DWORD dwQuotaPeakNonPagedPoolUsage;
	DWORD dwQuotaNonPagedPoolUsage;
	DWORD dwPageFileUsage;
	DWORD dwPageFileUsagePeak;
	DWORD dCommitCharge;
	THREADINFO ThreadSysInfo[1];
} PROCESSINFO, *PPROCESSINFO;

static void GetProessCPU (DWORD dwProcessId, int *cpurate, int *threadnum)
{
	int nCpuUsage    = 0;
	char tmp[0x20000];
	DWORD dwInfoSize = sizeof(tmp);
	PPROCESSINFO pProcessInfo;
	DWORD dwWorkingSet;
	long ( __stdcall *NtQuerySystemInformation )( DWORD, PVOID, DWORD, DWORD);
	static __int64 nLastTotalProcessCPUUsage   = 0;
	static __int64 nLastCurrentProcessCPUUsage = 0;
	__int64 nTotalProcessCPUUsage   = 0;
	__int64 nCurrentProcessCPUUsage = 0;
	__int64 nTotalDelta;
	__int64 nCurrentDelta;

	PVOID pProcInfo = (PVOID)tmp;
	NtQuerySystemInformation = (long(__stdcall*)(DWORD,PVOID,DWORD,DWORD))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQuerySystemInformation");
	NtQuerySystemInformation(5, pProcInfo, dwInfoSize, 0);
	pProcessInfo = (PPROCESSINFO)pProcInfo;
	do
	{
		nTotalProcessCPUUsage += (__int64)pProcessInfo->KernelTime.QuadPart + (__int64)pProcessInfo->UserTime.QuadPart;
		if (pProcessInfo->dwProcessID == dwProcessId)
		{
			dwWorkingSet = pProcessInfo->dwWorkingSet;
			nCurrentProcessCPUUsage += (__int64)pProcessInfo->KernelTime.QuadPart + (__int64)pProcessInfo->UserTime.QuadPart;

			*threadnum = pProcessInfo->dwThreadsCount;
		}
		if (pProcessInfo->dwOffset == 0)
		{
			break;
		}
		pProcessInfo = (PPROCESSINFO)((char*)pProcessInfo + pProcessInfo->dwOffset);
	} while(true);

	nTotalDelta   = nTotalProcessCPUUsage - nLastTotalProcessCPUUsage;
	nCurrentDelta = nCurrentProcessCPUUsage - nLastCurrentProcessCPUUsage;
	if (nTotalDelta > 0 && nCurrentDelta >= 0)
	{
	   nCpuUsage = (int)(100 * nCurrentDelta / nTotalDelta);
	}
	nLastTotalProcessCPUUsage   = nTotalProcessCPUUsage;
	nLastCurrentProcessCPUUsage = nCurrentProcessCPUUsage;

	*cpurate = nCpuUsage;	
}

static void getprocess_meminfo (double *currentmemsize, double *maxmemsize)
{
	HANDLE handle = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS mem;
	if (GetProcessMemoryInfo(handle, &mem, sizeof(mem)))
	{
		*currentmemsize = mem.WorkingSetSize / 1024;
		*maxmemsize = mem.PeakWorkingSetSize / 1024;
	}
}

static void getprocess_someinfo (struct processinfomgr *mgr)
{
	getprocess_meminfo(&mgr->currentmemsize, &mgr->maxmemsize);
	GetProessCPU(GetCurrentProcessId(), &mgr->cpurate, &mgr->threadnum);
	mgr->cpurate *= mgr->cpunum;
}

#else

#define STR_VmHWM "VmHWM"
#define STR_VmRSS "VmRSS"
#define STR_Threads "Threads"
static void getprocess_memsomeinfo (double *currentmemsize, double *maxmemsize, int *threadnum)
{
	char path[1024*4];
	char buf[1024*8];
	int readnum;
	char *str, *next, *name, *value;
	int fd;
	struct stat statbuf;
	snprintf(path, sizeof(path) - 1, "/proc/%d/status", getpid());
	if (stat(path, &statbuf))
		return;

	fd = open(path, O_RDONLY, 0);
	if (fd == -1)
		return;
	readnum = read(fd, buf, (int)(sizeof(buf) - 1));
	close(fd);
	if (readnum <= 0)
		return;

	buf[readnum] = '\0';

	next = buf;
	for (;;)
	{
		str = next;
		if (!str)
			break;

		next = strchr(str, '\n');
		if (next)
		{
			*next = '\0';
			next++;
		}

		value = strchr(str, ':');
		if (!value)
			break;
		name = str;
		*value = '\0';
		value++;
		while (*value != '\0')
		{
			if (isgraph(*value) != 0)
				break;

			value++;
		}

		if (strcmp(name, STR_VmHWM) == 0)
		{
			sscanf(value, "%lf kB", maxmemsize);
		}
		else if (strcmp(name, STR_VmRSS) == 0)
		{
			sscanf(value, "%lf kB", currentmemsize);
		}
		else if (strcmp(name, STR_Threads) == 0)
		{
			sscanf(value, "%d", threadnum);
		}
	}
}

static void getprocess_cpusomeinfo (int *cpurate)
{
	double tmp;
	char buf[1024];
	char cmd[1024];
	char *str;
	FILE *fp;
	int readnum;
	int i;
	snprintf(cmd, sizeof(cmd) - 1, "ps -eo pcpu,pid|grep %d", getpid());
	fp = popen(cmd, "r");
	readnum = fread(buf, 1, sizeof(buf) - 1, fp);
	pclose(fp);
	if (readnum <= 0)
		return;
	buf[readnum] = '\0';
	i = 0;
	while (buf[i] != '\0')
	{
		if (isgraph(buf[i]) != 0)
			break;
		i++;
	}
	str = &buf[i];
	while (buf[i] != '\0')
	{
		if (isgraph(buf[i]) == 0)
		{
			buf[i] = '\0';
			break;
		}
		i++;
	}
	sscanf(str, "%lf", &tmp);
	*cpurate = (int)tmp;
}

static void getprocess_someinfo (struct processinfomgr *mgr)
{
	getprocess_memsomeinfo(&mgr->currentmemsize, &mgr->maxmemsize, &mgr->threadnum);
	getprocess_cpusomeinfo(&mgr->cpurate);
}
#endif

/**
 * get the current process info.
 * */
void processinfo_get (double *currentmemsize, double *maxmemsize, int *cpurate, int *maxcpurate,
		int *cpunum, int *threadnum)
{
	if (!s_mgr.isinit)
	{
		processinfo_update();
	}

	*currentmemsize = s_mgr.currentmemsize;
	*maxmemsize = s_mgr.maxmemsize;
	*cpurate = s_mgr.cpurate;
	*maxcpurate = s_mgr.maxcpurate;
	*cpunum = s_mgr.cpunum;
	*threadnum = s_mgr.threadnum;
}

/**
 * update current process info.
 * */
void processinfo_update ()
{
	int64 currenttime = get_millisecond();
	if (!s_mgr.isinit)
	{
		s_mgr.isinit = true;
		s_mgr.cpunum = get_cpunum();
		s_mgr.currentmemsize = 0;
		s_mgr.maxmemsize = 0;
		s_mgr.cpurate = 0;
		s_mgr.maxcpurate = 0;
		s_mgr.threadnum = 1;
		s_mgr.lastupdatetime = 0;
	}

	/*
	 * Spending big, so every 5 seconds on a sampling.
	 * */
	if (currenttime - s_mgr.lastupdatetime < 5000)
		return;

	s_mgr.lastupdatetime = currenttime;
	getprocess_someinfo(&s_mgr);

	if (s_mgr.maxcpurate < s_mgr.cpurate)
		s_mgr.maxcpurate = s_mgr.cpurate;
}


