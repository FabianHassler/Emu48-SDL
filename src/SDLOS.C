/*
 *   SdlOs.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Win32 kernel functions (handles, threads, events, timers, files,
// serial ports, file mapping and INI files) on top of SDL3 and POSIX

#define _GNU_SOURCE							// FNM_CASEFOLD of glibc
#include "pch.h"
#include "Emu48.h"
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <fnmatch.h>
#include <poll.h>
#include <sched.h>
#include <termios.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define QPC_FREQ		10000000			// QueryPerformanceCounter() frequency, same as on Windows

enum HTYPE { H_FILE, H_COMM, H_EVENT, H_THREAD, H_MAPPING, H_FIND };

typedef struct
{
	enum HTYPE eType;
	union
	{
		struct								// H_FILE
		{
			INT nFd;
		} f;
		struct								// H_COMM
		{
			INT   nFd;						// same position as in H_FILE for ReadFile()
			INT   nWake[2];					// pipe to wake up WaitCommEvent()
			DWORD dwMask;					// events of SetCommMask()
			BOOL  bRxChar;					// last event was EV_RXCHAR
		} c;
		struct								// H_EVENT
		{
			SDL_Mutex     *pMutex;
			SDL_Condition *pCond;
			BOOL          bManual;
			BOOL          bSignaled;
			INT           nWaiters;			// threads in WaitForSingleObject()
			BOOL          bClosed;			// closed, freed by the last waiter
		} e;
		struct								// H_THREAD
		{
			SDL_Thread             *pThread;
			HANDLE                 hDone;	// signaled at thread end
			LPTHREAD_START_ROUTINE lpStartAddress;
			LPVOID                 lpParameter;
		} t;
		struct								// H_MAPPING
		{
			INT   nFd;
			BOOL  bWrite;
			DWORD dwSize;
		} m;
		struct								// H_FIND
		{
			DIR   *pDir;
			TCHAR szDir[MAX_PATH];
			TCHAR szPattern[MAX_PATH];
		} d;
	} u;
} HANDLE_T;

static HANDLE NewHandle(enum HTYPE eType)
{
	HANDLE_T *p = (HANDLE_T *) calloc(1,sizeof(*p));
	if (p) p->eType = eType;
	return (HANDLE) p;
}

BOOL CloseHandle(HANDLE hObject)
{
	HANDLE_T *p = (HANDLE_T *) hObject;

	if (p == NULL || hObject == INVALID_HANDLE_VALUE) return FALSE;

	switch (p->eType)
	{
	case H_FILE:
		close(p->u.f.nFd);					// also releases flock()
		break;
	case H_COMM:
		close(p->u.c.nFd);
		close(p->u.c.nWake[0]);
		close(p->u.c.nWake[1]);
		break;
	case H_EVENT:
		SDL_LockMutex(p->u.e.pMutex);
		if (p->u.e.nWaiters > 0)			// like Win32 the waiting threads keep the event
		{
			p->u.e.bClosed = TRUE;
			SDL_UnlockMutex(p->u.e.pMutex);
			return TRUE;
		}
		SDL_UnlockMutex(p->u.e.pMutex);
		SDL_DestroyCondition(p->u.e.pCond);
		SDL_DestroyMutex(p->u.e.pMutex);
		break;
	case H_THREAD:
		SDL_DetachThread(p->u.t.pThread);	// thread may still run, keep structure
		return TRUE;						// small structure intentionally not freed
	case H_MAPPING:
		close(p->u.m.nFd);
		break;
	case H_FIND:
		closedir(p->u.d.pDir);
		break;
	}
	free(p);
	return TRUE;
}

//################
//#
//#    Critical sections and events
//#
//################

VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	lpCriticalSection->pMutex = SDL_CreateMutex(); // SDL mutexes are recursive like critical sections
	return;
}

VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	SDL_DestroyMutex(lpCriticalSection->pMutex);
	lpCriticalSection->pMutex = NULL;
	return;
}

VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	SDL_LockMutex(lpCriticalSection->pMutex);
	return;
}

VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	SDL_UnlockMutex(lpCriticalSection->pMutex);
	return;
}

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,BOOL bManualReset,BOOL bInitialState,LPCTSTR lpName)
{
	HANDLE_T *p = (HANDLE_T *) NewHandle(H_EVENT);
	if (p != NULL)
	{
		p->u.e.pMutex = SDL_CreateMutex();
		p->u.e.pCond = SDL_CreateCondition();
		p->u.e.bManual = bManualReset;
		p->u.e.bSignaled = bInitialState;
	}
	return (HANDLE) p;
	UNREFERENCED_PARAMETER(lpEventAttributes);
	UNREFERENCED_PARAMETER(lpName);
}

BOOL SetEvent(HANDLE hEvent)
{
	HANDLE_T *p = (HANDLE_T *) hEvent;

	if (p == NULL || p->eType != H_EVENT) return FALSE; // invalid handle like Win32
	SDL_LockMutex(p->u.e.pMutex);
	p->u.e.bSignaled = TRUE;
	SDL_BroadcastCondition(p->u.e.pCond);
	SDL_UnlockMutex(p->u.e.pMutex);
	return TRUE;
}

BOOL ResetEvent(HANDLE hEvent)
{
	HANDLE_T *p = (HANDLE_T *) hEvent;

	if (p == NULL || p->eType != H_EVENT) return FALSE; // invalid handle like Win32
	SDL_LockMutex(p->u.e.pMutex);
	p->u.e.bSignaled = FALSE;
	SDL_UnlockMutex(p->u.e.pMutex);
	return TRUE;
}

DWORD WaitForSingleObject(HANDLE hHandle,DWORD dwMilliseconds)
{
	HANDLE_T *p = (HANDLE_T *) hHandle;
	DWORD dwResult = WAIT_OBJECT_0;
	BOOL  bFree;

	if (p == NULL) return WAIT_FAILED;		// invalid handle like Win32
	if (p->eType == H_THREAD)				// wait for thread end
		return WaitForSingleObject(p->u.t.hDone,dwMilliseconds);

	_ASSERT(p->eType == H_EVENT);
	SDL_LockMutex(p->u.e.pMutex);
	++p->u.e.nWaiters;
	if (dwMilliseconds == INFINITE)
	{
		while (!p->u.e.bSignaled)
			SDL_WaitCondition(p->u.e.pCond,p->u.e.pMutex);
	}
	else
	{
		Uint64 qwEnd = SDL_GetTicks() + dwMilliseconds;
		while (!p->u.e.bSignaled && dwResult == WAIT_OBJECT_0)
		{
			Uint64 qwNow = SDL_GetTicks();
			if (qwNow >= qwEnd || !SDL_WaitConditionTimeout(p->u.e.pCond,p->u.e.pMutex,(Sint32) (qwEnd - qwNow)))
			{
				if (!p->u.e.bSignaled) dwResult = WAIT_TIMEOUT;
			}
		}
	}
	if (dwResult == WAIT_OBJECT_0 && !p->u.e.bManual)
		p->u.e.bSignaled = FALSE;			// auto reset event
	bFree = (--p->u.e.nWaiters == 0 && p->u.e.bClosed);
	SDL_UnlockMutex(p->u.e.pMutex);
	if (bFree) CloseHandle(hHandle);		// closed while waiting
	return dwResult;
}

//################
//#
//#    Threads
//#
//################

static int SDLCALL ThreadProc(void *pData)
{
	HANDLE_T *p = (HANDLE_T *) pData;
	HANDLE hDone = p->u.t.hDone;

	p->u.t.lpStartAddress(p->u.t.lpParameter);
	SetEvent(hDone);						// thread finished
	return 0;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,SIZE_T dwStackSize,LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,DWORD dwCreationFlags,LPDWORD lpThreadId)
{
	HANDLE_T *p = (HANDLE_T *) NewHandle(H_THREAD);
	if (p != NULL)
	{
		p->u.t.lpStartAddress = lpStartAddress;
		p->u.t.lpParameter = lpParameter;
		p->u.t.hDone = CreateEvent(NULL,TRUE,FALSE,NULL);
		p->u.t.pThread = SDL_CreateThread(ThreadProc,"Emu48",p);
		if (lpThreadId && p->u.t.pThread)
		{
			// the thread ID is known when the thread is running
			while ((*lpThreadId = (DWORD) SDL_GetThreadID(p->u.t.pThread)) == 0)
				Sleep(0);
		}
	}
	return (HANDLE) p;
	UNREFERENCED_PARAMETER(lpThreadAttributes);
	UNREFERENCED_PARAMETER(dwStackSize);
	UNREFERENCED_PARAMETER(dwCreationFlags);
}

//################
//#
//#    Thread message queues
//#
//################

typedef struct
{
	DWORD dwThreadId;						// owner thread, 0 = free slot
	UINT  uHead;							// index of oldest message
	UINT  uCount;							// no. of messages in queue
	MSG   sMsg[64];
} MSGQUEUE_T;

static MSGQUEUE_T    sMsgQueue[4];			// message queues of threads
static SDL_InitState sMsgInit;
static SDL_Mutex     *pMsgLock = NULL;
static SDL_Condition *pMsgCond = NULL;

static VOID LockMsgQueues(VOID)
{
	if (SDL_ShouldInit(&sMsgInit))			// first call from any thread
	{
		pMsgLock = SDL_CreateMutex();
		pMsgCond = SDL_CreateCondition();
		SDL_SetInitialized(&sMsgInit,TRUE);
	}
	SDL_LockMutex(pMsgLock);
	return;
}

// get message queue of thread, create it if necessary, call with locked queues
static MSGQUEUE_T *GetMsgQueue(DWORD dwThreadId)
{
	MSGQUEUE_T *pFree = NULL;
	UINT i;

	for (i = 0; i < ARRAYSIZEOF(sMsgQueue); ++i)
	{
		if (sMsgQueue[i].dwThreadId == dwThreadId)
			return &sMsgQueue[i];
		if (sMsgQueue[i].dwThreadId == 0 && pFree == NULL)
			pFree = &sMsgQueue[i];
	}
	if (pFree != NULL)						// new queue
	{
		pFree->dwThreadId = dwThreadId;
		pFree->uHead = pFree->uCount = 0;
	}
	return pFree;
}

BOOL GetMessage(LPMSG lpMsg,HWND hWnd,UINT wMsgFilterMin,UINT wMsgFilterMax)
{
	MSGQUEUE_T *q;

	LockMsgQueues();
	q = GetMsgQueue((DWORD) SDL_GetCurrentThreadID());
	_ASSERT(q != NULL);
	if (q == NULL)							// no free queue, end message loop
	{
		SDL_UnlockMutex(pMsgLock);
		return FALSE;
	}
	while (q->uCount == 0)					// wait for message
		SDL_WaitCondition(pMsgCond,pMsgLock);
	*lpMsg = q->sMsg[q->uHead];
	q->uHead = (q->uHead + 1) % ARRAYSIZEOF(q->sMsg);
	--q->uCount;
	if (lpMsg->message == WM_QUIT)			// end of message loop, free queue
		q->dwThreadId = 0;
	SDL_UnlockMutex(pMsgLock);
	return lpMsg->message != WM_QUIT;
	UNREFERENCED_PARAMETER(hWnd);
	UNREFERENCED_PARAMETER(wMsgFilterMin);
	UNREFERENCED_PARAMETER(wMsgFilterMax);
}

BOOL PostThreadMessage(DWORD idThread,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	MSGQUEUE_T *q;
	BOOL bSucc;

	LockMsgQueues();
	q = GetMsgQueue(idThread);				// thread may not have called GetMessage() yet
	if ((bSucc = (q != NULL && q->uCount < ARRAYSIZEOF(q->sMsg))))
	{
		LPMSG pMsg = &q->sMsg[(q->uHead + q->uCount++) % ARRAYSIZEOF(q->sMsg)];
		pMsg->hwnd = NULL;
		pMsg->message = Msg;
		pMsg->wParam = wParam;
		pMsg->lParam = lParam;
		SDL_BroadcastCondition(pMsgCond);
	}
	SDL_UnlockMutex(pMsgLock);
	return bSucc;
}

//################
//#
//#    Time
//#
//################

BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency)
{
	lpFrequency->QuadPart = QPC_FREQ;
	return TRUE;
}

BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount)
{
	// scale SDL counter to QPC_FREQ
	lpPerformanceCount->QuadPart = (LONGLONG) (SDL_GetTicksNS() / (SDL_NS_PER_SECOND / QPC_FREQ));
	return TRUE;
}

VOID Sleep(DWORD dwMilliseconds)
{
	if (dwMilliseconds == 0)
		sched_yield();						// Sleep(0) gives up the time slice
	else
		SDL_Delay(dwMilliseconds);
	return;
}

VOID GetLocalTime(LPSYSTEMTIME lpSystemTime)
{
	struct timespec ts;
	struct tm tm;

	clock_gettime(CLOCK_REALTIME,&ts);
	localtime_r(&ts.tv_sec,&tm);
	lpSystemTime->wYear = tm.tm_year + 1900;
	lpSystemTime->wMonth = tm.tm_mon + 1;
	lpSystemTime->wDayOfWeek = tm.tm_wday;
	lpSystemTime->wDay = tm.tm_mday;
	lpSystemTime->wHour = tm.tm_hour;
	lpSystemTime->wMinute = tm.tm_min;
	lpSystemTime->wSecond = tm.tm_sec;
	lpSystemTime->wMilliseconds = (WORD) (ts.tv_nsec / 1000000);
	return;
}

BOOL GetSystemPowerStatus(LPSYSTEM_POWER_STATUS lpSystemPowerStatus)
{
	int nPercent;
	SDL_PowerState eState = SDL_GetPowerInfo(NULL,&nPercent);

	ZeroMemory(lpSystemPowerStatus,sizeof(*lpSystemPowerStatus));
	lpSystemPowerStatus->ACLineStatus = (eState == SDL_POWERSTATE_ON_BATTERY) ? AC_LINE_OFFLINE : AC_LINE_ONLINE;
	lpSystemPowerStatus->BatteryLifePercent = (nPercent < 0) ? 255 : (BYTE) nPercent;
	if (eState == SDL_POWERSTATE_NO_BATTERY)
		lpSystemPowerStatus->BatteryFlag = BATTERY_FLAG_NO_BATTERY;
	else if (nPercent >= 0 && nPercent < 5)
		lpSystemPowerStatus->BatteryFlag = BATTERY_FLAG_CRITICAL;
	else if (nPercent >= 0 && nPercent < 33)
		lpSystemPowerStatus->BatteryFlag = BATTERY_FLAG_LOW;
	else
		lpSystemPowerStatus->BatteryFlag = BATTERY_FLAG_HIGH;
	return TRUE;
}

// snprintf() with the Microsoft size prefix I64 of 64 bit integers converted to ll
INT PortSnprintf(LPTSTR lpBuffer,size_t nCount,LPCTSTR lpFormat,...)
{
	TCHAR   szFormat[512];
	LPTSTR  p = szFormat;
	BOOL    bSpec = FALSE;					// inside a conversion specification
	va_list ap;
	INT     nLen;

	for (; *lpFormat && p < szFormat + ARRAYSIZEOF(szFormat) - 2; ++lpFormat)
	{
		if (bSpec && strncmp(lpFormat,"I64",3) == 0)
		{
			*p++ = 'l';
			*p++ = 'l';
			lpFormat += 2;
			continue;
		}
		if (*lpFormat == '%') bSpec = !bSpec;
		else if (bSpec && strchr("0123456789.-+ #",*lpFormat) == NULL) bSpec = FALSE;
		*p++ = *lpFormat;
	}
	*p = 0;
	va_start(ap,lpFormat);
	nLen = vsnprintf(lpBuffer,nCount,szFormat,ap);
	va_end(ap);
	return nLen;
}

VOID OutputDebugString(LPCTSTR lpOutputString)
{
	#if defined _DEBUG
		fputs(lpOutputString,stderr);
	#endif
	return;
	UNREFERENCED_PARAMETER(lpOutputString);
}

//################
//#
//#    Multimedia timer
//#
//################

typedef struct
{
	SDL_TimerID    nId;
	LPTIMECALLBACK lpTimeProc;
	DWORD_PTR      dwUser;
	BOOL           bPeriodic;
} TIMER_T;

static TIMER_T sTimer[16];					// active timers
static SDL_Mutex *pTimerLock = NULL;

static Uint32 SDLCALL TimerProc(void *pData,SDL_TimerID nId,Uint32 uInterval)
{
	TIMER_T *p = (TIMER_T *) pData;
	LPTIMECALLBACK lpTimeProc;
	DWORD_PTR dwUser;

	SDL_LockMutex(pTimerLock);
	lpTimeProc = (p->nId == nId) ? p->lpTimeProc : NULL;
	dwUser = p->dwUser;
	if (p->nId == nId && !p->bPeriodic) p->nId = 0; // one shot timer done
	SDL_UnlockMutex(pTimerLock);

	if (lpTimeProc == NULL) return 0;		// timer killed
	lpTimeProc((UINT) nId,0,dwUser,0,0);
	return p->bPeriodic ? uInterval : 0;
}

UINT timeSetEvent(UINT uDelay,UINT uResolution,LPTIMECALLBACK lpTimeProc,DWORD_PTR dwUser,UINT fuEvent)
{
	UINT i;
	SDL_TimerID nId = 0;

	if (pTimerLock == NULL) pTimerLock = SDL_CreateMutex();

	SDL_LockMutex(pTimerLock);
	for (i = 0; i < ARRAYSIZEOF(sTimer) && sTimer[i].nId != 0; ++i) { }
	if (i < ARRAYSIZEOF(sTimer))			// free slot found
	{
		sTimer[i].lpTimeProc = lpTimeProc;
		sTimer[i].dwUser = dwUser;
		sTimer[i].bPeriodic = (fuEvent & TIME_PERIODIC) != 0;
		nId = SDL_AddTimer(uDelay ? uDelay : 1,TimerProc,&sTimer[i]);
		sTimer[i].nId = nId;
	}
	SDL_UnlockMutex(pTimerLock);
	return (UINT) nId;
	UNREFERENCED_PARAMETER(uResolution);
}

UINT timeKillEvent(UINT uTimerID)
{
	UINT i;

	SDL_LockMutex(pTimerLock);
	for (i = 0; i < ARRAYSIZEOF(sTimer); ++i)
	{
		if (sTimer[i].nId == uTimerID)
		{
			SDL_RemoveTimer(sTimer[i].nId);
			sTimer[i].nId = 0;				// free slot
		}
	}
	SDL_UnlockMutex(pTimerLock);
	return TIMERR_NOERROR;
}

UINT timeGetDevCaps(TIMECAPS *ptc,UINT cbtc)
{
	ptc->wPeriodMin = 1;
	ptc->wPeriodMax = 1000000;
	return TIMERR_NOERROR;
	UNREFERENCED_PARAMETER(cbtc);
}

//################
//#
//#    Strings
//#
//################

// simple Latin-1 <-> UTF-16 conversion, only used for file headers and symbol names
INT MultiByteToWideChar(UINT CodePage,DWORD dwFlags,LPCSTR lpMultiByteStr,INT cbMultiByte,LPWSTR lpWideCharStr,INT cchWideChar)
{
	INT i;

	if (cbMultiByte < 0) cbMultiByte = (INT) strlen(lpMultiByteStr) + 1;
	if (cchWideChar == 0) return cbMultiByte;
	for (i = 0; i < cbMultiByte && i < cchWideChar; ++i)
		lpWideCharStr[i] = (BYTE) lpMultiByteStr[i];
	return i;
	UNREFERENCED_PARAMETER(CodePage);
	UNREFERENCED_PARAMETER(dwFlags);
}

INT WideCharToMultiByte(UINT CodePage,DWORD dwFlags,LPCWSTR lpWideCharStr,INT cchWideChar,LPSTR lpMultiByteStr,INT cbMultiByte,LPCSTR lpDefaultChar,LPBOOL lpUsedDefaultChar)
{
	INT i;

	if (cchWideChar < 0) for (cchWideChar = 0; lpWideCharStr[cchWideChar++];) { }
	if (cbMultiByte == 0) return cchWideChar;
	for (i = 0; i < cchWideChar && i < cbMultiByte; ++i)
		lpMultiByteStr[i] = (lpWideCharStr[i] < 0x100) ? (CHAR) lpWideCharStr[i] : '?';
	return i;
	UNREFERENCED_PARAMETER(CodePage);
	UNREFERENCED_PARAMETER(dwFlags);
	UNREFERENCED_PARAMETER(lpDefaultChar);
	UNREFERENCED_PARAMETER(lpUsedDefaultChar);
}

//################
//#
//#    File names (Windows file names are case insensitive)
//#
//################

// replace '\' with '/'
static VOID ToSlash(LPTSTR lpszPath)
{
	for (; *lpszPath; ++lpszPath)
		if (*lpszPath == '\\') *lpszPath = '/';
	return;
}

// get the existing file name with case insensitive search of each path component
static VOID GetRealName(LPCTSTR lpszFileName,LPTSTR lpszRealName)
{
	LPTSTR p;
	struct stat st;

	lstrcpyn(lpszRealName,lpszFileName,MAX_PATH);
	ToSlash(lpszRealName);
	if (stat(lpszRealName,&st) == 0) return; // file exists

	// check each path component
	for (p = lpszRealName; p != NULL; p = strchr(p+1,'/'))
	{
		TCHAR szDir[MAX_PATH];
		LPTSTR pszName,pszNext;
		DIR *pDir;
		struct dirent *pEntry;

		if (p == lpszRealName && *p != '/')	// relative path, first component
		{
			lstrcpyn(szDir,".",ARRAYSIZEOF(szDir));
			pszName = p;
		}
		else
		{
			lstrcpyn(szDir,lpszRealName,(INT) (p - lpszRealName) + 1);
			if (szDir[0] == 0) lstrcpyn(szDir,"/",ARRAYSIZEOF(szDir));
			pszName = p + 1;
		}
		if ((pszNext = strchr(pszName,'/')) != NULL) *pszNext = 0;

		if (*pszName && (pDir = opendir(szDir)) != NULL)
		{
			while ((pEntry = readdir(pDir)) != NULL)
			{
				if (SDL_strcasecmp(pEntry->d_name,pszName) == 0)
				{
					memcpy(pszName,pEntry->d_name,strlen(pszName)); // same length
					break;
				}
			}
			closedir(pDir);
		}
		if (pszNext != NULL) *pszNext = '/';
	}
	return;
}

DWORD GetCurrentDirectory(DWORD nBufferLength,LPTSTR lpBuffer)
{
	if (getcwd(lpBuffer,nBufferLength) == NULL) return 0;
	return (DWORD) strlen(lpBuffer);
}

BOOL SetCurrentDirectory(LPCTSTR lpPathName)
{
	TCHAR szPath[MAX_PATH];
	GetRealName(lpPathName,szPath);
	return chdir(szPath) == 0;
}

DWORD GetFileAttributes(LPCTSTR lpFileName)
{
	TCHAR szPath[MAX_PATH];
	struct stat st;
	DWORD dwAttr;

	GetRealName(lpFileName,szPath);
	if (stat(szPath,&st) != 0) return INVALID_FILE_ATTRIBUTES;
	dwAttr = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
	if (access(szPath,W_OK) != 0) dwAttr |= FILE_ATTRIBUTE_READONLY;
	return dwAttr;
}

// only the read only attribute, removes or adds write permission
BOOL SetFileAttributes(LPCTSTR lpFileName,DWORD dwFileAttributes)
{
	TCHAR szPath[MAX_PATH];
	struct stat st;

	GetRealName(lpFileName,szPath);
	if (stat(szPath,&st) != 0) return FALSE;
	if (dwFileAttributes & FILE_ATTRIBUTE_READONLY)
		st.st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	else
		st.st_mode |= S_IWUSR;
	return chmod(szPath,st.st_mode & 07777) == 0;
}

DWORD GetFullPathName(LPCTSTR lpFileName,DWORD nBufferLength,LPTSTR lpBuffer,LPTSTR *lpFilePart)
{
	TCHAR szPath[MAX_PATH];
	LPTSTR p;

	if (lpFileName[0] == 0) return 0;		// no file name, error like Win32

	szPath[0] = 0;
	if (lpFileName[0] != '/' && lpFileName[0] != '\\')
	{
		GetCurrentDirectory(ARRAYSIZEOF(szPath),szPath);
		SDL_strlcat(szPath,"/",ARRAYSIZEOF(szPath));
	}
	SDL_strlcat(szPath,lpFileName,ARRAYSIZEOF(szPath));
	ToSlash(szPath);
	lstrcpyn(lpBuffer,szPath,nBufferLength);
	if (lpFilePart)
	{
		p = strrchr(lpBuffer,'/');
		*lpFilePart = p ? p + 1 : lpBuffer;
	}
	return (DWORD) strlen(lpBuffer);
}

//################
//#
//#    Serial ports
//#
//################

static HANDLE OpenComm(LPCTSTR lpszDevice)
{
	HANDLE_T *p;
	INT nFd;

	if ((nFd = open(lpszDevice,O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
		return INVALID_HANDLE_VALUE;

	if ((p = (HANDLE_T *) NewHandle(H_COMM)) == NULL || pipe(p->u.c.nWake) != 0)
	{
		free(p);
		close(nFd);
		return INVALID_HANDLE_VALUE;
	}
	p->u.c.nFd = nFd;
	return (HANDLE) p;
}

// standard baudrate equal or below the given baudrate, used if the exact one is not supported
static speed_t GetSpeed(DWORD dwBaudRate)
{
	const DWORD   dwRate[] = { 1200,  1800,  2400,  4800,  9600,  19200 };
	const speed_t nSpeed[] = { B1200, B1800, B2400, B4800, B9600, B19200 };
	UINT i;

	for (i = 0; i < ARRAYSIZEOF(dwRate) - 1 && dwRate[i+1] <= dwBaudRate; ++i) { }
	return nSpeed[i];
}

// raw mode with baudrate and stop bits of DCB, 8 data bits, no parity, no handshake
BOOL SetCommState(HANDLE hFile,const DCB *lpDCB)
{
	INT nFd = ((HANDLE_T *) hFile)->u.c.nFd;
	struct termios sTerm;

	if (tcgetattr(nFd,&sTerm) != 0) return FALSE;
	cfmakeraw(&sTerm);
	sTerm.c_cflag |= CLOCAL | CREAD;
	sTerm.c_cflag &= ~(CSTOPB | CRTSCTS);
	if (lpDCB->StopBits == TWOSTOPBITS) sTerm.c_cflag |= CSTOPB;
	sTerm.c_cc[VMIN] = 0;					// read returns immediately
	sTerm.c_cc[VTIME] = 0;
	cfsetispeed(&sTerm,GetSpeed(lpDCB->BaudRate));
	cfsetospeed(&sTerm,GetSpeed(lpDCB->BaudRate));
	if (tcsetattr(nFd,TCSANOW,&sTerm) != 0) return FALSE;
	SetCommSpeed(nFd,lpDCB->BaudRate);		// exact baudrate if supported
	return TRUE;
}

BOOL SetCommMask(HANDLE hFile,DWORD dwEvtMask)
{
	HANDLE_T *p = (HANDLE_T *) hFile;

	p->u.c.dwMask = dwEvtMask;
	write(p->u.c.nWake[1],"M",1);			// WaitCommEvent() returns
	return TRUE;
}

BOOL WaitCommEvent(HANDLE hFile,LPDWORD lpEvtMask,LPOVERLAPPED lpOverlapped)
{
	HANDLE_T *p = (HANDLE_T *) hFile;
	struct pollfd sPoll[2];
	CHAR cWake;

	sPoll[0].fd = p->u.c.nWake[0];
	sPoll[0].events = POLLIN;
	sPoll[1].fd = p->u.c.nFd;
	sPoll[1].events = (p->u.c.dwMask & EV_RXCHAR) ? POLLIN : 0;

	// received data may still be unread, limit the EV_RXCHAR rate
	if (p->u.c.bRxChar) poll(sPoll,1,1);

	*lpEvtMask = 0;
	if (poll(sPoll,2,-1) > 0)
	{
		if ((sPoll[0].revents & POLLIN) && read(p->u.c.nWake[0],&cWake,1) == 1 && cWake == 'T')
			*lpEvtMask |= EV_TXEMPTY;
		if (sPoll[1].revents & (POLLIN | POLLHUP))
			*lpEvtMask |= EV_RXCHAR;
	}
	*lpEvtMask &= p->u.c.dwMask;
	p->u.c.bRxChar = (*lpEvtMask & EV_RXCHAR) != 0;
	return TRUE;
	UNREFERENCED_PARAMETER(lpOverlapped);
}

BOOL ClearCommError(HANDLE hFile,LPDWORD lpErrors,LPVOID lpStat)
{
	*lpErrors = 0;							// errors are not detected
	return TRUE;
	UNREFERENCED_PARAMETER(hFile);
	UNREFERENCED_PARAMETER(lpStat);
}

BOOL PurgeComm(HANDLE hFile,DWORD dwFlags)
{
	if (dwFlags & PURGE_TXCLEAR) tcflush(((HANDLE_T *) hFile)->u.c.nFd,TCOFLUSH);
	return TRUE;
}

BOOL SetCommBreak(HANDLE hFile)
{
	return ioctl(((HANDLE_T *) hFile)->u.c.nFd,TIOCSBRK) == 0;
}

BOOL ClearCommBreak(HANDLE hFile)
{
	return ioctl(((HANDLE_T *) hFile)->u.c.nFd,TIOCCBRK) == 0;
}

//################
//#
//#    Files
//#
//################

HANDLE CreateFile(LPCTSTR lpFileName,DWORD dwDesiredAccess,DWORD dwShareMode,LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition,DWORD dwFlagsAndAttributes,HANDLE hTemplateFile)
{
	TCHAR szPath[MAX_PATH];
	HANDLE_T *p;
	INT nFd,nFlags,nLock;

	if (strncmp(lpFileName,"\\\\.\\",4) == 0)	// device name "\\.\<path>"
		return OpenComm(&lpFileName[4]);

	// access mode
	if ((dwDesiredAccess & (GENERIC_READ|GENERIC_WRITE)) == (GENERIC_READ|GENERIC_WRITE))
		nFlags = O_RDWR;
	else if (dwDesiredAccess & GENERIC_WRITE)
		nFlags = O_WRONLY;
	else
		nFlags = O_RDONLY;

	switch (dwCreationDisposition)
	{
	case CREATE_NEW:        nFlags |= O_CREAT | O_EXCL;  break;
	case CREATE_ALWAYS:     nFlags |= O_CREAT | O_TRUNC; break;
	case OPEN_ALWAYS:       nFlags |= O_CREAT;           break;
	case TRUNCATE_EXISTING: nFlags |= O_TRUNC;           break;
	}

	GetRealName(lpFileName,szPath);
	if ((nFd = open(szPath,nFlags,0644)) < 0)
		return INVALID_HANDLE_VALUE;

	// share mode: deny others writing with an advisory lock
	if ((dwShareMode & FILE_SHARE_WRITE) == 0)
	{
		nLock = (dwDesiredAccess & GENERIC_WRITE) ? LOCK_EX : LOCK_SH;
		if (flock(nFd,nLock | LOCK_NB) != 0)
		{
			close(nFd);
			return INVALID_HANDLE_VALUE;
		}
	}

	if ((p = (HANDLE_T *) NewHandle(H_FILE)) == NULL)
	{
		close(nFd);
		return INVALID_HANDLE_VALUE;
	}
	p->u.f.nFd = nFd;
	return (HANDLE) p;
	UNREFERENCED_PARAMETER(lpSecurityAttributes);
	UNREFERENCED_PARAMETER(dwFlagsAndAttributes);
	UNREFERENCED_PARAMETER(hTemplateFile);
}

BOOL ReadFile(HANDLE hFile,LPVOID lpBuffer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,LPOVERLAPPED lpOverlapped)
{
	ssize_t nRead = read(((HANDLE_T *) hFile)->u.f.nFd,lpBuffer,nNumberOfBytesToRead);
	if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (nRead > 0) ? (DWORD) nRead : 0;
	return nRead >= 0;
	UNREFERENCED_PARAMETER(lpOverlapped);
}

BOOL WriteFile(HANDLE hFile,LPCVOID lpBuffer,DWORD nNumberOfBytesToWrite,LPDWORD lpNumberOfBytesWritten,LPOVERLAPPED lpOverlapped)
{
	HANDLE_T *p = (HANDLE_T *) hFile;
	ssize_t nWritten = write(p->u.f.nFd,lpBuffer,nNumberOfBytesToWrite);

	if (p->eType == H_COMM)					// serial port
	{
		tcdrain(p->u.c.nFd);				// wait until data is sent
		write(p->u.c.nWake[1],"T",1);		// EV_TXEMPTY
	}
	if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (nWritten > 0) ? (DWORD) nWritten : 0;
	return nWritten >= 0;
	UNREFERENCED_PARAMETER(lpOverlapped);
}

DWORD SetFilePointer(HANDLE hFile,LONG lDistanceToMove,PLONG lpDistanceToMoveHigh,DWORD dwMoveMethod)
{
	const INT nWhence[] = { SEEK_SET, SEEK_CUR, SEEK_END };
	off_t lPos = lDistanceToMove;

	if (lpDistanceToMoveHigh) lPos = ((off_t) *lpDistanceToMoveHigh << 32) | (DWORD) lDistanceToMove;
	lPos = lseek(((HANDLE_T *) hFile)->u.f.nFd,lPos,nWhence[dwMoveMethod]);
	if (lPos < 0) return INVALID_SET_FILE_POINTER;
	if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = (LONG) (lPos >> 32);
	return (DWORD) lPos;
}

BOOL SetEndOfFile(HANDLE hFile)
{
	INT nFd = ((HANDLE_T *) hFile)->u.f.nFd;
	return ftruncate(nFd,lseek(nFd,0,SEEK_CUR)) == 0;
}

BOOL FlushFileBuffers(HANDLE hFile)
{
	return fsync(((HANDLE_T *) hFile)->u.f.nFd) == 0;
}

DWORD GetFileSize(HANDLE hFile,LPDWORD lpFileSizeHigh)
{
	struct stat st;

	if (fstat(((HANDLE_T *) hFile)->u.f.nFd,&st) != 0) return INVALID_FILE_SIZE;
	if (lpFileSizeHigh) *lpFileSizeHigh = (DWORD) ((ULONGLONG) st.st_size >> 32);
	return (DWORD) st.st_size;
}

HANDLE FindFirstFile(LPCTSTR lpFileName,LPWIN32_FIND_DATA lpFindFileData)
{
	HANDLE_T *p;
	LPTSTR pszSep;

	if ((p = (HANDLE_T *) NewHandle(H_FIND)) == NULL) return INVALID_HANDLE_VALUE;

	// split into directory and file pattern
	lstrcpyn(p->u.d.szDir,lpFileName,ARRAYSIZEOF(p->u.d.szDir));
	ToSlash(p->u.d.szDir);
	if ((pszSep = strrchr(p->u.d.szDir,'/')) != NULL)
	{
		lstrcpyn(p->u.d.szPattern,pszSep+1,ARRAYSIZEOF(p->u.d.szPattern));
		*pszSep = 0;
		if (p->u.d.szDir[0] == 0) lstrcpyn(p->u.d.szDir,"/",ARRAYSIZEOF(p->u.d.szDir));
	}
	else
	{
		lstrcpyn(p->u.d.szPattern,p->u.d.szDir,ARRAYSIZEOF(p->u.d.szPattern));
		lstrcpyn(p->u.d.szDir,".",ARRAYSIZEOF(p->u.d.szDir));
	}

	if ((p->u.d.pDir = opendir(p->u.d.szDir)) == NULL)
	{
		free(p);
		return INVALID_HANDLE_VALUE;
	}
	if (!FindNextFile((HANDLE) p,lpFindFileData))
	{
		CloseHandle((HANDLE) p);
		return INVALID_HANDLE_VALUE;
	}
	return (HANDLE) p;
}

BOOL FindNextFile(HANDLE hFindFile,LPWIN32_FIND_DATA lpFindFileData)
{
	HANDLE_T *p = (HANDLE_T *) hFindFile;
	struct dirent *pEntry;

	while ((pEntry = readdir(p->u.d.pDir)) != NULL)
	{
		// case insensitive pattern match like on Windows
		if (fnmatch(p->u.d.szPattern,pEntry->d_name,FNM_CASEFOLD) == 0)
		{
			TCHAR szPath[MAX_PATH];
			struct stat st;

			ZeroMemory(lpFindFileData,sizeof(*lpFindFileData));
			lstrcpyn(lpFindFileData->cFileName,pEntry->d_name,ARRAYSIZEOF(lpFindFileData->cFileName));
			SDL_snprintf(szPath,ARRAYSIZEOF(szPath),"%s/%s",p->u.d.szDir,pEntry->d_name);
			if (stat(szPath,&st) == 0)
			{
				lpFindFileData->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
				lpFindFileData->nFileSizeLow = (DWORD) st.st_size;
			}
			return TRUE;
		}
	}
	return FALSE;
}

//################
//#
//#    File mapping
//#
//################

typedef struct
{
	LPVOID pBase;
	SIZE_T dwSize;
} VIEW_T;

static VIEW_T sView[8];						// mapped views for UnmapViewOfFile()

HANDLE CreateFileMapping(HANDLE hFile,LPSECURITY_ATTRIBUTES lpAttributes,DWORD flProtect,DWORD dwMaximumSizeHigh,DWORD dwMaximumSizeLow,LPCTSTR lpName)
{
	HANDLE_T *p;

	if ((p = (HANDLE_T *) NewHandle(H_MAPPING)) == NULL) return NULL;
	p->u.m.nFd = dup(((HANDLE_T *) hFile)->u.f.nFd);
	p->u.m.bWrite = (flProtect == PAGE_READWRITE);
	p->u.m.dwSize = dwMaximumSizeLow ? dwMaximumSizeLow : GetFileSize(hFile,NULL);
	return (HANDLE) p;
	UNREFERENCED_PARAMETER(lpAttributes);
	UNREFERENCED_PARAMETER(dwMaximumSizeHigh);
	UNREFERENCED_PARAMETER(lpName);
}

LPVOID MapViewOfFile(HANDLE hFileMappingObject,DWORD dwDesiredAccess,DWORD dwFileOffsetHigh,DWORD dwFileOffsetLow,SIZE_T dwNumberOfBytesToMap)
{
	HANDLE_T *p = (HANDLE_T *) hFileMappingObject;
	SIZE_T dwSize = dwNumberOfBytesToMap ? dwNumberOfBytesToMap : p->u.m.dwSize;
	LPVOID pBase;
	UINT i;

	if (dwDesiredAccess & FILE_MAP_COPY)	// copy on write
		pBase = mmap(NULL,dwSize,PROT_READ|PROT_WRITE,MAP_PRIVATE,p->u.m.nFd,dwFileOffsetLow);
	else if (dwDesiredAccess & FILE_MAP_WRITE)
		pBase = mmap(NULL,dwSize,PROT_READ|PROT_WRITE,MAP_SHARED,p->u.m.nFd,dwFileOffsetLow);
	else
		pBase = mmap(NULL,dwSize,PROT_READ,MAP_SHARED,p->u.m.nFd,dwFileOffsetLow);
	if (pBase == MAP_FAILED) return NULL;

	for (i = 0; i < ARRAYSIZEOF(sView) && sView[i].pBase != NULL; ++i) { }
	_ASSERT(i < ARRAYSIZEOF(sView));
	if (i < ARRAYSIZEOF(sView))
	{
		sView[i].pBase = pBase;
		sView[i].dwSize = dwSize;
	}
	return pBase;
	UNREFERENCED_PARAMETER(dwFileOffsetHigh);
}

BOOL UnmapViewOfFile(LPCVOID lpBaseAddress)
{
	UINT i;

	for (i = 0; i < ARRAYSIZEOF(sView); ++i)
	{
		if (sView[i].pBase == lpBaseAddress)
		{
			munmap(sView[i].pBase,sView[i].dwSize);
			sView[i].pBase = NULL;
			return TRUE;
		}
	}
	return FALSE;
}

//################
//#
//#    INI files
//#
//################

// INI files without path are located in the user preference directory
static VOID GetIniPath(LPCTSTR lpFileName,LPTSTR lpszPath)
{
	if (strchr(lpFileName,'/') == NULL && strchr(lpFileName,'\\') == NULL)
	{
		LPTSTR lpszPref = SDL_GetPrefPath(NULL,"Emu48");
		SDL_snprintf(lpszPath,MAX_PATH,"%s%s",lpszPref ? lpszPref : "",lpFileName);
		SDL_free(lpszPref);
	}
	else
	{
		lstrcpyn(lpszPath,lpFileName,MAX_PATH);
	}
	return;
}

// read complete INI file into memory, NULL if not existing
static LPTSTR LoadIni(LPCTSTR lpFileName)
{
	TCHAR szPath[MAX_PATH];
	GetIniPath(lpFileName,szPath);
	return (LPTSTR) SDL_LoadFile(szPath,NULL);
}

// get next line, returns pointer behind line end
static LPTSTR NextLine(LPTSTR p,LPTSTR lpszLine,INT nSize)
{
	INT i = 0;

	while (*p && *p != '\n')
	{
		if (*p != '\r' && i < nSize - 1) lpszLine[i++] = *p;
		++p;
	}
	lpszLine[i] = 0;
	return *p ? p + 1 : p;
}

DWORD GetPrivateProfileString(LPCTSTR lpAppName,LPCTSTR lpKeyName,LPCTSTR lpDefault,LPTSTR lpReturnedString,DWORD nSize,LPCTSTR lpFileName)
{
	TCHAR szLine[1024];
	LPTSTR lpszIni,p;
	BOOL bSection = FALSE;

	lstrcpyn(lpReturnedString,lpDefault ? lpDefault : "",nSize);

	if ((lpszIni = LoadIni(lpFileName)) == NULL)
		return (DWORD) strlen(lpReturnedString);

	for (p = lpszIni; *p;)
	{
		LPTSTR pszEq;

		p = NextLine(p,szLine,ARRAYSIZEOF(szLine));
		if (szLine[0] == '[')				// section
		{
			LPTSTR pszEnd = strchr(szLine,']');
			if (pszEnd) *pszEnd = 0;
			bSection = SDL_strcasecmp(&szLine[1],lpAppName) == 0;
		}
		else if (bSection && (pszEq = strchr(szLine,'=')) != NULL)
		{
			*pszEq = 0;
			if (SDL_strcasecmp(szLine,lpKeyName) == 0)
			{
				lstrcpyn(lpReturnedString,pszEq+1,nSize);
				break;
			}
		}
	}
	SDL_free(lpszIni);
	return (DWORD) strlen(lpReturnedString);
}

UINT GetPrivateProfileInt(LPCTSTR lpAppName,LPCTSTR lpKeyName,INT nDefault,LPCTSTR lpFileName)
{
	TCHAR szValue[32];

	GetPrivateProfileString(lpAppName,lpKeyName,"",szValue,ARRAYSIZEOF(szValue),lpFileName);
	return szValue[0] ? (UINT) strtol(szValue,NULL,10) : (UINT) nDefault;
}

// lpString == NULL deletes the key
BOOL WritePrivateProfileString(LPCTSTR lpAppName,LPCTSTR lpKeyName,LPCTSTR lpString,LPCTSTR lpFileName)
{
	TCHAR szPath[MAX_PATH],szLine[1024];
	LPTSTR lpszIni,p;
	BOOL bSection = FALSE,bDone = FALSE;
	FILE *pFile;

	lpszIni = LoadIni(lpFileName);
	GetIniPath(lpFileName,szPath);
	if ((pFile = fopen(szPath,"w")) == NULL)
	{
		SDL_free(lpszIni);
		return FALSE;
	}

	for (p = lpszIni; p && *p;)
	{
		LPTSTR pszEq;

		p = NextLine(p,szLine,ARRAYSIZEOF(szLine));
		if (szLine[0] == '[')				// new section
		{
			if (bSection && !bDone && lpString)	// key not found in section, add it at the end
			{
				fprintf(pFile,"%s=%s\n",lpKeyName,lpString);
				bDone = TRUE;
			}
			bSection = SDL_strncasecmp(&szLine[1],lpAppName,strlen(lpAppName)) == 0
					&& szLine[strlen(lpAppName)+1] == ']';
		}
		else if (bSection && (pszEq = strchr(szLine,'=')) != NULL
				 && (INT) (pszEq - szLine) == (INT) strlen(lpKeyName)
				 && SDL_strncasecmp(szLine,lpKeyName,strlen(lpKeyName)) == 0)
		{
			if (lpString) fprintf(pFile,"%s=%s\n",lpKeyName,lpString); // replace value
			bDone = TRUE;
			continue;
		}
		if (szLine[0]) fprintf(pFile,"%s\n",szLine);
	}

	if (!bDone && lpString)					// key not written
	{
		if (!bSection) fprintf(pFile,"[%s]\n",lpAppName); // add section
		fprintf(pFile,"%s=%s\n",lpKeyName,lpString);
	}
	fclose(pFile);
	SDL_free(lpszIni);
	return TRUE;
}

//################
//#
//#    Registry
//#
//################

// serial devices of macOS (tty.*) and Linux (ttyS*, ttyUSB*, ttyACM*)
static BOOL IsSerialDevice(LPCTSTR szName)
{
	return strncmp(szName,"tty.",4) == 0 || strncmp(szName,"ttyS",4) == 0
		|| strncmp(szName,"ttyUSB",6) == 0 || strncmp(szName,"ttyACM",6) == 0;
}

// the serial ports as only key
LONG RegOpenKeyEx(HKEY hKey,LPCTSTR lpSubKey,DWORD ulOptions,DWORD samDesired,PHKEY phkResult)
{
	if (hKey != HKEY_LOCAL_MACHINE || lstrcmpi(lpSubKey,_T("Hardware\\DeviceMap\\SerialComm")) != 0)
		return ERROR_FILE_NOT_FOUND;
	*phkResult = (HKEY) opendir("/dev");
	return *phkResult ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
	UNREFERENCED_PARAMETER(ulOptions);
	UNREFERENCED_PARAMETER(samDesired);
}

// next serial port, value name and data is the device path
LONG RegEnumValue(HKEY hKey,DWORD dwIndex,LPTSTR lpValueName,LPDWORD lpcchValueName,LPDWORD lpReserved,LPDWORD lpType,LPBYTE lpData,LPDWORD lpcbData)
{
	struct dirent *pEntry;

	while ((pEntry = readdir((DIR *) hKey)) != NULL)
	{
		if (IsSerialDevice(pEntry->d_name))
		{
			SDL_snprintf(lpValueName,*lpcchValueName,"/dev/%s",pEntry->d_name);
			lstrcpyn((LPTSTR) lpData,lpValueName,*lpcbData);
			*lpcchValueName = lstrlen(lpValueName);
			*lpcbData = lstrlen((LPTSTR) lpData) + 1;
			*lpType = REG_SZ;
			return ERROR_SUCCESS;
		}
	}
	return ERROR_NO_MORE_ITEMS;
	UNREFERENCED_PARAMETER(dwIndex);
	UNREFERENCED_PARAMETER(lpReserved);
}

LONG RegCloseKey(HKEY hKey)
{
	closedir((DIR *) hKey);
	return ERROR_SUCCESS;
}
