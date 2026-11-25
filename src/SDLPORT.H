/*
 *   SdlPort.h
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Win32 API subset on top of SDL3 and POSIX, used instead of
// <windows.h> on non Windows platforms

#if !defined _SDLPORT_H
#define _SDLPORT_H

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

//################
//#
//#    Compiler
//#
//################

#define __inline			inline
#define __unaligned
#define CALLBACK
#define WINAPI
#define UNREFERENCED_PARAMETER(p)	((VOID)(p))

#if !defined __max
  #define __max(a,b)		(((a) > (b)) ? (a) : (b))
#endif
#if !defined __min
  #define __min(a,b)		(((a) < (b)) ? (a) : (b))
#endif

#if defined _DEBUG
  #define _ASSERT(a)		assert(a)
#else
  #define _ASSERT(a)		((void)0)
#endif

#if !defined VERIFY
#if defined _DEBUG
#define VERIFY(f)			_ASSERT(f)
#else // _DEBUG
#define VERIFY(f)			((VOID)(f))
#endif // _DEBUG
#endif // VERIFY

//################
//#
//#    Types (fixed size, identical to Win32/Win64)
//#
//################

#define VOID				void
#define CONST				const
typedef char				CHAR;
typedef int16_t				SHORT;
typedef int32_t				LONG;
typedef int32_t				INT;
typedef uint32_t			UINT;
typedef uint8_t				BYTE;
typedef uint16_t			WORD;
typedef uint32_t			DWORD;
typedef uint32_t			ULONG;
typedef int64_t				LONGLONG;
typedef uint64_t			ULONGLONG;
typedef int32_t				BOOL;
typedef float				FLOAT;
typedef intptr_t			INT_PTR, LONG_PTR;
typedef uintptr_t			UINT_PTR, ULONG_PTR, DWORD_PTR, SIZE_T;
typedef INT_PTR				LRESULT;
typedef UINT_PTR			WPARAM;
typedef LONG_PTR			LPARAM;

typedef char				TCHAR;
typedef unsigned char		_TUCHAR;
typedef unsigned short		WCHAR;
typedef TCHAR				*LPTSTR, *PTSTR, *LPSTR;
typedef const TCHAR			*LPCTSTR, *PCTSTR, *LPCSTR;
typedef WCHAR				*LPWSTR;
typedef const WCHAR			*LPCWSTR;
typedef BYTE				*LPBYTE;
typedef WORD				*LPWORD;
typedef DWORD				*LPDWORD;
typedef ULONG				*PULONG;
typedef LONG				*LPLONG, *PLONG;
typedef INT					*LPINT;
typedef BOOL				*LPBOOL;
typedef void				*LPVOID;
typedef const void			*LPCVOID;
typedef DWORD				COLORREF;

typedef union _LARGE_INTEGER
{
	struct { DWORD LowPart; LONG HighPart; } u;
	struct { DWORD LowPart; LONG HighPart; };
	LONGLONG QuadPart;
} LARGE_INTEGER;

#define TRUE				1
#define FALSE				0
#if !defined NULL
#define NULL				((void*)0)
#endif

#define MAX_PATH			1024

#define LOBYTE(w)			((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define LOWORD(l)			((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)			((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKELONG(a,b)		((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define RGB(r,g,b)			((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb)		(LOBYTE(rgb))
#define GetGValue(rgb)		(LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb)		(LOBYTE((rgb)>>16))
#define MAKEINTRESOURCE(i)	((LPTSTR)((ULONG_PTR)((WORD)(i))))

//################
//#
//#    Strings (TCHAR is char, UTF-8)
//#
//################

#define _T(x)				x
#define _tcscat				strcat
#define _tcsncmp			strncmp
#define _tcschr				strchr
#define _tcstoul			strtoul
#define _ttoi				atoi
#define _istdigit			isdigit
#define _istxdigit			isxdigit
#define _totupper			toupper
#define _sntprintf			PortSnprintf	// with the size prefix I64
#define _stscanf			sscanf
extern int PortSnprintf(char *lpBuffer,size_t nCount,const char *lpFormat,...);
#define wsprintf(b,...)		PortSnprintf(b,1024,__VA_ARGS__)
#define wvsprintf			vsprintf
#define lstrlen				(int)strlen
#define lstrcpy				strcpy
#define lstrcpyn(d,s,n)		SDL_strlcpy(d,s,n)
#define lstrcat				strcat
#define lstrcmp				strcmp
#define lstrcmpi			SDL_strcasecmp
#define _MAX_PATH			MAX_PATH

#define CP_ACP				0
#define MB_PRECOMPOSED		1
extern INT MultiByteToWideChar(UINT CodePage,DWORD dwFlags,LPCSTR lpMultiByteStr,INT cbMultiByte,LPWSTR lpWideCharStr,INT cchWideChar);
extern INT WideCharToMultiByte(UINT CodePage,DWORD dwFlags,LPCWSTR lpWideCharStr,INT cchWideChar,LPSTR lpMultiByteStr,INT cbMultiByte,LPCSTR lpDefaultChar,LPBOOL lpUsedDefaultChar);

//################
//#
//#    Memory
//#
//################

#define ZeroMemory(d,l)		memset((d),0,(l))
#define FillMemory(d,l,f)	memset((d),(f),(l))
#define CopyMemory(d,s,l)	memcpy((d),(s),(l))
#define MoveMemory(d,s,l)	memmove((d),(s),(l))
#define _alloca				alloca
#define MulDiv(a,b,c)		((INT)(((LONGLONG)(a)*(LONGLONG)(b))/(LONGLONG)(c)))

#define GMEM_MOVEABLE		0x0002
#define GMEM_ZEROINIT		0x0040
typedef void				*HGLOBAL;
static __inline HGLOBAL GlobalAlloc(UINT uFlags,SIZE_T dwBytes)
{
	return (uFlags & GMEM_ZEROINIT) ? calloc(1,dwBytes) : malloc(dwBytes);
}
static __inline HGLOBAL GlobalFree(HGLOBAL hMem) { free(hMem); return NULL; }
static __inline LPVOID  GlobalLock(HGLOBAL hMem) { return hMem; }
static __inline BOOL    GlobalUnlock(HGLOBAL hMem) { return TRUE; }

//################
//#
//#    Handles and kernel objects
//#
//################

typedef void				*HANDLE;
typedef void				*HINSTANCE, *HMODULE, *HKEY, *HICON, *HCURSOR, *HMENU, *HFONT, *HBRUSH;
typedef void				*HDROP, *HACCEL, *HHOOK, *HDDEDATA, *HCONV, *HSZ, *HMONITOR, *HKL, *HWAVEOUT;
typedef struct _SDLWND		*HWND;
typedef struct _SDLGDIOBJ	*HGDIOBJ;
typedef struct _SDLGDIOBJ	*HBITMAP;
typedef struct _SDLGDIOBJ	*HPALETTE;
typedef struct _SDLGDIOBJ	*HRGN;
typedef struct _SDLDC		*HDC;
typedef INT_PTR				(CALLBACK *DLGPROC)(HWND,UINT,WPARAM,LPARAM);
typedef LRESULT				(CALLBACK *WNDPROC)(HWND,UINT,WPARAM,LPARAM);

#define INVALID_HANDLE_VALUE		((HANDLE)(LONG_PTR)-1)
#define INVALID_SET_FILE_POINTER	((DWORD)-1)
#define INVALID_FILE_ATTRIBUTES		((DWORD)-1)
#define INVALID_FILE_SIZE			((DWORD)0xFFFFFFFF)
#define INFINITE					0xFFFFFFFF
#define WAIT_OBJECT_0				0
#define WAIT_TIMEOUT				258
#define WAIT_FAILED					((DWORD)0xFFFFFFFF)
#define ERROR_SUCCESS				0

extern BOOL  CloseHandle(HANDLE hObject);

// critical sections
typedef struct { SDL_Mutex *pMutex; } CRITICAL_SECTION, *LPCRITICAL_SECTION;
extern VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
extern VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
extern VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
extern VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

// events
typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
extern HANDLE CreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,BOOL bManualReset,BOOL bInitialState,LPCTSTR lpName);
extern BOOL   SetEvent(HANDLE hEvent);
extern BOOL   ResetEvent(HANDLE hEvent);
extern DWORD  WaitForSingleObject(HANDLE hHandle,DWORD dwMilliseconds);

// threads, CREATE_SUSPENDED is ignored because Emu48 resumes
// suspended threads directly after creation
#define CREATE_SUSPENDED				0x00000004
#define THREAD_PRIORITY_ABOVE_NORMAL	1
typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
extern HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,SIZE_T dwStackSize,LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,DWORD dwCreationFlags,LPDWORD lpThreadId);
static __inline DWORD ResumeThread(HANDLE hThread) { return 1; }
static __inline BOOL  SetThreadPriority(HANDLE hThread,INT nPriority) { return TRUE; }

// thread message queues
#define WM_QUIT						0x0012
typedef struct tagMSG
{
	HWND   hwnd;
	UINT   message;
	WPARAM wParam;
	LPARAM lParam;
} MSG, *LPMSG;
extern BOOL GetMessage(LPMSG lpMsg,HWND hWnd,UINT wMsgFilterMin,UINT wMsgFilterMax);
extern BOOL PostThreadMessage(DWORD idThread,UINT Msg,WPARAM wParam,LPARAM lParam);

// time
extern BOOL  QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
extern BOOL  QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
static __inline DWORD GetTickCount(VOID) { return (DWORD) SDL_GetTicks(); }
extern VOID  Sleep(DWORD dwMilliseconds);

typedef struct _SYSTEMTIME
{
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;
extern VOID GetLocalTime(LPSYSTEMTIME lpSystemTime);

// multimedia timer
#define TIME_ONESHOT				0x0000
#define TIME_PERIODIC				0x0001
#define TIMERR_NOERROR				0
typedef VOID (CALLBACK *LPTIMECALLBACK)(UINT uTimerID,UINT uMsg,DWORD_PTR dwUser,DWORD_PTR dw1,DWORD_PTR dw2);
typedef struct { UINT wPeriodMin; UINT wPeriodMax; } TIMECAPS;
extern UINT timeSetEvent(UINT uDelay,UINT uResolution,LPTIMECALLBACK lpTimeProc,DWORD_PTR dwUser,UINT fuEvent);
extern UINT timeKillEvent(UINT uTimerID);
static __inline UINT timeBeginPeriod(UINT uPeriod) { return TIMERR_NOERROR; }
static __inline UINT timeEndPeriod(UINT uPeriod) { return TIMERR_NOERROR; }
extern UINT timeGetDevCaps(TIMECAPS *ptc,UINT cbtc);
static __inline DWORD timeGetTime(VOID) { return (DWORD) SDL_GetTicks(); }

// waveform audio output (implemented in SdlWave.c)
#define WAVE_MAPPER					((UINT)-1)
#define WAVE_FORMAT_PCM				1
#define WAVE_FORMAT_4M08			0x00000100
#define CALLBACK_THREAD				0x00020000
#define MM_WOM_DONE					0x3BD
#define MMSYSERR_NOERROR			0
#define MMSYSERR_ERROR				1
#define MMSYSERR_BADDEVICEID		2
#define MMSYSERR_NOMEM				7
#define MAXPNAMELEN					32
typedef UINT MMRESULT;
typedef HWAVEOUT *LPHWAVEOUT;
typedef struct tWAVEFORMATEX
{
	WORD  wFormatTag;
	WORD  nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD  nBlockAlign;
	WORD  wBitsPerSample;
	WORD  cbSize;
} WAVEFORMATEX;
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;
typedef struct wavehdr_tag
{
	LPSTR     lpData;
	DWORD     dwBufferLength;
	DWORD     dwBytesRecorded;
	DWORD_PTR dwUser;
	DWORD     dwFlags;
	DWORD     dwLoops;
	struct wavehdr_tag *lpNext;
	DWORD_PTR reserved;
} *PWAVEHDR;
typedef struct tagWAVEOUTCAPS
{
	WORD  wMid;
	WORD  wPid;
	UINT  vDriverVersion;
	TCHAR szPname[MAXPNAMELEN];
	DWORD dwFormats;
	WORD  wChannels;
	WORD  wReserved1;
	DWORD dwSupport;
} WAVEOUTCAPS, *LPWAVEOUTCAPS;
extern UINT     waveOutGetNumDevs(VOID);
extern MMRESULT waveOutGetDevCaps(UINT_PTR uDeviceID,LPWAVEOUTCAPS pwoc,UINT cbwoc);
extern MMRESULT waveOutGetID(HWAVEOUT hwo,UINT *puDeviceID);
extern MMRESULT waveOutOpen(LPHWAVEOUT phwo,UINT uDeviceID,LPCWAVEFORMATEX pwfx,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen);
extern MMRESULT waveOutClose(HWAVEOUT hwo);
// wave headers need no preparation
static __inline MMRESULT waveOutPrepareHeader(HWAVEOUT hwo,PWAVEHDR pwh,UINT cbwh) { return MMSYSERR_NOERROR; }
static __inline MMRESULT waveOutUnprepareHeader(HWAVEOUT hwo,PWAVEHDR pwh,UINT cbwh) { return MMSYSERR_NOERROR; }
extern MMRESULT waveOutWrite(HWAVEOUT hwo,PWAVEHDR pwh,UINT cbwh);
extern MMRESULT waveOutReset(HWAVEOUT hwo);

// debug output
extern VOID OutputDebugString(LPCTSTR lpOutputString);

// power status
#define AC_LINE_OFFLINE				0x00
#define AC_LINE_ONLINE				0x01
#define BATTERY_FLAG_HIGH			0x01
#define BATTERY_FLAG_LOW			0x02
#define BATTERY_FLAG_CRITICAL		0x04
#define BATTERY_FLAG_NO_BATTERY		0x80
typedef struct _SYSTEM_POWER_STATUS
{
	BYTE  ACLineStatus;
	BYTE  BatteryFlag;
	BYTE  BatteryLifePercent;
	BYTE  SystemStatusFlag;
	DWORD BatteryLifeTime;
	DWORD BatteryFullLifeTime;
} SYSTEM_POWER_STATUS, *LPSYSTEM_POWER_STATUS;
extern BOOL GetSystemPowerStatus(LPSYSTEM_POWER_STATUS lpSystemPowerStatus);

//################
//#
//#    Files
//#
//################

#define GENERIC_READ				0x80000000
#define GENERIC_WRITE				0x40000000
#define FILE_SHARE_READ				0x00000001
#define FILE_SHARE_WRITE			0x00000002
#define CREATE_NEW					1
#define CREATE_ALWAYS				2
#define OPEN_EXISTING				3
#define OPEN_ALWAYS					4
#define TRUNCATE_EXISTING			5
#define FILE_ATTRIBUTE_READONLY		0x00000001
#define FILE_ATTRIBUTE_DIRECTORY	0x00000010
#define FILE_ATTRIBUTE_NORMAL		0x00000080
#define FILE_FLAG_OVERLAPPED		0x40000000
#define FILE_FLAG_SEQUENTIAL_SCAN	0x08000000
#define FILE_BEGIN					0
#define FILE_CURRENT				1
#define FILE_END					2

typedef struct _OVERLAPPED
{
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	DWORD     Offset;
	DWORD     OffsetHigh;
	HANDLE    hEvent;
} OVERLAPPED, *LPOVERLAPPED;

// file I/O is synchronous, overlapped I/O is never pending
#define ERROR_IO_PENDING			997
static __inline DWORD GetLastError(VOID) { return 0; }
static __inline BOOL GetOverlappedResult(HANDLE hFile,LPOVERLAPPED lpOverlapped,LPDWORD lpNumberOfBytesTransferred,BOOL bWait) { return TRUE; }

typedef struct _FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME;

typedef struct _WIN32_FIND_DATA
{
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
	TCHAR    cFileName[MAX_PATH];
	TCHAR    cAlternateFileName[14];
} WIN32_FIND_DATA, *LPWIN32_FIND_DATA;

extern HANDLE CreateFile(LPCTSTR lpFileName,DWORD dwDesiredAccess,DWORD dwShareMode,LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition,DWORD dwFlagsAndAttributes,HANDLE hTemplateFile);
extern BOOL   ReadFile(HANDLE hFile,LPVOID lpBuffer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,LPOVERLAPPED lpOverlapped);
extern BOOL   WriteFile(HANDLE hFile,LPCVOID lpBuffer,DWORD nNumberOfBytesToWrite,LPDWORD lpNumberOfBytesWritten,LPOVERLAPPED lpOverlapped);
extern DWORD  SetFilePointer(HANDLE hFile,LONG lDistanceToMove,PLONG lpDistanceToMoveHigh,DWORD dwMoveMethod);
extern BOOL   SetEndOfFile(HANDLE hFile);
extern BOOL   FlushFileBuffers(HANDLE hFile);
extern DWORD  GetFileAttributes(LPCTSTR lpFileName);
extern BOOL   SetFileAttributes(LPCTSTR lpFileName,DWORD dwFileAttributes);
extern DWORD  GetFileSize(HANDLE hFile,LPDWORD lpFileSizeHigh);
extern HANDLE FindFirstFile(LPCTSTR lpFileName,LPWIN32_FIND_DATA lpFindFileData);
extern BOOL   FindNextFile(HANDLE hFindFile,LPWIN32_FIND_DATA lpFindFileData);
static __inline BOOL FindClose(HANDLE hFindFile) { return CloseHandle(hFindFile); }
extern DWORD  GetCurrentDirectory(DWORD nBufferLength,LPTSTR lpBuffer);
extern BOOL   SetCurrentDirectory(LPCTSTR lpPathName);
extern DWORD  GetFullPathName(LPCTSTR lpFileName,DWORD nBufferLength,LPTSTR lpBuffer,LPTSTR *lpFilePart);

// serial ports (implemented with termios, device name "\\.\<path>")
#define MAXDWORD					0xFFFFFFFF
#define EV_RXCHAR					0x0001
#define EV_TXEMPTY					0x0004
#define EV_ERR						0x0080
#define CE_OVERRUN					0x0002
#define CE_FRAME					0x0008
#define CE_BREAK					0x0010
#define PURGE_TXABORT				0x0001
#define PURGE_TXCLEAR				0x0004
#define DTR_CONTROL_DISABLE			0
#define RTS_CONTROL_DISABLE			0
#define NOPARITY					0
#define TWOSTOPBITS					2
typedef struct _DCB
{
	DWORD DCBlength;
	DWORD BaudRate;
	DWORD fBinary;
	DWORD fParity;
	DWORD fOutxCtsFlow;
	DWORD fOutxDsrFlow;
	DWORD fDtrControl;
	DWORD fDsrSensitivity;
	DWORD fOutX;
	DWORD fErrorChar;
	DWORD fNull;
	DWORD fRtsControl;
	DWORD fAbortOnError;
	BYTE  ByteSize;
	BYTE  Parity;
	BYTE  StopBits;
} DCB;
typedef struct _COMMTIMEOUTS
{
	DWORD ReadIntervalTimeout;
	DWORD ReadTotalTimeoutMultiplier;
	DWORD ReadTotalTimeoutConstant;
	DWORD WriteTotalTimeoutMultiplier;
	DWORD WriteTotalTimeoutConstant;
} COMMTIMEOUTS;
extern BOOL SetCommState(HANDLE hFile,const DCB *lpDCB);
static __inline BOOL SetCommTimeouts(HANDLE hFile,const COMMTIMEOUTS *lpCommTimeouts) { return TRUE; } // reading never blocks
extern BOOL SetCommMask(HANDLE hFile,DWORD dwEvtMask);
extern BOOL WaitCommEvent(HANDLE hFile,LPDWORD lpEvtMask,LPOVERLAPPED lpOverlapped);
extern BOOL ClearCommError(HANDLE hFile,LPDWORD lpErrors,LPVOID lpStat);
extern BOOL PurgeComm(HANDLE hFile,DWORD dwFlags);
extern BOOL SetCommBreak(HANDLE hFile);
extern BOOL ClearCommBreak(HANDLE hFile);
extern BOOL SetCommSpeed(INT nFd,DWORD dwBaudRate);	// exact baudrate (implemented in SdlBaud.c)

// file mapping (implemented with a complete read into memory)
#define PAGE_READONLY				0x02
#define PAGE_READWRITE				0x04
#define FILE_MAP_COPY				0x0001
#define FILE_MAP_WRITE				0x0002
#define FILE_MAP_READ				0x0004
extern HANDLE CreateFileMapping(HANDLE hFile,LPSECURITY_ATTRIBUTES lpAttributes,DWORD flProtect,DWORD dwMaximumSizeHigh,DWORD dwMaximumSizeLow,LPCTSTR lpName);
extern LPVOID MapViewOfFile(HANDLE hFileMappingObject,DWORD dwDesiredAccess,DWORD dwFileOffsetHigh,DWORD dwFileOffsetLow,SIZE_T dwNumberOfBytesToMap);
extern BOOL   UnmapViewOfFile(LPCVOID lpBaseAddress);

// INI files
extern DWORD GetPrivateProfileString(LPCTSTR lpAppName,LPCTSTR lpKeyName,LPCTSTR lpDefault,LPTSTR lpReturnedString,DWORD nSize,LPCTSTR lpFileName);
extern UINT  GetPrivateProfileInt(LPCTSTR lpAppName,LPCTSTR lpKeyName,INT nDefault,LPCTSTR lpFileName);
extern BOOL  WritePrivateProfileString(LPCTSTR lpAppName,LPCTSTR lpKeyName,LPCTSTR lpString,LPCTSTR lpFileName);

// registry, only the serial ports in HKEY_LOCAL_MACHINE\Hardware\DeviceMap\SerialComm
#define HKEY_LOCAL_MACHINE			((HKEY) (ULONG_PTR) 0x80000002)
#define KEY_QUERY_VALUE				0x0001
#define KEY_ENUMERATE_SUB_KEYS		0x0008
#define REG_SZ						1
#define ERROR_FILE_NOT_FOUND		2
#define ERROR_NO_MORE_ITEMS			259
typedef HKEY						*PHKEY;
extern LONG RegOpenKeyEx(HKEY hKey,LPCTSTR lpSubKey,DWORD ulOptions,DWORD samDesired,PHKEY phkResult);
extern LONG RegEnumValue(HKEY hKey,DWORD dwIndex,LPTSTR lpValueName,LPDWORD lpcchValueName,LPDWORD lpReserved,LPDWORD lpType,LPBYTE lpData,LPDWORD lpcbData);
extern LONG RegCloseKey(HKEY hKey);

// COM declarations for SndEnum.c, DirectSound isn't available
typedef LONG						HRESULT;
#define FAILED(hr)					((hr) < 0)
typedef struct { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID, *LPGUID;
typedef const GUID					*REFGUID, *REFCLSID, *REFIID;
#define DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const GUID n = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#define DECLARE_INTERFACE_(i,b)		typedef struct i##Vtbl i##Vtbl; struct i { const i##Vtbl *lpVtbl; }; struct i##Vtbl
#define STDMETHOD(m)				HRESULT (*m)
#define STDMETHOD_(t,m)				t (*m)
#define THIS_						struct INTERFACE *This,
#define THIS						struct INTERFACE *This
#define PURE
typedef struct IClassFactory		*LPCLASSFACTORY;
struct IClassFactory
{
	const struct
	{
		HRESULT (*CreateInstance)(LPCLASSFACTORY This,LPVOID pUnkOuter,REFIID riid,LPVOID *ppvObject);
		ULONG   (*Release)(LPCLASSFACTORY This);
	} *lpVtbl;
};
DEFINE_GUID(IID_IClassFactory,0,0,0,0,0,0,0,0,0,0,0);
typedef INT_PTR						(*FARPROC)(VOID);
static __inline HMODULE LoadLibrary(LPCTSTR lpLibFileName) { return NULL; }
static __inline FARPROC GetProcAddress(HMODULE hModule,LPCSTR lpProcName) { return NULL; }
static __inline BOOL    FreeLibrary(HMODULE hLibModule) { return TRUE; }

//################
//#
//#    GDI subset (memory bitmaps, implemented in SdlGdi.c)
//#
//################

typedef struct tagPOINT { LONG x; LONG y; } POINT, *LPPOINT;
typedef struct tagSIZE  { LONG cx; LONG cy; } SIZE, *LPSIZE;
typedef struct tagRECT  { LONG left; LONG top; LONG right; LONG bottom; } RECT, *LPRECT;

typedef struct tagRGBQUAD
{
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
} RGBQUAD;

#pragma pack(push,2)
typedef struct tagBITMAPFILEHEADER
{
	WORD  bfType;
	DWORD bfSize;
	WORD  bfReserved1;
	WORD  bfReserved2;
	DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER
{
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;

typedef struct tagBITMAPINFO
{
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD          bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;

typedef struct tagBITMAP
{
	LONG   bmType;
	LONG   bmWidth;
	LONG   bmHeight;
	LONG   bmWidthBytes;
	WORD   bmPlanes;
	WORD   bmBitsPixel;
	LPVOID bmBits;
} BITMAP;

typedef struct tagPALETTEENTRY
{
	BYTE peRed;
	BYTE peGreen;
	BYTE peBlue;
	BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;

typedef struct tagLOGPALETTE
{
	WORD         palVersion;
	WORD         palNumEntries;
	PALETTEENTRY palPalEntry[1];
} LOGPALETTE;

typedef struct _RGNDATAHEADER
{
	DWORD dwSize;
	DWORD iType;
	DWORD nCount;
	DWORD nRgnSize;
	RECT  rcBound;
} RGNDATAHEADER;

typedef struct _RGNDATA
{
	RGNDATAHEADER rdh;
	char          Buffer[1];
} RGNDATA, *LPRGNDATA;

typedef struct tagXFORM { FLOAT eM11, eM12, eM21, eM22, eDx, eDy; } XFORM;

#define BI_RGB				0
#define BI_RLE8				1
#define BI_RLE4				2
#define BI_BITFIELDS		3
#define DIB_RGB_COLORS		0
#define CBM_INIT			4
#define RDH_RECTANGLES		1

#define SRCCOPY				0x00CC0020
#define DSTINVERT			0x00550009
#define BLACKNESS			0x00000042
#define WHITENESS			0x00FF0062

#define COLORONCOLOR		3
#define HALFTONE			4
#define NULL_BRUSH			5
#define BLACK_PEN			7
#define WHITE_PEN			6
#define SYSTEM_FONT			13
#define DEFAULT_GUI_FONT	17
#define OBJ_BITMAP			7

extern HDC      CreateCompatibleDC(HDC hdc);
extern BOOL     DeleteDC(HDC hdc);
extern HGDIOBJ  SelectObject(HDC hdc,HGDIOBJ h);
extern BOOL     DeleteObject(HGDIOBJ ho);
extern HGDIOBJ  GetStockObject(INT i);
extern HGDIOBJ  GetCurrentObject(HDC hdc,UINT type);
extern INT      GetObject(HGDIOBJ h,INT c,LPVOID pv);
extern HBITMAP  CreateDIBSection(HDC hdc,const BITMAPINFO *pbmi,UINT usage,VOID **ppvBits,HANDLE hSection,DWORD offset);
extern HBITMAP  CreateDIBitmap(HDC hdc,const BITMAPINFOHEADER *pbmih,DWORD flInit,const VOID *pjBits,const BITMAPINFO *pbmi,UINT iUsage);
extern HBITMAP  CreateCompatibleBitmap(HDC hdc,INT cx,INT cy);
extern INT      GetDIBits(HDC hdc,HBITMAP hbm,UINT start,UINT cLines,LPVOID lpvBits,LPBITMAPINFO lpbmi,UINT usage);
extern UINT     SetDIBColorTable(HDC hdc,UINT iStart,UINT cEntries,const RGBQUAD *prgbq);
extern BOOL     BitBlt(HDC hdc,INT x,INT y,INT cx,INT cy,HDC hdcSrc,INT x1,INT y1,DWORD rop);
extern BOOL     StretchBlt(HDC hdcDest,INT xDest,INT yDest,INT wDest,INT hDest,HDC hdcSrc,INT xSrc,INT ySrc,INT wSrc,INT hSrc,DWORD rop);
extern BOOL     PatBlt(HDC hdc,INT x,INT y,INT w,INT h,DWORD rop);
static __inline INT SetStretchBltMode(HDC hdc,INT mode) { return COLORONCOLOR; } // only nearest neighbor
extern BOOL     SetWindowOrgEx(HDC hdc,INT x,INT y,LPPOINT lppt);
static __inline BOOL GdiFlush(VOID) { return TRUE; }
extern HPALETTE CreatePalette(const LOGPALETTE *plpal);
// palettes are not needed on true color displays
static __inline HPALETTE SelectPalette(HDC hdc,HPALETTE hPal,BOOL bForceBkgd) { return hPal; }
static __inline UINT RealizePalette(HDC hdc) { return 0; }
extern UINT     GetPaletteEntries(HPALETTE hpal,UINT iStart,UINT cEntries,LPPALETTEENTRY pPalEntries);
extern BOOL     MoveToEx(HDC hdc,INT x,INT y,LPPOINT lppt);
extern BOOL     LineTo(HDC hdc,INT x,INT y);
extern HRGN     ExtCreateRegion(const XFORM *lpx,DWORD nCount,const RGNDATA *lpData);
extern INT      OffsetRgn(HRGN hrgn,INT x,INT y);

// text output into owner drawn list box items (implemented in SdlDlg.c)
#define FW_NORMAL			400
#define FW_BOLD				700
#define ANSI_CHARSET		0
#define OUT_DEFAULT_PRECIS	0
#define CLIP_DEFAULT_PRECIS	0
#define DEFAULT_QUALITY		0
#define FF_DONTCARE			0
#define LOGPIXELSY			90
#define ETO_OPAQUE			0x0002
#define COLOR_HIGHLIGHT		13
#define COLOR_HIGHLIGHTTEXT	14
#define COLOR_BTNFACE		15
typedef struct
{
	LONG tmHeight;
	LONG tmAscent;
	LONG tmDescent;
	LONG tmInternalLeading;
	LONG tmExternalLeading;
	LONG tmAveCharWidth;
	LONG tmMaxCharWidth;
} TEXTMETRIC;
extern HFONT    CreateFont(INT cHeight,INT cWidth,INT cEscapement,INT cOrientation,INT cWeight,DWORD bItalic,DWORD bUnderline,DWORD bStrikeOut,DWORD iCharSet,DWORD iOutPrecision,DWORD iClipPrecision,DWORD iQuality,DWORD iPitchAndFamily,LPCTSTR pszFaceName);
static __inline INT GetDeviceCaps(HDC hdc,INT index) { return 72; } // LOGPIXELSY, font sizes in points
extern BOOL     GetTextMetrics(HDC hdc,TEXTMETRIC *lptm);
extern COLORREF SetTextColor(HDC hdc,COLORREF color);
extern COLORREF SetBkColor(HDC hdc,COLORREF color);
extern COLORREF GetTextColor(HDC hdc);
extern COLORREF GetBkColor(HDC hdc);
extern DWORD    GetSysColor(INT nIndex);
extern BOOL     ExtTextOut(HDC hdc,INT x,INT y,UINT options,const RECT *lprect,LPCTSTR lpString,UINT c,const INT *lpDx);
extern BOOL     DrawFocusRect(HDC hdc,const RECT *lprc);

static __inline BOOL SetRect(LPRECT lprc,INT xLeft,INT yTop,INT xRight,INT yBottom)
{
	lprc->left = xLeft; lprc->top = yTop; lprc->right = xRight; lprc->bottom = yBottom;
	return TRUE;
}
static __inline BOOL IsRectEmpty(const RECT *lprc)
{
	return lprc->left >= lprc->right || lprc->top >= lprc->bottom;
}

//################
//#
//#    USER subset (main window, implemented in SdlMain.c)
//#
//################

#define WS_OVERLAPPED		0x00000000
#define WS_POPUP			0x80000000
#define WS_CHILD			0x40000000
#define WS_MINIMIZEBOX		0x00020000
#define WS_VISIBLE			0x10000000
#define WS_CLIPSIBLINGS		0x04000000
#define WS_CAPTION			0x00C00000
#define WS_SYSMENU			0x00080000
#define GWL_STYLE			(-16)
#define SWP_NOSIZE			0x0001
#define SWP_NOMOVE			0x0002
#define SWP_NOZORDER		0x0004
#define HWND_TOPMOST		((HWND)-1)
#define HWND_NOTOPMOST		((HWND)-2)

#define MB_OK				0x00000000
#define MB_OKCANCEL			0x00000001
#define MB_YESNOCANCEL		0x00000003
#define MB_YESNO			0x00000004
#define MB_ICONSTOP			0x00000010
#define MB_ICONEXCLAMATION	0x00000030
#define MB_ICONINFORMATION	0x00000040
#define MB_DEFBUTTON2		0x00000100
#define MB_APPLMODAL		0x00000000
#define MB_SETFOREGROUND	0x00010000
#define IDOK				1
#define IDCANCEL			2
#define IDYES				6
#define IDNO				7

#define VK_BACK				0x08
#define VK_TAB				0x09
#define VK_RETURN			0x0D
#define VK_SHIFT			0x10
#define VK_CONTROL			0x11
#define VK_MENU				0x12
#define VK_ESCAPE			0x1B
#define VK_SPACE			0x20
#define VK_PRIOR			0x21
#define VK_NEXT				0x22
#define VK_END				0x23
#define VK_HOME				0x24
#define VK_LEFT				0x25
#define VK_UP				0x26
#define VK_RIGHT			0x27
#define VK_DOWN				0x28
#define VK_INSERT			0x2D
#define VK_DELETE			0x2E
#define VK_APPS				0x5D
#define VK_ADD				0x6B
#define VK_SUBTRACT			0x6D
#define VK_F2				0x71
#define VK_F5				0x74
#define VK_F6				0x75
#define VK_F7				0x76
#define VK_F8				0x77
#define VK_F9				0x78
#define VK_F11				0x7A

#define WM_USER				0x0400
#define WM_DESTROY			0x0002
#define WM_SETREDRAW		0x000B
#define WM_SETCURSOR		0x0020
#define WM_DRAWITEM			0x002B
#define WM_MEASUREITEM		0x002C
#define WM_VKEYTOITEM		0x002E
#define WM_CONTEXTMENU		0x007B
#define WM_CTLCOLORSTATIC	0x0138
#define WM_LBUTTONUP		0x0202
#define CW_USEDEFAULT		((INT)0x80000000)
#define SW_RESTORE			9
#define CWP_SKIPDISABLED	0x0002
typedef struct tagPOINTS { SHORT x; SHORT y; } POINTS;
#define MAKEPOINTS(l)		(*((POINTS *)&(l)))
#define POINTSTOPOINT(pt,pts) { (pt).x = (pts).x; (pt).y = (pts).y; }

extern INT  MessageBox(HWND hWnd,LPCTSTR lpText,LPCTSTR lpCaption,UINT uType);
static __inline BOOL MessageBeep(UINT uType) { return TRUE; } // no system sound
extern BOOL InvalidateRect(HWND hWnd,const RECT *lpRect,BOOL bErase);
extern BOOL SetWindowText(HWND hWnd,LPCTSTR lpString);
static __inline BOOL AdjustWindowRect(LPRECT lpRect,DWORD dwStyle,BOOL bMenu) { return TRUE; } // client size is window size
extern BOOL SetWindowPos(HWND hWnd,HWND hWndInsertAfter,INT X,INT Y,INT cx,INT cy,UINT uFlags);
extern BOOL GetClientRect(HWND hWnd,LPRECT lpRect);
extern LONG_PTR GetWindowLongPtr(HWND hWnd,INT nIndex);
extern LONG_PTR SetWindowLongPtr(HWND hWnd,INT nIndex,LONG_PTR dwNewLong);
extern HMENU GetMenu(HWND hWnd);
extern BOOL  SetMenu(HWND hWnd,HMENU hMenu);
#define GCLP_HCURSOR		(-12)
#define GetClassLongPtr(h,i)	((ULONG_PTR) 0)	// no class cursor
extern HCURSOR SetCursor(HCURSOR hCursor);
extern BOOL SetWindowRgn(HWND hWnd,HRGN hRgn,BOOL bRedraw);
extern BOOL PostMessage(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam);
extern LRESULT SendMessage(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam);

#define WM_COMMAND			0x0111
#define WM_SYSCOMMAND		0x0112
#define WM_COPYDATA			0x004A
#define WM_SETICON			0x0080
#define WM_CLOSE			0x0010
#define SC_CLOSE			0xF060
#define MK_LBUTTON			0x0001
#define MK_RBUTTON			0x0002
#define ICON_SMALL			0
#define ICON_BIG			1
#define IMAGE_ICON			1
#define LR_LOADFROMFILE		0x0010
#define LR_DEFAULTSIZE		0x0040
#define LR_SHARED			0x8000
#define MAXLONG				0x7fffffff
#define FAR
#define __cdecl
#define min(a,b)			(((a) < (b)) ? (a) : (b))
#define max(a,b)			(((a) > (b)) ? (a) : (b))

#define KL_NAMELENGTH		9
#define LANG_NEUTRAL		0x00
#define SUBLANG_NEUTRAL		0x00
#define PRIMARYLANGID(l)	((WORD)(l) & 0x3ff)
#define SUBLANGID(l)		((WORD)(l) >> 10)

typedef BITMAPINFOHEADER	*PBITMAPINFOHEADER;
typedef LOGPALETTE			*PLOGPALETTE;
typedef RGNDATA				*PRGNDATA;
typedef char				*PSZ;
typedef const char			*LPCCH;

typedef struct tagWINDOWPLACEMENT
{
	UINT  length;
	UINT  flags;
	UINT  showCmd;
	POINT ptMinPosition;
	POINT ptMaxPosition;
	RECT  rcNormalPosition;
} WINDOWPLACEMENT;

extern BOOL DestroyWindow(HWND hWnd);
extern VOID DragAcceptFiles(HWND hWnd,BOOL fAccept);
extern BOOL GetKeyboardLayoutName(LPTSTR pwszKLID);
extern BOOL GetWindowPlacement(HWND hWnd,WINDOWPLACEMENT *lpwndpl);
extern BOOL SetWindowPlacement(HWND hWnd,const WINDOWPLACEMENT *lpwndpl);
static __inline HANDLE LoadImage(HINSTANCE hInst,LPCTSTR name,UINT type,INT cx,INT cy,UINT fuLoad) { return NULL; } // no window icons
extern BOOL GetWindowRect(HWND hWnd,LPRECT lpRect);
extern BOOL ShowWindow(HWND hWnd,INT nCmdShow);
extern HWND SetFocus(HWND hWnd);
extern HWND GetActiveWindow(VOID);
extern HWND GetParent(HWND hWnd);
extern INT  GetDlgCtrlID(HWND hWnd);
extern INT  GetWindowText(HWND hWnd,LPTSTR lpString,INT nMaxCount);
extern INT  GetWindowTextLength(HWND hWnd);
extern HWND ChildWindowFromPointEx(HWND hWndParent,POINT pt,UINT uFlags);
extern HDC  GetDC(HWND hWnd);
static __inline INT ReleaseDC(HWND hWnd,HDC hDC) { return DeleteDC(hDC); }
#define GWLP_USERDATA		(-21)

// menus
#define MF_BYCOMMAND		0x0000
#define MF_STRING			0x0000
#define MF_ENABLED			0x0000
#define MF_GRAYED			0x0001
#define MF_UNCHECKED		0x0000
#define MF_CHECKED			0x0008
#define MF_POPUP			0x0010
#define MF_BYPOSITION		0x0400
#define MF_SEPARATOR		0x0800
extern HMENU GetSubMenu(HMENU hMenu,INT nPos);
extern INT   GetMenuItemCount(HMENU hMenu);
extern UINT  GetMenuItemID(HMENU hMenu,INT nPos);
extern INT   GetMenuString(HMENU hMenu,UINT uIDItem,LPTSTR lpString,INT cchMax,UINT flags);
extern UINT  GetMenuState(HMENU hMenu,UINT uId,UINT uFlags);
extern BOOL  DeleteMenu(HMENU hMenu,UINT uPosition,UINT uFlags);
extern BOOL  InsertMenu(HMENU hMenu,UINT uPosition,UINT uFlags,UINT_PTR uIDNewItem,LPCTSTR lpNewItem);
static __inline BOOL IsMenu(HMENU hMenu) { return hMenu != NULL; }
extern UINT  EnableMenuItem(HMENU hMenu,UINT uIDEnableItem,UINT uEnable);
extern BOOL  DestroyMenu(HMENU hMenu);
extern HMENU LoadMenu(HINSTANCE hInstance,LPCTSTR lpMenuName);
extern HMENU CreatePopupMenu(VOID);
extern BOOL  AppendMenu(HMENU hMenu,UINT uFlags,UINT_PTR uIDNewItem,LPCTSTR lpNewItem);
extern DWORD CheckMenuItem(HMENU hMenu,UINT uIDCheckItem,UINT uCheck);
extern HMENU GetSystemMenu(HWND hWnd,BOOL bRevert);
extern BOOL  TrackPopupMenu(HMENU hMenu,UINT uFlags,INT x,INT y,INT nReserved,HWND hWnd,const RECT *prcRect);

// dialog boxes (implemented in SdlDlg.c)
#define IDC_STATIC			(-1)
#define WS_DISABLED			0x08000000
#define WS_BORDER			0x00800000
#define WS_VSCROLL			0x00200000
#define WS_GROUP			0x00020000
#define WS_TABSTOP			0x00010000
#define WS_EX_TOOLWINDOW	0x00000080
#define DS_MODALFRAME		0x00000080
#define ES_MULTILINE		0x00000004
#define ES_AUTOHSCROLL		0x00000080
#define ES_OEMCONVERT		0x00000400
#define ES_READONLY			0x00000800
#define ES_NUMBER			0x00002000
#define SS_REALSIZEIMAGE	0x00000800
#define BS_AUTOCHECKBOX		0x00000003
#define BS_AUTORADIOBUTTON	0x00000009
#define BS_TYPEMASK			0x0000000F
#define LBS_NOTIFY			0x00000001
#define LBS_SORT			0x00000002
#define LBS_OWNERDRAWFIXED	0x00000010
#define LBS_HASSTRINGS		0x00000040
#define LBS_NOINTEGRALHEIGHT 0x00000100
#define LBS_WANTKEYBOARDINPUT 0x00000400
#define LBS_EXTENDEDSEL		0x00000800
#define LBS_NOSEL			0x00004000
#define CBS_DROPDOWN		0x00000002
#define CBS_DROPDOWNLIST	0x00000003
#define CBS_AUTOHSCROLL		0x00000040
#define CBS_OEMCONVERT		0x00000080
#define CBS_SORT			0x00000100
#define TBS_AUTOTICKS		0x00000001
#define BST_UNCHECKED		0
#define BST_CHECKED			1
#define WM_INITDIALOG		0x0110
#define BN_CLICKED			0
#define EN_CHANGE			0x0300
#define LBN_SELCHANGE		1
#define LBN_DBLCLK			2
#define LBN_SETFOCUS		4
#define LBN_KILLFOCUS		5
#define CBN_SETFOCUS		3
#define BM_SETCHECK			0x00F1
#define LB_INSERTSTRING		0x0181
#define LB_DELETESTRING		0x0182
#define LB_GETTOPINDEX		0x018E
#define LB_GETITEMRECT		0x0198
#define LB_SETCARETINDEX	0x019E
#define LB_GETCARETINDEX	0x019F
#define CB_FINDSTRINGEXACT	0x0158
#define CB_SELECTSTRING		0x014D
#define ODS_SELECTED		0x0001
#define ODS_FOCUS			0x0010
typedef struct
{
	UINT      CtlType;
	UINT      CtlID;
	UINT      itemID;
	UINT      itemWidth;
	UINT      itemHeight;
	ULONG_PTR itemData;
} MEASUREITEMSTRUCT, *LPMEASUREITEMSTRUCT;
typedef struct
{
	UINT      CtlType;
	UINT      CtlID;
	UINT      itemID;
	UINT      itemAction;
	UINT      itemState;
	HWND      hwndItem;
	HDC       hDC;
	RECT      rcItem;
	ULONG_PTR itemData;
} DRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;
#define CBN_SELENDOK		9
#define EM_SETSEL			0x00B1
#define LB_ADDSTRING		0x0180
#define LB_RESETCONTENT		0x0184
#define LB_SETSEL			0x0185
#define LB_SETCURSEL		0x0186
#define LB_GETSEL			0x0187
#define LB_GETCURSEL		0x0188
#define LB_GETTEXT			0x0189
#define LB_GETTEXTLEN		0x018A
#define LB_GETCOUNT			0x018B
#define LB_SETTOPINDEX		0x0197
#define LB_GETSELCOUNT		0x0190
#define LB_GETSELITEMS		0x0191
#define LB_SETITEMDATA		0x019A
#define LB_GETITEMDATA		0x0199
#define LB_SELITEMRANGE		0x019B
#define LB_ERR				(-1)
#define CB_ADDSTRING		0x0143
#define CB_RESETCONTENT		0x014B
#define CB_GETCURSEL		0x0147
#define CB_SETCURSEL		0x014E
#define CB_GETCOUNT			0x0146
#define CB_GETLBTEXT		0x0148
#define CB_GETITEMDATA		0x0150
#define CB_SETITEMDATA		0x0151
#define CB_ERR				(-1)
#define TBM_GETPOS			0x0400
#define TBM_SETPOS			0x0405
#define TBM_SETRANGE		0x0406
#define TBM_SETTICFREQ		0x0414
#define MAKEWPARAM(l,h)		((WPARAM)(DWORD)MAKELONG(l,h))
#define MAKELPARAM(l,h)		((LPARAM)(DWORD)MAKELONG(l,h))
#define WM_SETFONT			0x0030
extern INT_PTR DialogBoxParam(HINSTANCE hInstance,LPCTSTR lpTemplateName,HWND hWndParent,DLGPROC lpDialogFunc,LPARAM dwInitParam);
#define DialogBox(i,t,w,f)	DialogBoxParam(i,t,w,f,0)
extern BOOL    EndDialog(HWND hDlg,INT_PTR nResult);
extern HWND    GetDlgItem(HWND hDlg,INT nIDDlgItem);
extern BOOL    SetDlgItemText(HWND hDlg,INT nIDDlgItem,LPCTSTR lpString);
extern UINT    GetDlgItemText(HWND hDlg,INT nIDDlgItem,LPTSTR lpString,INT cchMax);
extern BOOL    CheckDlgButton(HWND hDlg,INT nIDButton,UINT uCheck);
extern UINT    IsDlgButtonChecked(HWND hDlg,INT nIDButton);
extern LRESULT SendDlgItemMessage(HWND hDlg,INT nIDDlgItem,UINT Msg,WPARAM wParam,LPARAM lParam);
extern BOOL    EnableWindow(HWND hWnd,BOOL bEnable);
static __inline HWND GetLastActivePopup(HWND hWnd) { return hWnd; }
extern HWND    CreateDialog(HINSTANCE hInstance,LPCTSTR lpTemplateName,HWND hWndParent,DLGPROC lpDialogFunc);

// toolbar (implemented in SdlDlg.c)
typedef struct { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR, *LPNMHDR;
#define InitCommonControls()
#define TB_ENABLEBUTTON		(WM_USER + 1)
#define TBSTATE_ENABLED		0x04
#define TBSTYLE_BUTTON		0x00
#define TBSTYLE_SEP			0x01
#define TBSTYLE_TOOLTIPS	0x0100
#define TTN_GETDISPINFO		((UINT) -530)
typedef struct
{
	INT       iBitmap;
	INT       idCommand;
	BYTE      fsState;
	BYTE      fsStyle;
	DWORD_PTR dwData;
	INT_PTR   iString;
} TBBUTTON;
typedef struct
{
	NMHDR     hdr;
	LPTSTR    lpszText;
	TCHAR     szText[80];
	HINSTANCE hinst;
	UINT      uFlags;
	LPARAM    lParam;
} TOOLTIPTEXT, *LPTOOLTIPTEXT;
extern HWND CreateToolbarEx(HWND hwnd,DWORD ws,UINT wID,INT nBitmaps,HINSTANCE hBMInst,UINT_PTR wBMID,const TBBUTTON *lpButtons,INT iNumButtons,INT dxButton,INT dyButton,INT dxBitmap,INT dyBitmap,UINT uStructSize);

// resources (implemented in SdlUser.c)
typedef LPVOID				HRSRC;
extern HRSRC   FindResource(HMODULE hModule,LPCTSTR lpName,LPCTSTR lpType);
extern HGLOBAL LoadResource(HMODULE hModule,HRSRC hResInfo);
#define LockResource(h)		((LPVOID) (h))
static __inline BOOL FreeResource(HGLOBAL hResData) { return TRUE; }
extern HBITMAP LoadBitmap(HINSTANCE hInstance,LPCTSTR lpBitmapName);
extern INT     LoadString(HINSTANCE hInstance,UINT uID,LPTSTR lpBuffer,INT cchBufferMax);
#define LoadIcon(i,n)		((HICON) NULL)	// no window icons

// property sheets (implemented in SdlDlg.c)
#define WM_NOTIFY			0x004E
#define PSN_KILLACTIVE		((UINT) -201)
#define PSP_DEFAULT			0x00000000
#define PSH_PROPSHEETPAGE	0x00000008
#define PSH_NOAPPLYNOW		0x00000080
#define PSH_USECALLBACK		0x00000100
#define PSCB_INITIALIZED	1
#define PSCB_PRECREATE		2
typedef struct { DWORD style; DWORD dwExtendedStyle; WORD cdit; SHORT x,y,cx,cy; } DLGTEMPLATE, *LPDLGTEMPLATE;
typedef INT (CALLBACK *PFNPROPSHEETCALLBACK)(HWND hwndDlg,UINT uMsg,LPARAM lParam);
typedef struct
{
	DWORD     dwSize;
	DWORD     dwFlags;
	HINSTANCE hInstance;
	LPCTSTR   pszTemplate;
	HICON     hIcon;
	LPCTSTR   pszTitle;
	DLGPROC   pfnDlgProc;
	LPARAM    lParam;
	LPVOID    pfnCallback;
} PROPSHEETPAGE;
typedef const PROPSHEETPAGE	*LPCPROPSHEETPAGE;
typedef struct
{
	DWORD     dwSize;
	DWORD     dwFlags;
	HWND      hwndParent;
	HINSTANCE hInstance;
	HICON     hIcon;
	LPCTSTR   pszCaption;
	UINT      nPages;
	UINT      nStartPage;
	LPCPROPSHEETPAGE ppsp;
	PFNPROPSHEETCALLBACK pfnCallback;
} PROPSHEETHEADER;
extern INT_PTR PropertySheet(PROPSHEETHEADER *lppsph);

// folder selection (implemented in SdlUser.c)
#define BIF_RETURNONLYFSDIRS	0x0001
#define BIF_STATUSTEXT		0x0004
#define BFFM_INITIALIZED	1
#define BFFM_SELCHANGED		2
#define BFFM_SETSTATUSTEXT	(WM_USER + 100)
#define BFFM_SETSELECTION	(WM_USER + 103)
#define SUCCEEDED(hr)		((hr) >= 0)
typedef LPTSTR				LPITEMIDLIST;	// selected path
typedef INT (CALLBACK *BFFCALLBACK)(HWND hwnd,UINT uMsg,LPARAM lParam,LPARAM lpData);
typedef struct
{
	HWND        hwndOwner;
	LPITEMIDLIST pidlRoot;
	LPTSTR      pszDisplayName;
	LPCTSTR     lpszTitle;
	UINT        ulFlags;
	BFFCALLBACK lpfn;
	LPARAM      lParam;
	INT         iImage;
} BROWSEINFO;
typedef struct IMalloc *LPMALLOC;
typedef struct
{
	VOID  (*Free)(LPMALLOC pThis,LPVOID pv);
	ULONG (*Release)(LPMALLOC pThis);
} IMallocVtbl;
struct IMalloc { const IMallocVtbl *lpVtbl; };
extern LONG         SHGetMalloc(LPMALLOC *ppMalloc);
extern LPITEMIDLIST SHBrowseForFolder(BROWSEINFO *lpbi);
extern BOOL         SHGetPathFromIDList(LPITEMIDLIST pidl,LPTSTR pszPath);

// clipboard (text and DIB as BMP image)
#define CF_TEXT				1
#define CF_DIB				8
#define CF_PALETTE			9
#define CF_UNICODETEXT		13
static __inline BOOL OpenClipboard(HWND hWndNewOwner) { return TRUE; }
extern BOOL   CloseClipboard(VOID);
static __inline BOOL EmptyClipboard(VOID) { return TRUE; }
extern HANDLE SetClipboardData(UINT uFormat,HANDLE hMem);
extern HANDLE GetClipboardData(UINT uFormat);
extern BOOL   IsClipboardFormatAvailable(UINT format);

// locale
#define LOCALE_USER_DEFAULT	0x0400
#define LOCALE_SDECIMAL		0x000E
extern INT GetLocaleInfo(DWORD Locale,DWORD LCType,LPTSTR lpLCData,INT cchData);
#define _tcsspn				strspn

// common file dialogs
#define OFN_OVERWRITEPROMPT	0x00000002
#define OFN_HIDEREADONLY	0x00000004
#define OFN_PATHMUSTEXIST	0x00000800
#define OFN_FILEMUSTEXIST	0x00001000
#define OFN_CREATEPROMPT	0x00002000
#define OFN_EXPLORER		0x00080000
typedef struct tagOFN
{
	DWORD   lStructSize;
	HWND    hwndOwner;
	HINSTANCE hInstance;
	LPCTSTR lpstrFilter;
	LPTSTR  lpstrCustomFilter;
	DWORD   nMaxCustFilter;
	DWORD   nFilterIndex;
	LPTSTR  lpstrFile;
	DWORD   nMaxFile;
	LPTSTR  lpstrFileTitle;
	DWORD   nMaxFileTitle;
	LPCTSTR lpstrInitialDir;
	LPCTSTR lpstrTitle;
	DWORD   Flags;
	WORD    nFileOffset;
	WORD    nFileExtension;
	LPCTSTR lpstrDefExt;
} OPENFILENAME, *LPOPENFILENAME;
extern BOOL GetOpenFileName(LPOPENFILENAME lpofn);
extern BOOL GetSaveFileName(LPOPENFILENAME lpofn);

// shell (implemented in SdlUser.c)
#define SW_SHOWNORMAL		1
extern HINSTANCE ShellExecute(HWND hwnd,LPCTSTR lpOperation,LPCTSTR lpFile,LPCTSTR lpParameters,LPCTSTR lpDirectory,INT nShowCmd);

// path names
#define _MAX_DRIVE			3
#define _MAX_DIR			MAX_PATH
#define _MAX_FNAME			MAX_PATH
#define _MAX_EXT			MAX_PATH
extern VOID _tsplitpath(LPCTSTR path,LPTSTR drive,LPTSTR dir,LPTSTR fname,LPTSTR ext);
extern VOID _tmakepath(LPTSTR path,LPCTSTR drive,LPCTSTR dir,LPCTSTR fname,LPCTSTR ext);

//################
//#
//#    Winsock subset (BSD sockets)
//#
//################

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

typedef INT					SOCKET;
typedef struct sockaddr_in	SOCKADDR_IN;
typedef struct sockaddr		*LPSOCKADDR;
typedef struct hostent		*PHOSTENT;
typedef struct in_addr		*PIN_ADDR;
#define INVALID_SOCKET		(-1)
#define SOCKET_ERROR		(-1)
#define closesocket			close
typedef struct { WORD wVersion; } WSADATA;

#endif // _SDLPORT_H
