/*
 *   SdlMain.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// main program and main window of the SDL3 frontend (macOS, Linux)

#include "pch.h"
#include "resource.h"
#include "Emu48.h"
#include "io.h"
#include "kml.h"
#include "sdlmain.h"

#include <sys/un.h>
#include <signal.h>

SDL_Window *pSdlWindow = NULL;

static SDL_Renderer *pRenderer = NULL;
static SDL_Texture  *pTexture = NULL;
static HBITMAP hWindowBitmap = NULL;		// pixels of main window
static SDL_AtomicInt nInvalidate;			// window must be redrawn
static BOOL bQuit = FALSE;					// leave main loop
static Uint32 uSdlUserEvent = 0;			// event type for PostMessage()
static INT nWindowZoom = 200;				// window zoom in percent
static INT nMenuHeight = 0;					// height of menu bar in window
static BOOL bAcceptFiles = FALSE;			// dropping files allowed
static DWORD dwWindowStyle = STYLE_TITLE;	// GWL_STYLE of main window
static HRGN hWindowRgn = NULL;				// window shape owned by the window
static BOOL bWindowMove = FALSE;			// window is moved with the mouse
static float fMoveX,fMoveY;					// mouse position in window when moving started
static LPTSTR pszDropFile[16];				// dropped files
static UINT nDropFiles = 0;
static SOCKET sInstance = INVALID_SOCKET;	// listening socket of the single instance

//################
//#
//#    Single instance
//#
//################

// connected socket to the running instance, the instance is found by
// the Unix domain socket "Emu48.sock" in the preference directory
static SOCKET InstanceConnect(struct sockaddr_un *psAddr)
{
	LPTSTR lpszPref = SDL_GetPrefPath(NULL,"Emu48");
	SOCKET s = INVALID_SOCKET;

	SDL_zerop(psAddr);
	psAddr->sun_family = AF_UNIX;
	if (   lpszPref != NULL
		&& SDL_snprintf(psAddr->sun_path,sizeof(psAddr->sun_path),"%sEmu48.sock",lpszPref) < (int) sizeof(psAddr->sun_path)
		&& (s = socket(AF_UNIX,SOCK_STREAM,0)) != INVALID_SOCKET
		&& connect(s,(struct sockaddr *) psAddr,sizeof(*psAddr)) != 0)
	{
		closesocket(s);						// no running instance
		s = INVALID_SOCKET;
	}
	SDL_free(lpszPref);
	return s;
}

// receive the state file names of other instances
static int SDLCALL InstanceThread(void *pData)
{
	SOCKET s;

	while ((s = accept(sInstance,NULL,NULL)) != INVALID_SOCKET)
	{
		LPTSTR szFilename = (LPTSTR) calloc(MAX_PATH,sizeof(TCHAR));
		INT    nLen = 0, nRead;

		// full path file name with EOS, only EOS if the other instance has none,
		// nothing from an instance without "Single Instance"
		while (nLen < MAX_PATH - 1 && (nRead = (INT) read(s,szFilename + nLen,MAX_PATH - 1 - nLen)) > 0)
			nLen += nRead;
		closesocket(s);
		if (nLen > 0)
			PostMessage(hWnd,WM_COPYDATA,0,(LPARAM) szFilename);
		else
			free(szFilename);
	}
	return 0;
	UNREFERENCED_PARAMETER(pData);
}

// remove the socket of this instance
static VOID InstanceStop(VOID)
{
	struct sockaddr_un sAddr;
	socklen_t nLen = sizeof(sAddr);

	if (sInstance == INVALID_SOCKET) return;
	if (getsockname(sInstance,(struct sockaddr *) &sAddr,&nLen) == 0 && sAddr.sun_path[0])
		unlink(sAddr.sun_path);
	closesocket(sInstance);					// InstanceThread() ends with the program
	sInstance = INVALID_SOCKET;
	return;
}

// with "Single Instance" a running instance gets the state file name and this one quits,
// else this instance becomes the instance found by others when no instance is running
static BOOL InstanceStart(VOID)
{
	struct sockaddr_un sAddr;
	SOCKET s = InstanceConnect(&sAddr);

	if (s != INVALID_SOCKET)				// running an instance of me
	{
		if (bSingleInstance)
		{
			LPTSTR lpFilePart;				// address of file name in path

			szBufferFilename[0] = 0;
			if (nArgc >= 2)					// get full path file name
				GetFullPathName(ppArgv[1],ARRAYSIZEOF(szBufferFilename),szBufferFilename,&lpFilePart);
			signal(SIGPIPE,SIG_IGN);		// no termination when the instance closed the socket
			write(s,szBufferFilename,lstrlen(szBufferFilename) + 1);
		}
		closesocket(s);
		return bSingleInstance;
	}
	if (sAddr.sun_path[0] == 0) return FALSE; // no preference directory

	unlink(sAddr.sun_path);					// socket of a terminated instance
	if (   (sInstance = socket(AF_UNIX,SOCK_STREAM,0)) == INVALID_SOCKET
		|| bind(sInstance,(struct sockaddr *) &sAddr,sizeof(sAddr)) != 0
		|| listen(sInstance,4) != 0)
	{
		InstanceStop();
		return FALSE;
	}
	SDL_DetachThread(SDL_CreateThread(InstanceThread,"Instance",NULL));
	return FALSE;
}

// state file of another instance like WM_COPYDATA of the Windows version
static VOID OnInstanceFile(LPTSTR szFilename)
{
	if (SDL_GetWindowFlags(pSdlWindow) & SDL_WINDOW_MINIMIZED)
		SDL_RestoreWindow(pSdlWindow);		// show window
	ForceForegroundWindow(hWnd);			// put the window into foreground

	// current instance has document loaded and got a file name
	if (bDocumentAvail && *szFilename)
	{
		TCHAR  szActFilename[MAX_PATH];
		LPTSTR lpFilePart;					// address of file name in path

		lstrcpyn(szBufferFilename,szFilename,ARRAYSIZEOF(szBufferFilename));

		// get full path file name for actual state file
		GetFullPathName(szCurrentFilename,ARRAYSIZEOF(szActFilename),szActFilename,&lpFilePart);

		// check if both file names are unequal
		if (lstrcmp(szBufferFilename,szActFilename) != 0)
		{
			UINT nCurState;

			if (pbyRom)
			{
				nCurState = SwitchToState(SM_INVALID);
				if (IDCANCEL == SaveChanges(bAutoSave))
					goto cancel;
			}
			if (OpenDocument(szBufferFilename)) // open new file
			{
				MruAdd(szBufferFilename);
			}
cancel:
			if (pbyRom) SwitchToState(nCurState);
		}
	}
	free(szFilename);
	return;
}

//################
//#
//#    Win32 window functions
//#
//################

BOOL InvalidateRect(HWND hWnd,const RECT *lpRect,BOOL bErase)
{
	SDL_SetAtomicInt(&nInvalidate,1);		// redraw complete window in main loop
	return TRUE;
	UNREFERENCED_PARAMETER(hWnd);
	UNREFERENCED_PARAMETER(lpRect);
	UNREFERENCED_PARAMETER(bErase);
}

// window zoom in percent from the actual window size
static INT GetWindowZoom(VOID)
{
	RECT rc;
	int w,h;

	GetClientRect(hWnd,&rc);
	SDL_GetWindowSize(pSdlWindow,&w,&h);
	return (rc.right > 0) ? w * 100 / rc.right : nWindowZoom;
}

// window size for the zoomed window bitmap and the menu bar
static VOID SizeWindow(VOID)
{
	SDL_Rect r = { 0, 0, 0, 0 };
	RECT rc;
	int  x,y,w,h,nTop = 0;

	GetClientRect(hWnd,&rc);
	nMenuHeight = MenuBarHeight();
	if (rc.right <= 0 || rc.bottom <= 0) return;

	// window with title bar must fit into the usable display area (without menu bar and dock)
	if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(pSdlWindow),&r))
	{
		SDL_GetWindowBordersSize(pSdlWindow,&nTop,NULL,NULL,NULL);
		nWindowZoom = __min(nWindowZoom,r.w * 100 / rc.right);
		nWindowZoom = __min(nWindowZoom,(r.h - nTop - nMenuHeight) * 100 / rc.bottom);
		nWindowZoom = __max(nWindowZoom,10);	// a window size of 0 has no aspect ratio
	}
	w = rc.right * nWindowZoom / 100;
	h = rc.bottom * nWindowZoom / 100 + nMenuHeight;
	// new aspect ratio before resizing, an aspect ratio 0:0 makes macOS fail on windows
	// larger than the screen
	SDL_SetWindowAspectRatio(pSdlWindow,(float) w / h,(float) w / h);
	SDL_SetWindowSize(pSdlWindow,w,h);
	SDL_SyncWindow(pSdlWindow);				// X11 resizes asynchronously

	// move window up or left when it ends outside the usable display area
	SDL_GetWindowPosition(pSdlWindow,&x,&y);
	if (r.w > 0 && (x + w > r.x + r.w || y + h > r.y + r.h))
	{
		SDL_SetWindowPosition(pSdlWindow,__max(r.x,__min(x,r.x + r.w - w)),__max(r.y + nTop,__min(y,r.y + r.h - h)));
	}
	return;
}

// position of the scaled window bitmap below the menu bar
static SDL_FRect GetBitmapRect(VOID)
{
	SDL_FRect r = { 0.0f, 0.0f, 1.0f, 1.0f };
	RECT rc;
	int  w,h;

	GetClientRect(hWnd,&rc);
	SDL_GetWindowSize(pSdlWindow,&w,&h);
	h -= nMenuHeight;
	if (rc.right > 0 && rc.bottom > 0 && h > 0)
	{
		r.w = (float) w;					// fit into window with same aspect ratio
		r.h = (float) rc.bottom * w / rc.right;
		if (r.h > h)
		{
			r.h = (float) h;
			r.w = (float) rc.right * h / rc.bottom;
		}
		r.x = (w - r.w) / 2;
		r.y = nMenuHeight + (h - r.h) / 2;
	}
	return r;
}

// convert window position to window bitmap position
static VOID WindowToBitmap(float *px,float *py)
{
	SDL_FRect r = GetBitmapRect();
	RECT rc;

	GetClientRect(hWnd,&rc);
	*px = (*px - r.x) * rc.right / r.w;
	*py = (*py - r.y) * rc.bottom / r.h;
	return;
}

BOOL SetWindowPos(HWND hWnd,HWND hWndInsertAfter,INT X,INT Y,INT cx,INT cy,UINT uFlags)
{
	if (hWnd == &sMainWnd && (uFlags & SWP_NOSIZE) == 0 && cx > 0 && cy > 0)
	{
		HBITMAP hNewBitmap = CreateCompatibleBitmap(hWindowDC,cx,cy);

		nWindowZoom = GetWindowZoom();			// keep zoom set by the user

		EnterCriticalSection(&csGDILock);	// the CPU thread draws into hWindowDC
		{
			SelectObject(hWindowDC,hNewBitmap);
			DeleteObject(hWindowBitmap);
			hWindowBitmap = hNewBitmap;
		}
		LeaveCriticalSection(&csGDILock);

		SizeWindow();
		SDL_DestroyTexture(pTexture);
		pTexture = SDL_CreateTexture(pRenderer,SDL_PIXELFORMAT_XRGB8888,SDL_TEXTUREACCESS_STREAMING,cx,cy);
		#if SDL_VERSION_ATLEAST(3,4,0)
		SDL_SetTextureScaleMode(pTexture,SDL_SCALEMODE_PIXELART);
		#else
		SDL_SetTextureScaleMode(pTexture,SDL_SCALEMODE_NEAREST);
		#endif
	}
	if ((uFlags & SWP_NOMOVE) == 0)
		SDL_SetWindowPosition(GetSdlWindow(hWnd),X,Y);
	if (hWndInsertAfter == HWND_TOPMOST || hWndInsertAfter == HWND_NOTOPMOST)
		SDL_SetWindowAlwaysOnTop(GetSdlWindow(hWnd),hWndInsertAfter == HWND_TOPMOST);
	return TRUE;
}

BOOL GetClientRect(HWND hWnd,LPRECT lpRect)
{
	INT nWidth = 0, nHeight = 0, nStride;

	if (hWindowBitmap) GetBitmapBits(hWindowBitmap,&nWidth,&nHeight,&nStride);
	return SetRect(lpRect,0,0,nWidth,nHeight);
	UNREFERENCED_PARAMETER(hWnd);
}

BOOL GetWindowPlacement(HWND hWnd,WINDOWPLACEMENT *lpwndpl)
{
	return GetWindowRect(hWnd,&lpwndpl->rcNormalPosition);
}

BOOL SetWindowPlacement(HWND hWnd,const WINDOWPLACEMENT *lpwndpl)
{
	SDL_SetWindowPosition(GetSdlWindow(hWnd),lpwndpl->rcNormalPosition.left,lpwndpl->rcNormalPosition.top);
	return TRUE;
}

LONG_PTR GetWindowLongPtr(HWND hWnd,INT nIndex)
{
	if (nIndex == GWLP_USERDATA) return *GetUserData(hWnd);
	return (nIndex == GWL_STYLE) ? dwWindowStyle : 0;
}

// GWL_STYLE switches the title bar
LONG_PTR SetWindowLongPtr(HWND hWnd,INT nIndex,LONG_PTR dwNewLong)
{
	LONG_PTR dwOldLong = GetWindowLongPtr(hWnd,nIndex);

	if (nIndex == GWLP_USERDATA) *GetUserData(hWnd) = dwNewLong;

	if (nIndex == GWL_STYLE)
	{
		dwWindowStyle = (DWORD) dwNewLong;
		SDL_SetWindowBordered(pSdlWindow,(dwWindowStyle & WS_CAPTION) == WS_CAPTION);
	}
	return dwOldLong;
}

HCURSOR SetCursor(HCURSOR hCursor)
{
	if (hCursor) SDL_SetCursor((SDL_Cursor *) hCursor);
	return hCursor;
}

// window shape from the rectangles of the region, the window owns the region
BOOL SetWindowRgn(HWND hWnd,HRGN hRgn,BOOL bRedraw)
{
	SDL_Surface *pShape = NULL;
	RECT rc;

	DeleteObject(hWindowRgn);
	hWindowRgn = hRgn;
	GetClientRect(hWnd,&rc);
	if (hRgn != NULL && (pShape = SDL_CreateSurface(rc.right,rc.bottom,SDL_PIXELFORMAT_ARGB8888)) != NULL)
	{
		const RECT *pRects;
		UINT i,nRects = GetRegionRects(hRgn,&pRects);

		SDL_FillSurfaceRect(pShape,NULL,0);	// transparent outside region
		for (i = 0; i < nRects; ++i)
		{
			SDL_Rect r = { pRects[i].left, pRects[i].top, pRects[i].right - pRects[i].left, pRects[i].bottom - pRects[i].top };
			SDL_FillSurfaceRect(pShape,&r,0xFFFFFFFF);
		}
	}
	SDL_SetWindowShape(pSdlWindow,pShape);
	SDL_DestroySurface(pShape);
	return TRUE;
	UNREFERENCED_PARAMETER(bRedraw);
}

// the message is sent to the window by the main loop or a modal dialog loop
BOOL PostMessage(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	LPMSG pMsg = (LPMSG) malloc(sizeof(*pMsg));
	SDL_Event e;

	ZeroMemory(pMsg,sizeof(*pMsg));
	pMsg->hwnd = hWnd;
	pMsg->message = Msg;
	pMsg->wParam = wParam;
	pMsg->lParam = lParam;
	SDL_zero(e);
	e.type = uSdlUserEvent;
	e.user.data1 = pMsg;
	return SDL_PushEvent(&e);
}

// send message of PostMessage() to its window
VOID DispatchPosted(SDL_Event *pEvent)
{
	LPMSG pMsg;

	if (pEvent->type != uSdlUserEvent) return;
	pMsg = (LPMSG) pEvent->user.data1;
	if (pMsg->hwnd == &sMainWnd)
	{
		if (pMsg->message == WM_COMMAND)
			OnCommand(pMsg->wParam);
		if (pMsg->message == WM_SYSCOMMAND && (pMsg->wParam & 0xFFF0) == SC_CLOSE)
			OnFileExit();
		if (pMsg->message == WM_COPYDATA)	// state file of another instance
			OnInstanceFile((LPTSTR) pMsg->lParam);
	}
	else if (IsDialog(pMsg->hwnd))
	{
		SendMessage(pMsg->hwnd,pMsg->message,pMsg->wParam,pMsg->lParam);
	}
	free(pMsg);
	return;
}

BOOL DestroyWindow(HWND hWnd)
{
	if (hWnd != &sMainWnd) return DialogDestroy(hWnd);
	bQuit = TRUE;							// leave main loop
	return TRUE;
}

VOID DragAcceptFiles(HWND hWnd,BOOL fAccept)
{
	bAcceptFiles = fAccept;
	return;
	UNREFERENCED_PARAMETER(hWnd);
}

BOOL GetKeyboardLayoutName(LPTSTR pwszKLID)
{
	lstrcpy(pwszKLID,_T("00000409"));		// US keyboard layout
	return TRUE;
}

//################
//#
//#    Keyboard
//#
//################

// translate SDL key to Windows virtual key code used by KML scripts, letters by
// their label like on Windows, all other keys by their position on a US keyboard
// (the keyboard layout reported to KML scripts), so they work with any layout
BYTE VirtualKey(const SDL_KeyboardEvent *pKey)
{
	SDL_Keycode  k = pKey->key;
	SDL_Scancode s = pKey->scancode;

	if (k >= SDLK_A && k <= SDLK_Z) return (BYTE) ('A' + (k - SDLK_A));
	if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) return (BYTE) ('A' + (s - SDL_SCANCODE_A)); // no Latin letter
	if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9) return (BYTE) ('1' + (s - SDL_SCANCODE_1));
	if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F12) return (BYTE) (0x70 + (s - SDL_SCANCODE_F1));
	if (s >= SDL_SCANCODE_KP_1 && s <= SDL_SCANCODE_KP_9) return (BYTE) (0x61 + (s - SDL_SCANCODE_KP_1));

	switch (s)
	{
	case SDL_SCANCODE_0:            return '0';
	case SDL_SCANCODE_KP_0:         return 0x60;
	case SDL_SCANCODE_KP_MULTIPLY:  return 0x6A;
	case SDL_SCANCODE_KP_PLUS:      return VK_ADD;
	case SDL_SCANCODE_KP_MINUS:     return VK_SUBTRACT;
	case SDL_SCANCODE_KP_PERIOD:    return 0x6E;
	case SDL_SCANCODE_KP_DIVIDE:    return 0x6F;
	case SDL_SCANCODE_KP_ENTER:     return VK_RETURN;
	case SDL_SCANCODE_RETURN:       return VK_RETURN;
	case SDL_SCANCODE_BACKSPACE:    return VK_BACK;
	case SDL_SCANCODE_TAB:          return VK_TAB;
	case SDL_SCANCODE_ESCAPE:       return VK_ESCAPE;
	case SDL_SCANCODE_SPACE:        return VK_SPACE;
	case SDL_SCANCODE_PAGEUP:       return VK_PRIOR;
	case SDL_SCANCODE_PAGEDOWN:     return VK_NEXT;
	case SDL_SCANCODE_END:          return VK_END;
	case SDL_SCANCODE_HOME:         return VK_HOME;
	case SDL_SCANCODE_LEFT:         return VK_LEFT;
	case SDL_SCANCODE_UP:           return VK_UP;
	case SDL_SCANCODE_RIGHT:        return VK_RIGHT;
	case SDL_SCANCODE_DOWN:         return VK_DOWN;
	case SDL_SCANCODE_INSERT:       return VK_INSERT;
	case SDL_SCANCODE_DELETE:       return VK_DELETE;
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT:       return VK_SHIFT;
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL:        return VK_CONTROL;
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT:         return VK_MENU;
	case SDL_SCANCODE_SEMICOLON:    return 0xBA;
	case SDL_SCANCODE_EQUALS:       return 0xBB;
	case SDL_SCANCODE_COMMA:        return 0xBC;
	case SDL_SCANCODE_MINUS:        return 0xBD;
	case SDL_SCANCODE_PERIOD:       return 0xBE;
	case SDL_SCANCODE_SLASH:        return 0xBF;
	case SDL_SCANCODE_GRAVE:        return 0xC0;
	case SDL_SCANCODE_LEFTBRACKET:  return 0xDB;
	case SDL_SCANCODE_BACKSLASH:    return 0xDC;
	case SDL_SCANCODE_RIGHTBRACKET: return 0xDD;
	case SDL_SCANCODE_APOSTROPHE:   return 0xDE;
	case SDL_SCANCODE_NONUSBACKSLASH: return 0xE2; // < > key of ISO keyboards
	default:                        return 0;    // no mapping
	}
}

//################
//#
//#    Main window
//#
//################

// mouse and keyboard input goes to the calculator
static __inline BOOL IsInput(VOID)
{
	return nMacroState != MACRO_PLAY && nState == SM_RUN;
}

static LPCTSTR DropFileName(UINT nIndex, LPVOID pData)
{
	return pszDropFile[nIndex];
	UNREFERENCED_PARAMETER(pData);
}

// collect the files of a drop operation and load them
static VOID OnDrop(const SDL_DropEvent *pDrop)
{
	switch (pDrop->type)
	{
	case SDL_EVENT_DROP_FILE:
		if (nDropFiles < ARRAYSIZEOF(pszDropFile))
			pszDropFile[nDropFiles++] = SDL_strdup(pDrop->data);
		break;
	case SDL_EVENT_DROP_COMPLETE:
		if (bAcceptFiles && nState == SM_RUN && nDropFiles > 0)
			LoadObjectFiles(nDropFiles,DropFileName,NULL);
		while (nDropFiles > 0) SDL_free(pszDropFile[--nDropFiles]);
		break;
	default:
		break;
	}
	return;
}

// redraw window content like WM_PAINT
static VOID OnPaint(VOID)
{
	RECT rcPaint;
	HDC hPaintDC;

	UpdateWindowBars();						// update visibility of title and menu bar

	hPaintDC = CreateCompatibleDC(hWindowDC);
	SelectObject(hPaintDC,hWindowBitmap);	// paint DC with origin 0,0 like BeginPaint()
	GetClientRect(hWnd,&rcPaint);
	PaintMainWindow(hPaintDC,&rcPaint);
	DeleteDC(hPaintDC);
	return;
}

// copy window bitmap to screen
static VOID OnPresent(VOID)
{
	float fDensity = SDL_GetWindowPixelDensity(pSdlWindow);
	SDL_FRect r = GetBitmapRect();
	INT nWidth,nHeight,nStride;
	int w,h;

	EnterCriticalSection(&csGDILock);
	{
		LPBYTE pbyBits = GetBitmapBits(hWindowBitmap,&nWidth,&nHeight,&nStride);
		SDL_UpdateTexture(pTexture,NULL,pbyBits,nStride);
	}
	LeaveCriticalSection(&csGDILock);

	SDL_SetRenderScale(pRenderer,fDensity,fDensity);
	SDL_RenderClear(pRenderer);
	SDL_RenderTexture(pRenderer,pTexture,NULL,&r);
	SDL_GetWindowSize(pSdlWindow,&w,&h);
	MenuDraw(w,h);
	SDL_RenderPresent(pRenderer);
	return;
}

// update window content
VOID MainWindowUpdate(VOID)
{
	if (bWindowMove)						// move window while holding the left mouse button
	{
		float x,y;

		// the mouse doesn't move inside the moving window, so poll the global position
		bWindowMove = (SDL_GetGlobalMouseState(&x,&y) & SDL_BUTTON_LMASK) != 0;
		SDL_SetWindowPosition(pSdlWindow,(int) (x - fMoveX),(int) (y - fMoveY));
	}
	if (SDL_GetAtomicInt(&nInvalidate))
	{
		SDL_SetAtomicInt(&nInvalidate,0);
		OnPaint();
	}
	OnPresent();
	return;
}

static VOID OnEvent(SDL_Event *pEvent)
{
	switch (pEvent->type)
	{
	case SDL_EVENT_QUIT:
		OnFileExit();
		break;
	case SDL_EVENT_WINDOW_EXPOSED:
		InvalidateRect(hWnd,NULL,FALSE);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		{
			float x = pEvent->button.x, y = pEvent->button.y;
			BOOL  bButton;
			int   nX,nY;

			WindowToBitmap(&x,&y);
			bButton = MouseIsButton((DWORD) x,(DWORD) y);

			// context menu in client area not over a button
			if (pEvent->button.button == SDL_BUTTON_RIGHT && !bButton)
				MenuContext(pEvent->button.x,pEvent->button.y);
			// right button like WM_RBUTTONDOWN of the Windows version, holds the key
			if (pEvent->button.button == SDL_BUTTON_RIGHT && IsInput())
				MouseButtonDownAt(MK_RBUTTON,(DWORD) x,(DWORD) y);
			if (pEvent->button.button != SDL_BUTTON_LEFT) break;

			if (IsInput()) MouseButtonDownAt(MK_LBUTTON,(DWORD) x,(DWORD) y);

			// no title bar or window movement over client enabled and hit area not over a button
			if ((!bTitleBar || bClientWinMove) && !bButton)
			{
				SDL_GetGlobalMouseState(&fMoveX,&fMoveY);
				SDL_GetWindowPosition(pSdlWindow,&nX,&nY);
				fMoveX -= nX;				// mouse position in window
				fMoveY -= nY;
				bWindowMove = TRUE;
			}
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		WindowToBitmap(&pEvent->button.x,&pEvent->button.y);
		if (pEvent->button.button == SDL_BUTTON_LEFT && IsInput())
			MouseButtonUpAt(0,(DWORD) pEvent->button.x,(DWORD) pEvent->button.y);
		break;
	case SDL_EVENT_MOUSE_MOTION:
		if (bWindowMove) break;				// window is moved in MainWindowUpdate()
		WindowToBitmap(&pEvent->motion.x,&pEvent->motion.y);
		if (IsInput())
			MouseMovesTo((pEvent->motion.state & SDL_BUTTON_LMASK) ? MK_LBUTTON : 0,
						 (DWORD) pEvent->motion.x,(DWORD) pEvent->motion.y);
		break;
	case SDL_EVENT_DROP_FILE:
	case SDL_EVENT_DROP_COMPLETE:
		OnDrop(&pEvent->drop);
		break;
	case SDL_EVENT_KEY_DOWN:
		{
			UINT nId = MenuAccelerator(&pEvent->key);

			if (nId != 0)					// accelerator like TranslateAccelerator()
			{
				OnCommand(nId);
				break;
			}
		}
		// call RunKey() only once (suppress autorepeat feature)
		if (IsInput() && !pEvent->key.repeat)
			RunKey(VirtualKey(&pEvent->key),TRUE);
		break;
	case SDL_EVENT_KEY_UP:
		if (IsInput())
			RunKey(VirtualKey(&pEvent->key),FALSE);
		break;
	default:
		DispatchPosted(pEvent);				// message from PostMessage()
		break;
	}
	return;
}

int main(int argc, char *argv[])
{
	TCHAR  szTemp[MAX_PATH+8];
	LPTSTR lpFilePart;

	SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK,"1"); // Ctrl+click opens context menu
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH,"1"); // click into inactive window like on Windows
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
	{
		fprintf(stderr,"SDL initialization failed: %s\n",SDL_GetError());
		return 1;
	}
	uSdlUserEvent = SDL_RegisterEvents(1);

	nArgc = argc;							// no. of command line arguments
	ppArgv = (LPCTSTR *) argv;				// command line arguments

	#if defined __APPLE__
	// started by the Finder in "/", the folder of Emu48.app like the folder of Emu48.exe on Windows
	if (GetCurrentDirectory(ARRAYSIZEOF(szTemp),szTemp) == 1 && SDL_GetBasePath() != NULL)
	{
		lstrcpyn(szTemp,SDL_GetBasePath(),ARRAYSIZEOF(szTemp));
		if ((lpFilePart = strstr(szTemp,_T(".app/"))) != NULL)
		{
			*lpFilePart = 0;				// app bundle name
			*strrchr(szTemp,_T('/')) = 0;	// folder of app bundle
			SetCurrentDirectory(szTemp);
		}
	}
	#endif

	// read emulator settings
	GetCurrentDirectory(ARRAYSIZEOF(szCurrentDirectory),szCurrentDirectory);
	ReadSettings();

	// running an instance of me?
	if (InstanceStart())
	{
		SDL_Quit();
		return 0;							// quit program
	}

	nWindowZoom = ReadSettingsInt(_T("Emulator"),_T("WindowZoom"),nWindowZoom);
	if (nWindowZoom < 50 || nWindowZoom > 800) nWindowZoom = 200;

	// create window
	if (!SDL_CreateWindowAndRenderer(szNoTitle,256 * nWindowZoom / 100,64 * nWindowZoom / 100,SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_TRANSPARENT,&pSdlWindow,&pRenderer))
	{
		fprintf(stderr,"Window creation failed: %s\n",SDL_GetError());
		return 1;
	}
	SDL_SetRenderVSync(pRenderer,1);
	SDL_SetRenderDrawColor(pRenderer,0,0,0,SDL_ALPHA_OPAQUE); // transparent window only for shapes
	hWnd = &sMainWnd;

	// like WM_CREATE
	CreateLocks();
	MenuInit(pSdlWindow,pRenderer);
	hCursorArrow = (HCURSOR) SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
	hCursorHand  = (HCURSOR) SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
	hWindowDC = CreateCompatibleDC(NULL);
	hWindowBitmap = CreateCompatibleBitmap(hWindowDC,256,64);
	SelectObject(hWindowDC,hWindowBitmap);
	SetWindowPos(hWnd,NULL,0,0,256,64,SWP_NOMOVE);

	// initialization
	QueryPerformanceFrequency(&lFreq);		// init high resolution counter
	QueryPerformanceCounter(&lAppStart);

	szCurrentKml[0] = 0;					// no KML file selected
	SetSpeed(bRealSpeed);					// set speed
	MruInit(4);								// init MRU entries

	// create auto event handle
	hEventShutdn = CreateEvent(NULL,FALSE,FALSE,NULL);

	nState     = SM_RUN;					// init state must be <> nNextState
	nNextState = SM_INVALID;				// go into invalid state
	hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)&WorkerThread,NULL,0,NULL);
	while (nState!=nNextState) Sleep(0);	// wait for thread initialized

	SoundOpen(uWaveDevId);					// open waveform-audio output device

	if (nArgc >= 2)							// use decoded parameter line
		lstrcpyn(szBufferFilename,ppArgv[1],ARRAYSIZEOF(szBufferFilename));
	else									// use last document setting
		ReadLastDocument(szBufferFilename, ARRAYSIZEOF(szBufferFilename));

	if (szBufferFilename[0])				// given default document
	{
		lstrcpy(szTemp,_T("Loading "));
		lstrcpyn(&szTemp[8],szBufferFilename,MAX_PATH);
		SetWindowTitle(szTemp);
		if (OpenDocument(szBufferFilename))
		{
			MruAdd(szCurrentFilename);
			goto start;
		}
	}

	SetWindowTitle(_T("New Document"));

	// no default document, ask for new one
	if (NewDocument()) SetWindowTitle(_T("Untitled"));

start:
	if (bStartupBackup) SaveBackup();		// make a RAM backup at startup
	if (pbyRom) SwitchToState(SM_RUN);

	// message loop
	while (!bQuit)
	{
		SDL_Event e;

		MenuInput(TRUE);
		DialogInput(TRUE);
		if (SDL_WaitEventTimeout(&e,10))
		{
			do if (!DialogEvent(&e) && !MenuEvent(&e)) OnEvent(&e); while (!bQuit && SDL_PollEvent(&e));
		}
		DialogInput(FALSE);
		MenuInput(FALSE);
		MainWindowUpdate();
		DialogUpdate();
	}

	// like WM_DESTROY
	if (hThread) SwitchToState(SM_RETURN);	// exit emulation thread
	SetWindowTitle(NULL);					// free memory of title

	SoundClose();							// close waveform-audio output device

	// get full path name of szCurrentFilename
	if (GetFullPathName(szCurrentFilename,ARRAYSIZEOF(szBufferFilename),szBufferFilename,&lpFilePart) == 0)
		szBufferFilename[0] = 0;			// no last document name

	WriteLastDocument(szBufferFilename);	// save last document setting
	WriteSettings();						// save emulation settings
	WriteSettingsInt(_T("Emulator"),_T("WindowZoom"),GetWindowZoom());

	CloseHandle(hThread);					// close thread handle
	CloseHandle(hEventShutdn);				// close event handle
	ResetDocument();
	ResetBackup();
	MruCleanup();
	DeleteLocks();

	InstanceStop();
	MenuCleanup();
	SDL_DestroyRenderer(pRenderer);
	SDL_DestroyWindow(pSdlWindow);
	SDL_Quit();
	return 0;
}
