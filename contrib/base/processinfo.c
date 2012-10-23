
/*
 * Copyright (C) lcinx
 * lcinx@163.com 
*/

#include <stdio.h>
#include "processinfo.h"
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
#endif

struct processinfomgr
{
	bool isinit;
	struct processinfo state;
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

static void getprocess_meminfo (struct processinfo *state)
{
	HANDLE handle = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS mem;
	if (GetProcessMemoryInfo(handle, &mem, sizeof(mem)))
	{
		state->cur_mem = mem.WorkingSetSize / 1024;
		state->max_mem = mem.PeakWorkingSetSize / 1024;
		state->cur_vm = mem.PagefileUsage / 1024;
		state->max_vm = mem.PeakPagefileUsage / 1024;
	}
}

static void getprocess_someinfo (struct processinfo *state)
{
	getprocess_meminfo(state);
	GetProessCPU(GetCurrentProcessId(), &state->cur_cpu, &state->threadnum);
	state->cur_cpu *= state->cpunum;
}

#else

static unsigned long long getprocesstotal ()
{
	char path[1024*4];
	char buf[1024*8];
	char *str, *tmp;
	int readnum;
	int fd;
	char state;
	int ppid, pgrp, session, tty, tpgid;
	unsigned long flags, min_flt, maj_flt, cmin_flt, cmaj_flt;
	unsigned long long utime, stime, cutime, cstime;
	unsigned long long total;
	struct stat statbuf;
	snprintf(path, sizeof(path) - 1, "/proc/%d/stat", getpid());
	if (stat(path, &statbuf))
		return 0;

	fd = open(path, O_RDONLY, 0);
	if (fd == -1)
		return 0;
	readnum = read(fd, buf, (int)(sizeof(buf) - 1));
	close(fd);
	if (readnum <= 0)
		return 0;

	buf[readnum] = '\0';
	str = buf;
	str = strchr(str, '(') + 1;
	tmp = strrchr(str, ')');
	str = tmp + 2;
	sscanf(str, 
	"%c "
	"%d %d %d %d %d "
	"%lu %lu %lu %lu %lu "
	"%Lu %Lu %Lu %Lu ",	/* utime stime cutime cstime*/
	&state, 
	&ppid, &pgrp, &session, &tty, &tpgid,
	&flags, &min_flt, &cmin_flt, &maj_flt, &cmaj_flt,
	&utime, &stime, &cutime, &cstime);

	total = utime + stime + cutime + cstime;

	return total;
}

static unsigned long long getcputotal ()
{
	const char *path = "/proc/stat";
	char buf[1024*8];
	int readnum;
	unsigned long long data[10];
	unsigned long long total;
	int fd;
	int i;
	char *str;
	struct stat statbuf;
	if (stat(path, &statbuf))
		return 0;

	fd = open(path, O_RDONLY, 0);
	if (fd == -1)
		return 0;
	readnum = read(fd, buf, (int)(sizeof(buf) - 1));
	close(fd);
	if (readnum <= 0)
		return 0;
	buf[readnum] = '\0';
	str = buf;
	for (;;)
	{
		if (*str == '\0')
			return 0;
		if (isdigit(*str) != 0)
			break;
		str++;
	}

	sscanf(str, "%Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu", 
		&data[0], &data[1], &data[2], &data[3], &data[4], &data[5],
		&data[6], &data[7], &data[8]);
	
	total = 0;
	for (i = 0; i < 9; ++i)
	{
		total += data[i];
	}

	return total;
}

static void getprocess_cpusomeinfo (int *cpurate, int cpunum)
{
	static unsigned long long lastallcputotal = 0;
	static unsigned long long lastprocesscputotal = 0;

	unsigned long long allcputotal = getcputotal();
	unsigned long long processcputotal = getprocesstotal();

	if (allcputotal == 0 || processcputotal == 0)
		return;

	double delay_allcpu = 0;
	double delay_processcpu = 0;
	if (lastallcputotal != 0)
	{
		delay_allcpu = (double)(allcputotal - lastallcputotal);
		delay_processcpu = (double)(processcputotal - lastprocesscputotal);

		*cpurate = (int)(cpunum * 100 * (delay_processcpu / delay_allcpu));
	}

	lastallcputotal = allcputotal;
	lastprocesscputotal = processcputotal;
}

#define STR_VmPeak "VmPeak"
#define STR_VmSize "VmSize"
#define STR_VmHWM "VmHWM"
#define STR_VmRSS "VmRSS"
#define STR_Threads "Threads"
static void getprocess_memsomeinfo (struct processinfo *state)
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

		if (strcmp(name, STR_VmPeak) == 0)
		{
			sscanf(value, "%lf kB", &state->max_vm);
		}
		else if (strcmp(name, STR_VmSize) == 0)
		{
			sscanf(value, "%lf kB", &state->cur_vm);
		}
		else if (strcmp(name, STR_VmHWM) == 0)
		{
			sscanf(value, "%lf kB", &state->max_mem);
		}
		else if (strcmp(name, STR_VmRSS) == 0)
		{
			sscanf(value, "%lf kB", &state->cur_mem);
		}
		else if (strcmp(name, STR_Threads) == 0)
		{
			sscanf(value, "%d", &state->threadnum);
		}
	}
}

static void getprocess_someinfo (struct processinfo *state)
{
	getprocess_memsomeinfo(state);
	getprocess_cpusomeinfo(&state->cur_cpu, state->cpunum);
}
#endif

/**
 * get the current process info.
 * note: memory unit: KB.
 * */
void processinfo_get (struct processinfo *state)
{
	if (!s_mgr.isinit)
	{
		processinfo_update();
	}

	state->cur_mem = s_mgr.state.cur_mem;
	state->max_mem = s_mgr.state.max_mem;
	state->cur_vm = s_mgr.state.cur_vm;
	state->max_vm = s_mgr.state.max_vm;
	state->cur_cpu = s_mgr.state.cur_cpu;
	state->max_cpu = s_mgr.state.max_cpu;
	state->cpunum = s_mgr.state.cpunum;
	state->threadnum = s_mgr.state.threadnum;
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
		s_mgr.state.cur_mem = 0;
		s_mgr.state.max_mem = 0;
		s_mgr.state.cur_vm = 0;
		s_mgr.state.max_vm = 0;
		s_mgr.state.cur_cpu = 0;
		s_mgr.state.max_cpu = 0;
		s_mgr.state.cpunum = get_cpunum();
		s_mgr.state.threadnum = 1;
		s_mgr.lastupdatetime = 0;
	}

	if (currenttime - s_mgr.lastupdatetime < 500)
		return;

	s_mgr.lastupdatetime = currenttime;
	getprocess_someinfo(&s_mgr.state);

	if (s_mgr.state.max_cpu < s_mgr.state.cur_cpu)
		s_mgr.state.max_cpu = s_mgr.state.cur_cpu;
}

