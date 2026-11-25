/*
 *   SdlUser.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Win32 user interface functions (message boxes, file dialogs,
// clipboard, path names) on top of SDL3

#include "pch.h"
#include "resource.h"
#include "Emu48.h"
#include "sdlmain.h"
#include <locale.h>

//################
//#
//#    Message boxes
//#
//################

INT MessageBox(HWND hWnd,LPCTSTR lpText,LPCTSTR lpCaption,UINT uType)
{
	SDL_MessageBoxButtonData sButton[3];
	SDL_MessageBoxData sData;
	INT nButtons = 0, nDefault, nResult = IDCANCEL;

	switch (uType & 0x0F)					// buttons
	{
	case MB_YESNOCANCEL:
		sButton[nButtons].buttonID = IDYES;    sButton[nButtons++].text = "Yes";
		sButton[nButtons].buttonID = IDNO;     sButton[nButtons++].text = "No";
		sButton[nButtons].buttonID = IDCANCEL; sButton[nButtons++].text = "Cancel";
		break;
	case MB_YESNO:
		sButton[nButtons].buttonID = IDYES;    sButton[nButtons++].text = "Yes";
		sButton[nButtons].buttonID = IDNO;     sButton[nButtons++].text = "No";
		break;
	case MB_OKCANCEL:
		sButton[nButtons].buttonID = IDOK;     sButton[nButtons++].text = "OK";
		sButton[nButtons].buttonID = IDCANCEL; sButton[nButtons++].text = "Cancel";
		break;
	default:
		sButton[nButtons].buttonID = IDOK;     sButton[nButtons++].text = "OK";
		break;
	}

	nDefault = ((uType & MB_DEFBUTTON2) && nButtons > 1) ? 1 : 0;
	sButton[nDefault].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
	for (nResult = 0; nResult < nButtons; ++nResult)
		if (nResult != nDefault) sButton[nResult].flags = 0;
	sButton[nButtons-1].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

	SDL_zero(sData);
	switch (uType & 0xF0)					// icon
	{
	case MB_ICONSTOP:        sData.flags = SDL_MESSAGEBOX_ERROR;       break;
	case MB_ICONEXCLAMATION: sData.flags = SDL_MESSAGEBOX_WARNING;     break;
	default:                 sData.flags = SDL_MESSAGEBOX_INFORMATION; break;
	}
	sData.window = GetParentWindow(hWnd);
	sData.title = lpCaption ? lpCaption : szNoTitle;
	sData.message = lpText;
	sData.numbuttons = nButtons;
	sData.buttons = sButton;

	nResult = IDCANCEL;
	if (!SDL_ShowMessageBox(&sData,&nResult)) nResult = IDCANCEL;
	return nResult;
	UNREFERENCED_PARAMETER(hWnd);
}

// the SDL message box is already centered on its parent window
int AppMessageBox(HWND hwndParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
	return MessageBox(hwndParent, lpText, lpCaption, uType);
}

//################
//#
//#    File dialogs
//#
//################

typedef struct
{
	SDL_AtomicInt nDone;					// dialog closed
	TCHAR szFile[MAX_PATH];					// selected file, empty on cancel
} DIALOGRESULT;

static void SDLCALL DialogCallback(void *pUserData,const char * const *ppFileList,int nFilter)
{
	DIALOGRESULT *p = (DIALOGRESULT *) pUserData;

	p->szFile[0] = 0;
	if (ppFileList != NULL && ppFileList[0] != NULL)
		lstrcpyn(p->szFile,ppFileList[0],ARRAYSIZEOF(p->szFile));
	SDL_SetAtomicInt(&p->nDone,1);
	return;
	UNREFERENCED_PARAMETER(nFilter);
}

// Win32 file filters as SDL filters, "*.E48;*.E49" as "e48;E48;e49;E49", the patterns
// of Linux file dialogs are case sensitive
static INT ConvertFilter(LPCTSTR lpstrFilter,SDL_DialogFileFilter *pFilter,LPTSTR lpszBuf,INT nMax)
{
	INT n = 0;

	while (lpstrFilter && *lpstrFilter && n < nMax)
	{
		LPCTSTR lpszPattern = lpstrFilter + lstrlen(lpstrFilter) + 1;

		pFilter[n].name = lpstrFilter;
		pFilter[n].pattern = lpszBuf;
		while (*lpszPattern)				// each "*.ext" of the pattern
		{
			LPCTSTR lpszExt = lpszPattern + 2;
			INT nLen = (INT) strcspn(lpszExt,";");

			if (*lpszExt == '*')			// all files
			{
				*lpszBuf++ = '*';
			}
			else							// extension in lower and in original case
			{
				INT i;

				for (i = 0; i < nLen; ++i) *lpszBuf++ = (TCHAR) tolower(lpszExt[i]);
				*lpszBuf++ = ';';
				CopyMemory(lpszBuf,lpszExt,nLen);
				lpszBuf += nLen;
			}
			lpszPattern = lpszExt + nLen;
			if (*lpszPattern == ';') *lpszBuf++ = *lpszPattern++;
		}
		*lpszBuf++ = 0;
		++n;
		lpstrFilter = lpszPattern + 1;		// next filter
	}
	return n;
}

// modal SDL file or folder dialog, szFile contains the selection
BOOL FileDialog(HWND hOwner,SDL_FileDialogType eType,const SDL_DialogFileFilter *pFilter,INT nFilters,LPCTSTR szDefault,LPTSTR szFile,INT nMax)
{
	SDL_PropertiesID nProps = SDL_CreateProperties();
	DIALOGRESULT sResult;

	SDL_zero(sResult);
	SDL_SetPointerProperty(nProps,SDL_PROP_FILE_DIALOG_WINDOW_POINTER,GetParentWindow(hOwner));
	SDL_SetPointerProperty(nProps,SDL_PROP_FILE_DIALOG_FILTERS_POINTER,(void *) pFilter);
	SDL_SetNumberProperty(nProps,SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER,nFilters);
	if (szDefault && *szDefault) SDL_SetStringProperty(nProps,SDL_PROP_FILE_DIALOG_LOCATION_STRING,szDefault);
	SDL_ShowFileDialogWithProperties(eType,DialogCallback,&sResult,nProps);
	SDL_DestroyProperties(nProps);

	while (SDL_GetAtomicInt(&sResult.nDone) == 0) // modal like Win32
	{
		SDL_PumpEvents();
		MainWindowUpdate();					// calculator keeps running
		SDL_Delay(10);
	}
	if (sResult.szFile[0] == 0) return FALSE; // cancel
	lstrcpyn(szFile,sResult.szFile,nMax);
	return TRUE;
}

static BOOL OpenFileName(LPOPENFILENAME lpofn,BOOL bSave)
{
	SDL_DialogFileFilter sFilter[8];
	TCHAR szPatterns[512];
	LPTSTR p;
	INT n;

	n = ConvertFilter(lpofn->lpstrFilter,sFilter,szPatterns,ARRAYSIZEOF(sFilter));

	// SDL preselects the first filter, move the selected filter to the front
	if (lpofn->nFilterIndex > 1 && lpofn->nFilterIndex <= (DWORD) n)
	{
		SDL_DialogFileFilter sSelected = sFilter[lpofn->nFilterIndex-1];
		MoveMemory(&sFilter[1],&sFilter[0],(lpofn->nFilterIndex-1) * sizeof(sFilter[0]));
		sFilter[0] = sSelected;
	}
	#if defined __APPLE__
	// the macOS dialog has no filter selection and allows the files of all filters
	n = __min(n,1);
	#endif

	if (!FileDialog(lpofn->hwndOwner,bSave ? SDL_FILEDIALOG_SAVEFILE : SDL_FILEDIALOG_OPENFILE,sFilter,n,NULL,lpofn->lpstrFile,lpofn->nMaxFile))
		return FALSE;

	// add default extension on save
	p = strrchr(lpofn->lpstrFile,'/');
	p = p ? p + 1 : lpofn->lpstrFile;
	if (bSave && strchr(p,'.') == NULL && lpofn->lpstrDefExt
		&& lstrlen(lpofn->lpstrFile) + 1 + lstrlen(lpofn->lpstrDefExt) < (INT) lpofn->nMaxFile)
	{
		lstrcat(lpofn->lpstrFile,_T("."));
		lstrcat(lpofn->lpstrFile,lpofn->lpstrDefExt);
	}

	// offsets of file name and extension
	lpofn->nFileOffset = (WORD) (p - lpofn->lpstrFile);
	p = strrchr(p,'.');
	lpofn->nFileExtension = p ? (WORD) (p + 1 - lpofn->lpstrFile) : 0;
	return TRUE;
}

BOOL GetOpenFileName(LPOPENFILENAME lpofn)
{
	return OpenFileName(lpofn,FALSE);
}

BOOL GetSaveFileName(LPOPENFILENAME lpofn)
{
	return OpenFileName(lpofn,TRUE);
}

//################
//#
//#    Shell
//#
//################

// open a document in the directory with the default application, the manual in the help window
HINSTANCE ShellExecute(HWND hwnd,LPCTSTR lpOperation,LPCTSTR lpFile,LPCTSTR lpParameters,LPCTSTR lpDirectory,INT nShowCmd)
{
	TCHAR szUrl[2*MAX_PATH];

	if (lstrcmp(lpFile,_T("Emu48.htm")) == 0)	// the manual of Help Topics is the help window
	{
		HelpTopics(hwnd);
		return (HINSTANCE) 33;
	}
	SDL_snprintf(szUrl,ARRAYSIZEOF(szUrl),"file://%s%s%s",lpDirectory ? lpDirectory : "",
				 (lpDirectory && *lpDirectory && lpDirectory[lstrlen(lpDirectory)-1] != '/') ? "/" : "",lpFile);
	return (HINSTANCE) (INT_PTR) (SDL_OpenURL(szUrl) ? 33 : 2); // > 32 success
	UNREFERENCED_PARAMETER(hwnd);
	UNREFERENCED_PARAMETER(lpOperation);
	UNREFERENCED_PARAMETER(lpParameters);
	UNREFERENCED_PARAMETER(nShowCmd);
}

//################
//#
//#    Path names
//#
//################

VOID _tsplitpath(LPCTSTR path,LPTSTR drive,LPTSTR dir,LPTSTR fname,LPTSTR ext)
{
	LPCTSTR pszName = strrchr(path,'/');
	LPCTSTR pszExt;

	pszName = pszName ? pszName + 1 : path;
	if ((pszExt = strrchr(pszName,'.')) == NULL) pszExt = pszName + lstrlen(pszName);

	if (drive) drive[0] = 0;				// no drives
	if (dir)								// with '\\' like Windows for GetCutPathName()
	{
		memcpy(dir,path,pszName - path);
		dir[pszName - path] = 0;
		for (; *dir; ++dir) if (*dir == '/') *dir = '\\';
	}
	if (fname)
	{
		memcpy(fname,pszName,pszExt - pszName);
		fname[pszExt - pszName] = 0;
	}
	if (ext) lstrcpy(ext,pszExt);
	return;
}

VOID _tmakepath(LPTSTR path,LPCTSTR drive,LPCTSTR dir,LPCTSTR fname,LPCTSTR ext)
{
	path[0] = 0;
	if (dir && *dir)
	{
		lstrcat(path,dir);
		if (path[lstrlen(path)-1] != '/' && path[lstrlen(path)-1] != '\\') lstrcat(path,_T("/"));
	}
	if (fname) lstrcat(path,fname);
	if (ext && *ext)
	{
		if (*ext != '.') lstrcat(path,_T("."));
		lstrcat(path,ext);
	}
	for (; *path; ++path) if (*path == '\\') *path = '/';
	return;
	UNREFERENCED_PARAMETER(drive);
}

//################
//#
//#    Clipboard (text only)
//#
//################

static LPTSTR lpszClipText = NULL;			// text read from clipboard

BOOL CloseClipboard(VOID)
{
	SDL_free(lpszClipText);
	lpszClipText = NULL;
	return TRUE;
}

// the clipboard takes ownership of hMem like on Windows
static LPBYTE pbyClipImage = NULL;			// BMP file of CF_DIB
static size_t nClipImageSize = 0;

static const void * SDLCALL ClipImage(void *pUserData,const char *szMimeType,size_t *pnSize)
{
	*pnSize = nClipImageSize;
	return pbyClipImage;
	UNREFERENCED_PARAMETER(pUserData);
	UNREFERENCED_PARAMETER(szMimeType);
}

static void SDLCALL ClipImageCleanup(void *pUserData)
{
	free(pbyClipImage);
	pbyClipImage = NULL;
	return;
	UNREFERENCED_PARAMETER(pUserData);
}

// the clipboard takes ownership of hMem like on Windows
HANDLE SetClipboardData(UINT uFormat,HANDLE hMem)
{
	static const char *szMimeType[] = { "image/bmp" };
	LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER) hMem;
	BITMAPFILEHEADER bfh;
	DWORD dwColors,dwSize;

	switch (uFormat)
	{
	case CF_TEXT:
		SDL_SetClipboardText((LPCTSTR) hMem);
		break;
	case CF_DIB:							// DIB as BMP file
		dwColors = DibColors(lpbi);
		dwSize = lpbi->biSize + dwColors * sizeof(RGBQUAD) + lpbi->biSizeImage;
		ZeroMemory(&bfh,sizeof(bfh));
		bfh.bfType = 0x4D42;				// "BM"
		bfh.bfSize = sizeof(bfh) + dwSize;
		bfh.bfOffBits = sizeof(bfh) + lpbi->biSize + dwColors * sizeof(RGBQUAD);
		if ((pbyClipImage = (LPBYTE) malloc(bfh.bfSize)) != NULL)
		{
			CopyMemory(pbyClipImage,&bfh,sizeof(bfh));
			CopyMemory(pbyClipImage + sizeof(bfh),lpbi,dwSize);
			nClipImageSize = bfh.bfSize;
			SDL_SetClipboardData(ClipImage,ClipImageCleanup,NULL,szMimeType,ARRAYSIZEOF(szMimeType));
		}
		break;
	case CF_PALETTE:						// palette object
		DeleteObject((HGDIOBJ) hMem);
		return hMem;
	}
	GlobalFree(hMem);
	return hMem;
}

HANDLE GetClipboardData(UINT uFormat)
{
	if (uFormat != CF_TEXT) return NULL;
	SDL_free(lpszClipText);
	lpszClipText = SDL_GetClipboardText();
	return (HANDLE) lpszClipText;
}

BOOL IsClipboardFormatAvailable(UINT format)
{
	return format == CF_TEXT && SDL_HasClipboardText();
}

INT GetLocaleInfo(DWORD Locale,DWORD LCType,LPTSTR lpLCData,INT cchData)
{
	_ASSERT(LCType == LOCALE_SDECIMAL);
	lstrcpyn(lpLCData,localeconv()->decimal_point,cchData);
	return lstrlen(lpLCData) + 1;
	UNREFERENCED_PARAMETER(Locale);
}

//################
//#
//#    Resources
//#
//################

// the debugger toolbar, the strings and the bitmaps of Emu48.rc, extracted by CMake
static WORD wToolbarRc[] = { 1,
	#include "toolbar.rc"
};

static const struct { UINT uID; LPCTSTR szText; } sStringRc[] =
{
	#include "string.rc"
};

#include "bitmap.rc"
static const struct { UINT uID; const BYTE *pbyData; } sBitmapRc[] = { BITMAP_TABLE };

// only the toolbar resource, MFC toolbar data
HRSRC FindResource(HMODULE hModule,LPCTSTR lpName,LPCTSTR lpType)
{
	if ((ULONG_PTR) lpName != IDR_DEBUG_TOOLBAR) return NULL;
	return (HRSRC) wToolbarRc;
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(lpType);
}

HGLOBAL LoadResource(HMODULE hModule,HRSRC hResInfo)
{
	static WORD wData[ARRAYSIZEOF(wToolbarRc) + 1];

	// version, width, height, item count, items
	wData[0] = wToolbarRc[0];
	wData[1] = wToolbarRc[1];
	wData[2] = wToolbarRc[2];
	wData[3] = (WORD) (ARRAYSIZEOF(wToolbarRc) - 3);
	CopyMemory(&wData[4],&wToolbarRc[3],(ARRAYSIZEOF(wToolbarRc) - 3) * sizeof(WORD));
	return (HGLOBAL) wData;
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(hResInfo);
}

HBITMAP LoadBitmap(HINSTANCE hInstance,LPCTSTR lpBitmapName)
{
	UINT i;

	for (i = 0; i < ARRAYSIZEOF(sBitmapRc); ++i)
	{
		if (sBitmapRc[i].uID == (UINT) (ULONG_PTR) lpBitmapName)
		{
			const BITMAPINFO *pbmi = (const BITMAPINFO *) (sBitmapRc[i].pbyData + sizeof(BITMAPFILEHEADER));
			const BYTE *pbyBits = sBitmapRc[i].pbyData + ((const BITMAPFILEHEADER *) sBitmapRc[i].pbyData)->bfOffBits;
			return CreateDIBitmap(NULL,&pbmi->bmiHeader,CBM_INIT,pbyBits,pbmi,DIB_RGB_COLORS);
		}
	}
	return NULL;
	UNREFERENCED_PARAMETER(hInstance);
}

INT LoadString(HINSTANCE hInstance,UINT uID,LPTSTR lpBuffer,INT cchBufferMax)
{
	UINT i;

	*lpBuffer = 0;
	for (i = 0; i < ARRAYSIZEOF(sStringRc); ++i)
		if (sStringRc[i].uID == uID) lstrcpyn(lpBuffer,sStringRc[i].szText,cchBufferMax);
	return lstrlen(lpBuffer);
	UNREFERENCED_PARAMETER(hInstance);
}
