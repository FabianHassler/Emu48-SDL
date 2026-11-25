/*
 *   Commands.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 1995 Sebastien Carlier
 *   Copyright (C) 2026 Fabian Hassler (split from Emu48.c)
 *
 */

// emulator state and menu commands common to all platforms

#include "pch.h"
#include "resource.h"
#include "Emu48.h"
#include "io.h"
#include "kml.h"
#include "debugger.h"

#define VERSION   "1.68"

#ifdef _DEBUG
LPCTSTR szNoTitle = _T("Emu48 ")_T(VERSION)_T(" Debug");
#else
LPCTSTR szNoTitle = _T("Emu48 ")_T(VERSION);
#endif
LPTSTR szAppName = _T("Emu48");				// application name for DDE server
LPTSTR szTopic   = _T("Stack");				// topic for DDE server
LPTSTR szTitle   = NULL;

const LPCTSTR szLicence =
	_T("Emu48/SDL, the SDL3 port for macOS and Linux:\r\n")
	_T(EMU48SDL_URL)_T("\r\n")
	_T("\r\n")
	_T("This program is free software; you can redistribute it and/or modify\r\n")
	_T("it under the terms of the GNU General Public License as published by\r\n")
	_T("the Free Software Foundation; either version 2 of the License, or\r\n")
	_T("(at your option) any later version.\r\n")
	_T("\r\n")
	_T("This program is distributed in the hope that it will be useful,\r\n")
	_T("but WITHOUT ANY WARRANTY; without even the implied warranty of\r\n")
	_T("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\r\n")
	_T("See the GNU General Public License for more details.\r\n")
	_T("\r\n")
	_T("You should have received a copy of the GNU General Public License along\r\n")
	_T("with this program; if not, write to the Free Software Foundation, Inc.,\r\n")
	_T("51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.");


CRITICAL_SECTION csGDILock;					// critical section for hWindowDC
CRITICAL_SECTION csLcdLock;					// critical section for display update
CRITICAL_SECTION csKeyLock;					// critical section for key scan
CRITICAL_SECTION csIOLock;					// critical section for I/O access
CRITICAL_SECTION csT1Lock;					// critical section for timer1 access
CRITICAL_SECTION csT2Lock;					// critical section for timer2 access
CRITICAL_SECTION csTxdLock;					// critical section for transmit byte
CRITICAL_SECTION csRecvLock;				// critical section for receive byte
CRITICAL_SECTION csSlowLock;				// critical section for speed slow down
CRITICAL_SECTION csDbgLock;					// critical section for	debugger purpose
INT              nArgc;						// no. of command line arguments
LPCTSTR          *ppArgv;					// command line arguments
LARGE_INTEGER    lFreq;						// high performance counter frequency
LARGE_INTEGER    lAppStart;					// high performance counter value at Appl. start
DWORD            idDdeInst;					// DDE server id
UINT             uCF_HpObj;					// DDE clipboard format
HANDLE           hThread;
HANDLE           hEventShutdn;				// event handle to stop cpu thread

HINSTANCE        hApp = NULL;
HWND             hWnd = NULL;
HWND             hDlgDebug = NULL;			// handle for debugger dialog
HWND             hDlgFind = NULL;			// handle for debugger find dialog
HWND             hDlgProfile = NULL;		// handle for debugger profile dialog
HWND             hDlgRplObjView = NULL;		// handle for debugger rpl object viewer
HDC              hWindowDC = NULL;
HPALETTE         hPalette = NULL;
HPALETTE         hOldPalette = NULL;		// old palette of hWindowDC
DWORD            dwTColor = (DWORD) -1;		// transparency color
DWORD            dwTColorTol = 0;			// transparency color tolerance
HRGN             hRgn = NULL;
BOOL             bTitleBar = TRUE;			// title bar visible
HCURSOR          hCursorArrow = NULL;
HCURSOR          hCursorHand = NULL;
UINT             uWaveDevId = WAVE_MAPPER;	// default audio device
DWORD            dwWakeupDelay = 200;		// ON key hold time to switch on calculator
BOOL             bAutoSave = FALSE;
BOOL             bAutoSaveOnExit = TRUE;
BOOL             bSaveDefConfirm = TRUE;	// yes
BOOL             bStartupBackup = FALSE;
BOOL             bAlwaysDisplayLog = TRUE;
BOOL             bLoadObjectWarning = TRUE;
BOOL             bShowTitle = TRUE;			// show main window title bar
BOOL             bShowMenu = TRUE;			// show main window menu bar
BOOL             bAlwaysOnTop = FALSE;		// emulator window always on top
BOOL             bActFollowsMouse = FALSE;	// emulator window activation follows mouse
BOOL             bClientWinMove = FALSE;	// emulator window can be moved over client area
BOOL             bSingleInstance = FALSE;	// multiple emulator instances allowed

VOID SetWindowTitle(LPCTSTR szString)
{
	if (szTitle) free(szTitle);

	_ASSERT(hWnd != NULL);
	if (szString)
	{
		szTitle = DuplicateString(szString);
		SetWindowText(hWnd, szTitle);
	}
	else
	{
		szTitle = NULL;
		SetWindowText(hWnd, szNoTitle);
	}
	return;
}

//################
//#
//#    Window Helper
//#
//################

VOID CreateLocks(VOID)
{
	InitializeCriticalSection(&csGDILock);
	InitializeCriticalSection(&csLcdLock);
	InitializeCriticalSection(&csKeyLock);
	InitializeCriticalSection(&csIOLock);
	InitializeCriticalSection(&csT1Lock);
	InitializeCriticalSection(&csT2Lock);
	InitializeCriticalSection(&csTxdLock);
	InitializeCriticalSection(&csRecvLock);
	InitializeCriticalSection(&csSlowLock);
	InitializeCriticalSection(&csDbgLock);
	return;
}

VOID DeleteLocks(VOID)
{
	DeleteCriticalSection(&csGDILock);
	DeleteCriticalSection(&csLcdLock);
	DeleteCriticalSection(&csKeyLock);
	DeleteCriticalSection(&csIOLock);
	DeleteCriticalSection(&csT1Lock);
	DeleteCriticalSection(&csT2Lock);
	DeleteCriticalSection(&csTxdLock);
	DeleteCriticalSection(&csRecvLock);
	DeleteCriticalSection(&csSlowLock);
	DeleteCriticalSection(&csDbgLock);
	return;
}

//
// redraw main window area prcPaint in client coordinates
//
VOID PaintMainWindow(HDC hPaintDC, CONST RECT *prcPaint)
{
	if (hMainDC != NULL)
	{
		RECT rcMainPaint = *prcPaint;
		rcMainPaint.left   += nBackgroundX;	// coordinates in source bitmap
		rcMainPaint.top    += nBackgroundY;
		rcMainPaint.right  += nBackgroundX;
		rcMainPaint.bottom += nBackgroundY;

		EnterCriticalSection(&csGDILock);	// solving NT GDI problems
		{
			UINT nLines = (Chipset.lcounter == 0) ? 64 : (Chipset.lcounter + 1);

			// redraw background bitmap
			BitBlt(hPaintDC, prcPaint->left, prcPaint->top,
				   prcPaint->right-prcPaint->left, prcPaint->bottom-prcPaint->top,
				   hMainDC, rcMainPaint.left, rcMainPaint.top, SRCCOPY);

			SetWindowOrgEx(hPaintDC, nBackgroundX, nBackgroundY, NULL);

			// redraw main display area
			StretchBlt(hPaintDC, nLcdX, nLcdY,
					   131*nLcdZoom*nGdiXZoom, nLines*nLcdZoom*nGdiYZoom,
					   hLcdDC, Chipset.boffset*nLcdZoom, 0,
					   131*nLcdZoom, nLines*nLcdZoom, SRCCOPY);
			// redraw menu display area
			StretchBlt(hPaintDC, nLcdX, nLcdY+nLines*nLcdZoom*nGdiYZoom,
					   131*nLcdZoom*nGdiXZoom, (64-nLines)*nLcdZoom*nGdiYZoom,
					   hLcdDC, 0, nLines*nLcdZoom,
					   131*nLcdZoom, (64-nLines)*nLcdZoom, SRCCOPY);
			GdiFlush();
		}
		LeaveCriticalSection(&csGDILock);
		UpdateAnnunciators(0x3F);
		RefreshButtons(&rcMainPaint);
	}
	return;
}

//################
//#
//#    Save Helper
//#
//################

//
// UINT SaveChanges(BOOL bAuto);
// Return code :
// IDYES    File successfuly saved
// IDNO     File not saved
// IDCANCEL Cancel command
//
UINT SaveChanges(BOOL bAuto)
{
	UINT uReply;

	if (bDocumentAvail == FALSE) return IDNO;

	if (bAuto)
		uReply = IDYES;
	else
	{
		UINT uStyle = bSaveDefConfirm ? 0 : MB_DEFBUTTON2;
		uReply = YesNoCancelMessage(_T("Do you want to save changes?"),uStyle);
	}

	if (uReply != IDYES) return uReply;

	if (szCurrentFilename[0] == 0)
	{ // Save As...
		if (GetSaveAsFilename())
		{
			if (SaveDocumentAs(szBufferFilename))
			{
				MruAdd(szBufferFilename);
				return IDYES;
			}
			else
				return IDCANCEL;
		}
		return IDNO;
	}

	SaveDocument();
	MruAdd(szCurrentFilename);
	return IDYES;
}



//
// load files into the emulator, used for dropped files
//
LRESULT LoadObjectFiles(UINT nNumFiles, LPCTSTR (*fnFileName)(UINT nIndex, LPVOID pData), LPVOID pData)
{
	UINT  nIndex;
	BOOL  bSuccess = FALSE;

	SuspendDebugger();						// suspend debugger
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	// calculator off, turn on
	if (!(Chipset.IORam[BITOFFSET]&DON))
	{
		// turn on HP
		KeyboardEvent(TRUE,0,0x8000);
		Sleep(dwWakeupDelay);
		KeyboardEvent(FALSE,0,0x8000);
	}

	_ASSERT(nState == SM_RUN);				// emulator must be in RUN state
	if (WaitForSleepState())				// wait for cpu SHUTDN then sleep state
	{
		InfoMessage(_T("The emulator is busy."));
		goto cancel;
	}

	_ASSERT(nState == SM_SLEEP);

	// get each name and load it into the emulator
	for (nIndex = 0;nIndex < nNumFiles;++nIndex)
	{
		// try loading file
		if ((bSuccess = LoadObject(fnFileName(nIndex,pData))) == FALSE)
			break;
	}

	SwitchToState(SM_RUN);					// run state
	while (nState!=nNextState) Sleep(0);
	_ASSERT(nState == SM_RUN);

	if (bSuccess == FALSE)					// data not copied
		goto cancel;

	KeyboardEvent(TRUE,0,0x8000);
	Sleep(dwWakeupDelay);
	KeyboardEvent(FALSE,0,0x8000);
	// wait for sleep mode
	while (Chipset.Shutdn == FALSE) Sleep(0);

cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	ResumeDebugger();
	return 0;
}

//
// WM_INITMENU
//
LRESULT OnInitMenu(HMENU hMenu)
{
	// disable stack loading items on HP38G, HP39/40G
	BOOL bStackEnable = cCurrentRomType!='6' && cCurrentRomType!='A' && cCurrentRomType!='E';
	BOOL bRun         = nState == SM_RUN || nState == SM_SLEEP;

	UINT uStackEnable = (bRun && bStackEnable) ? MF_ENABLED : MF_GRAYED;
	UINT uRun         = bRun                   ? MF_ENABLED : MF_GRAYED;
	UINT uBackup      = bBackup                ? MF_ENABLED : MF_GRAYED;

	EnableMenuItem(hMenu,ID_FILE_NEW,MF_ENABLED);
	EnableMenuItem(hMenu,ID_FILE_OPEN,MF_ENABLED);
	EnableMenuItem(hMenu,ID_FILE_SAVE,(bRun && szCurrentFilename[0]) ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(hMenu,ID_FILE_SAVEAS,uRun);
	EnableMenuItem(hMenu,ID_FILE_CLOSE,uRun);
	EnableMenuItem(hMenu,ID_OBJECT_LOAD,uStackEnable);
	EnableMenuItem(hMenu,ID_OBJECT_SAVE,uStackEnable);
	EnableMenuItem(hMenu,ID_VIEW_COPY,uRun);
	EnableMenuItem(hMenu,ID_STACK_COPY,uStackEnable);
	EnableMenuItem(hMenu,ID_STACK_PASTE,uStackEnable);
	EnableMenuItem(hMenu,ID_VIEW_RESET,uRun);
	EnableMenuItem(hMenu,ID_BACKUP_SAVE,uRun);
	EnableMenuItem(hMenu,ID_BACKUP_RESTORE,uBackup);
	EnableMenuItem(hMenu,ID_BACKUP_DELETE,uBackup);
	EnableMenuItem(hMenu,ID_VIEW_SCRIPT,uRun);
	EnableMenuItem(hMenu,ID_TOOL_DISASM,uRun);
	EnableMenuItem(hMenu,ID_TOOL_DEBUG,(bRun && nDbgState == DBG_OFF) ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(hMenu,ID_TOOL_MACRO_RECORD,(bRun && nMacroState == MACRO_OFF) ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(hMenu,ID_TOOL_MACRO_PLAY,(bRun && nMacroState == MACRO_OFF) ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(hMenu,ID_TOOL_MACRO_STOP,(bRun && nMacroState != MACRO_OFF) ? MF_ENABLED : MF_GRAYED);

	MruUpdateMenu(hMenu);					// update MRU list
	return 0;
}

VOID UpdateWindowBars(VOID)
{
	DWORD dwStyle;
	HMENU hMenu;

	BOOL bUpdate = FALSE;					// no update

	// get current title bar style
	dwStyle = (DWORD) GetWindowLongPtr(hWnd,GWL_STYLE);
	if ((bTitleBar = (bShowTitle || bDocumentAvail == FALSE)))
	{
		// title bar off
		if ((dwStyle & STYLE_TITLE) != STYLE_TITLE)
		{
			SetWindowLongPtr(hWnd,GWL_STYLE,(dwStyle & ~STYLE_NOTITLE) | STYLE_TITLE);
			bUpdate = TRUE;
		}
	}
	else
	{
		// title bar on
		if ((dwStyle & STYLE_NOTITLE) != STYLE_NOTITLE)
		{
			SetWindowLongPtr(hWnd,GWL_STYLE,(dwStyle & ~STYLE_TITLE) | STYLE_NOTITLE);
			bUpdate = TRUE;
		}
	}

	hMenu = GetMenu(hWnd);					// get system menu
	if (bShowMenu || bDocumentAvail == FALSE)
	{
		if (hMenu == NULL)					// menu off
		{
			// restore menu bar
			SetMenu(hWnd,LoadMenu(hApp,MAKEINTRESOURCE(IDR_MENU)));
			bUpdate = TRUE;
		}
	}
	else
	{
		if (hMenu != NULL)					// menu on
		{
			// close menu bar
			SetMenu(hWnd,NULL);
			VERIFY(DestroyMenu(hMenu));
			bUpdate = TRUE;
		}
	}

	if (dwTColor != (DWORD) -1)				// prepare background bitmap with transparency
	{
		if (!bTitleBar && GetMenu(hWnd) == NULL)
		{
			if (hRgn == NULL)
			{
				EnterCriticalSection(&csGDILock); // solving NT GDI problems
				{
					// enable background bitmap transparency
					hRgn = CreateRgnFromBitmap((HBITMAP) GetCurrentObject(hMainDC,OBJ_BITMAP),
											   dwTColor,
											   dwTColorTol);
					if (hRgn != NULL)		// region definition successful
					{
						OffsetRgn(hRgn,-(INT) nBackgroundX,-(INT) nBackgroundY);
						SetWindowRgn(hWnd,hRgn,TRUE);
					}
					else					// region definition failed
					{
						// disable transparency
						dwTColor = (DWORD) -1;
					}
					GdiFlush();
				}
				LeaveCriticalSection(&csGDILock);
			}
		}
		else
		{
			if (hRgn != NULL)				// region active
			{
				EnterCriticalSection(&csGDILock); // solving NT GDI problems
				{
					// disable background bitmap transparency
					SetWindowRgn(hWnd,NULL,TRUE);
					hRgn = NULL;

					GdiFlush();
				}
				LeaveCriticalSection(&csGDILock);
			}
		}
	}

	if (bUpdate)							// changed state of title or menu bar
	{
		ResizeWindow();						// resize & redraw window
	}
	return;
}

//
// WM_COMMAND
//
LRESULT OnCommand(WPARAM wParam)
{
	switch (LOWORD(wParam))
	{
	case ID_FILE_NEW:            return OnFileNew();
	case ID_FILE_OPEN:           return OnFileOpen();
	case ID_FILE_SAVE:           return OnFileSave();
	case ID_FILE_SAVEAS:         return OnFileSaveAs();
	case ID_FILE_CLOSE:          return OnFileClose();
	case ID_FILE_EXIT:           return OnFileExit();
	case ID_STACK_COPY:          return OnStackCopy();
	case ID_STACK_PASTE:         return OnStackPaste();
	case ID_VIEW_COPY:           return OnViewCopy();
	case ID_VIEW_RESET:          return OnViewReset();
	case ID_VIEW_SETTINGS:       return OnViewSettings();
	case ID_VIEW_SCRIPT:         return OnViewScript();
	case ID_BACKUP_SAVE:         return OnBackupSave();
	case ID_BACKUP_RESTORE:      return OnBackupRestore();
	case ID_BACKUP_DELETE:       return OnBackupDelete();
	case ID_OBJECT_LOAD:         return OnObjectLoad();
	case ID_OBJECT_SAVE:         return OnObjectSave();
	case ID_TOOL_DISASM:         return OnToolDisasm();
	case ID_TOOL_DEBUG:          return OnToolDebug();
	case ID_TOOL_MACRO_RECORD:   return OnToolMacroNew();
	case ID_TOOL_MACRO_PLAY:     return OnToolMacroPlay();
	case ID_TOOL_MACRO_STOP:     return OnToolMacroStop();
	case ID_TOOL_MACRO_SETTINGS: return OnToolMacroSettings();
	case ID_HELP_TOPICS:         return OnTopics();
	case ID_ABOUT:               return OnAbout();
	}
	// check if command ID belongs to MRU file area
	if (   (LOWORD(wParam) >= ID_FILE_MRU_FILE1)
		&& (LOWORD(wParam) <  ID_FILE_MRU_FILE1 + MruEntries()))
		return OnFileMruOpen(LOWORD(wParam));
	return 0;
}

//
// ID_ABOUT
//
static INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hDlg,IDC_VERSION,_T("Emu48/SDL ")_T(EMU48SDL_VERSION));
		SetDlgItemText(hDlg,IDC_LICENSE,szLicence);
		return TRUE;
	case WM_COMMAND:
		wParam = LOWORD(wParam);
		if ((wParam==IDOK)||(wParam==IDCANCEL))
		{
			EndDialog(hDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
	UNREFERENCED_PARAMETER(lParam);
}

LRESULT OnAbout(VOID)
{
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_ABOUT), hWnd, (DLGPROC)About) == -1)
		AbortMessage(_T("About Dialog Box Creation Error!"));
	return 0;
}

//
// ID_FILE_NEW
//
LRESULT OnFileNew(VOID)
{
	if (bDocumentAvail)
	{
		SwitchToState(SM_INVALID);
		if (IDCANCEL == SaveChanges(bAutoSave))
			goto cancel;
	}
	if (NewDocument()) SetWindowTitle(_T("Untitled"));
cancel:
	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_FILE_OPEN
//
LRESULT OnFileOpen(VOID)
{
	if (bDocumentAvail)
	{
		SwitchToState(SM_INVALID);
		if (IDCANCEL == SaveChanges(bAutoSave))
			goto cancel;
	}
	if (GetOpenFilename())
	{
		if (OpenDocument(szBufferFilename))
			MruAdd(szBufferFilename);
	}
cancel:
	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_FILE_MRU_FILE1
//
LRESULT OnFileMruOpen(UINT wID)
{
	TCHAR szFilename[MAX_PATH];

	wID -= ID_FILE_MRU_FILE1;				// zero based MRU index

	// full filename from MRU list
	MruFilename(wID,szFilename,ARRAYSIZEOF(szFilename));
	if (*szFilename == 0) return 0;			// MRU slot not filled

	if (bDocumentAvail)
	{
		SwitchToState(SM_INVALID);
		// saving may change MRU index and destroy lpszFilename pointer content
		if (IDCANCEL == SaveChanges(bAutoSave))
			goto cancel;
	}
	if (!OpenDocument(szFilename))			// document loading failed
	{
		wID = MruID(szFilename);			// get actual MRU ID after saving
		if (wID != (UINT) -1)				// entry still in MRU list
		{
			MruRemove(wID);					// entry not valid any more
		}
	}
	else
	{
		MruAdd(szFilename);					// add entry to top of MRU list
	}
cancel:
	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_FILE_SAVE
//
LRESULT OnFileSave(VOID)
{
	if (bDocumentAvail)
	{
		SwitchToState(SM_INVALID);
		SaveChanges(TRUE);
		SwitchToState(SM_RUN);
	}
	return 0;
}

//
// ID_FILE_SAVEAS
//
LRESULT OnFileSaveAs(VOID)
{
	if (bDocumentAvail)
	{
		SwitchToState(SM_INVALID);
		if (GetSaveAsFilename())
		{
			if (SaveDocumentAs(szBufferFilename))
				MruAdd(szCurrentFilename);
		}
		SwitchToState(SM_RUN);
	}
	return 0;
}

//
// ID_FILE_CLOSE
//
LRESULT OnFileClose(VOID)
{
	if (bDocumentAvail)
	{
		SwitchToState(SM_INVALID);
		if (SaveChanges(bAutoSave) != IDCANCEL)
		{
			ResetDocument();
			SetWindowTitle(NULL);
		}
		else
		{
			SwitchToState(SM_RUN);
		}
	}
	return 0;
}

//
// ID_FILE_EXIT
//
// WM_SYS_CLOSE
//
LRESULT OnFileExit(VOID)
{
	SwitchToState(SM_INVALID);				// hold emulation thread
	if (SaveChanges(bAutoSaveOnExit) == IDCANCEL)
	{
		SwitchToState(SM_RUN);				// on cancel restart emulation thread
		return 0;
	}
	DestroyWindow(hWnd);
	return 0;
}

//
// ID_VIEW_RESET
//
LRESULT OnViewReset(VOID)
{
	if (nState != SM_RUN) return 0;
	if (YesNoMessage(_T("Are you sure you want to press the Reset Button?"))==IDYES)
	{
		SwitchToState(SM_SLEEP);
		CpuReset();							// register setting after Cpu Reset
		SwitchToState(SM_RUN);
	}
	return 0;
}

//
// ID_VIEW_SCRIPT
//
LRESULT OnViewScript(VOID)
{
	TCHAR szKmlFile[MAX_PATH];
	BOOL  bKMLChanged,bSucc;

	BYTE cType = cCurrentRomType;
	if (nState != SM_RUN)
	{
		InfoMessage(_T("You cannot change the KML script when Emu48 is not running.\n")
					_T("Use the File,New menu item to create a new calculator."));
		return 0;
	}
	SwitchToState(SM_INVALID);

	// make a copy of the current KML script file name
	_ASSERT(sizeof(szKmlFile) == sizeof(szCurrentKml));
	lstrcpyn(szKmlFile,szCurrentKml,ARRAYSIZEOF(szKmlFile));

	bKMLChanged = FALSE;					// KML script not changed
	bSucc = TRUE;							// KML script successful loaded

	do
	{
		if (!DisplayChooseKml(cType))		// quit with Cancel
		{
			if (!bKMLChanged)				// KML script not changed
				break;						// exit loop with current loaded KML script

			// restore KML script file name
			lstrcpyn(szCurrentKml,szKmlFile,ARRAYSIZEOF(szCurrentKml));

			// try to restore old KML script
			if ((bSucc = InitKML(szCurrentKml,FALSE)))
				break;						// exit loop with success

			// restoring failed, save document
			if (IDCANCEL != SaveChanges(bAutoSave))
				break;						// exit loop with no success

			_ASSERT(bSucc == FALSE);		// for continuing loop
		}
		else								// quit with Ok
		{
			bKMLChanged = TRUE;				// KML script changed
			bSucc = InitKML(szCurrentKml,FALSE);
		}
	}
	while (!bSucc);							// retry if KML script is invalid

	if (bSucc)
	{
		if (Chipset.wRomCrc != wRomCrc)		// ROM changed
		{
			CpuReset();
			Chipset.Shutdn = FALSE;			// automatic restart

			Chipset.wRomCrc = wRomCrc;		// update current ROM fingerprint
		}
		if (pbyRom) SwitchToState(SM_RUN);	// continue emulation
	}
	else
	{
		ResetDocument();					// close document
		SetWindowTitle(NULL);
	}
	return 0;
}

//
// ID_BACKUP_SAVE
//
LRESULT OnBackupSave(VOID)
{
	UINT nOldState;
	if (pbyRom == NULL) return 0;
	nOldState = SwitchToState(SM_INVALID);
	SaveBackup();
	SwitchToState(nOldState);
	return 0;
}

//
// ID_BACKUP_RESTORE
//
LRESULT OnBackupRestore(VOID)
{
	SwitchToState(SM_INVALID);
	RestoreBackup();
	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_BACKUP_DELETE
//
LRESULT OnBackupDelete(VOID)
{
	ResetBackup();
	return 0;
}

//
// ID_OBJECT_LOAD
//
LRESULT OnObjectLoad(VOID)
{
	SuspendDebugger();						// suspend debugger
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	// calculator off, turn on
	if (!(Chipset.IORam[BITOFFSET]&DON))
	{
		KeyboardEvent(TRUE,0,0x8000);
		Sleep(dwWakeupDelay);
		KeyboardEvent(FALSE,0,0x8000);

		// wait for sleep mode
		while (Chipset.Shutdn == FALSE) Sleep(0);
	}

	if (nState != SM_RUN)
	{
		InfoMessage(_T("The emulator must be running to load an object."));
		goto cancel;
	}

	if (WaitForSleepState())				// wait for cpu SHUTDN then sleep state
	{
		InfoMessage(_T("The emulator is busy."));
		goto cancel;
	}

	_ASSERT(nState == SM_SLEEP);

	if (bLoadObjectWarning)
	{
		UINT uReply = YesNoCancelMessage(
			_T("Warning: Trying to load an object while the emulator is busy\n")
			_T("will certainly result in a memory lost. Before loading an object\n")
			_T("you should be sure that the calculator is in idle state.\n")
			_T("Do you want to see this warning next time you try to load an object?"),0);
		switch (uReply)
		{
		case IDYES:
			break;
		case IDNO:
			bLoadObjectWarning = FALSE;
			break;
		case IDCANCEL:
			SwitchToState(SM_RUN);
			goto cancel;
		}
	}

	if (!GetLoadObjectFilename(_T(HP_FILTER),_T("HP")))
	{
		SwitchToState(SM_RUN);
		goto cancel;
	}

	if (!LoadObject(szBufferFilename))
	{
		SwitchToState(SM_RUN);
		goto cancel;
	}

	SwitchToState(SM_RUN);					// run state
	while (nState!=nNextState) Sleep(0);
	_ASSERT(nState == SM_RUN);
	KeyboardEvent(TRUE,0,0x8000);
	Sleep(dwWakeupDelay);
	KeyboardEvent(FALSE,0,0x8000);
	while (Chipset.Shutdn == FALSE) Sleep(0);

cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	ResumeDebugger();
	return 0;
}

//
// ID_OBJECT_SAVE
//
LRESULT OnObjectSave(VOID)
{
	if (nState != SM_RUN)
	{
		InfoMessage(_T("The emulator must be running to save an object."));
		return 0;
	}

	if (WaitForSleepState())				// wait for cpu SHUTDN then sleep state
	{
		InfoMessage(_T("The emulator is busy."));
		return 0;
	}

	_ASSERT(nState == SM_SLEEP);

	if (GetSaveObjectFilename(_T(HP_FILTER),_T("HP")))
	{
		SaveObject(szBufferFilename);
	}

	SwitchToState(SM_RUN);
	return 0;
}


//################
//#
//#    Settings
//#
//################

// get R/W state of file
static BOOL IsFileWriteable(LPCTSTR szFilename)
{
	DWORD dwFileAtt;

	BOOL bWriteable = FALSE;

	SetCurrentDirectory(szEmuDirectory);
	dwFileAtt = GetFileAttributes(szFilename);
	if (dwFileAtt != 0xFFFFFFFF)
		bWriteable = ((dwFileAtt & FILE_ATTRIBUTE_READONLY) == 0);
	SetCurrentDirectory(szCurrentDirectory);
	return bWriteable;
}

// set listfield for serial combo boxes
static VOID SetCommList(HWND hDlg,LPCTSTR szWireSetting,LPCTSTR szIrSetting)
{
	WPARAM wSelectWire,wSelectIr;
	HKEY   hKey;

	wSelectWire = wSelectIr = 0;			// set selections to disabled
	SendDlgItemMessage(hDlg,IDC_WIRE,CB_ADDSTRING,0,(LPARAM) _T(NO_SERIAL));
	SendDlgItemMessage(hDlg,IDC_IR  ,CB_ADDSTRING,0,(LPARAM) _T(NO_SERIAL));

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					 _T("Hardware\\DeviceMap\\SerialComm"),
					 0,
					 KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
					 &hKey) == ERROR_SUCCESS)
	{
		HANDLE hComm;
		TCHAR  szBuffer[256],cKey[256],cData[256];
		DWORD  dwKeySize,dwDataSize;
		WPARAM wSelIndexWire,wSelIndexIr;
		BOOL   bAddWire,bAddIr;
		DWORD  dwType,dwEnumVal;
		LONG   lRet;

		wSelIndexWire = wSelIndexIr = 1;	// preset selector

		dwEnumVal = 0;
		do
		{
			dwKeySize = ARRAYSIZEOF(cKey);	// init return buffer sizes
			dwDataSize = sizeof(cData);

			lRet = RegEnumValue(hKey,dwEnumVal++,
								cKey,&dwKeySize,
								NULL,&dwType,
								(LPBYTE) cData,&dwDataSize);

			if (lRet == ERROR_SUCCESS && dwType == REG_SZ)
			{
				wsprintf(szBuffer,_T("\\\\.\\%s"),cData);
				if ((bAddWire = (lstrcmp(&szBuffer[4],szWireSetting) == 0)))
					wSelectWire = wSelIndexWire;
				if ((bAddIr = (lstrcmp(&szBuffer[4],szIrSetting) == 0)))
					wSelectIr = wSelIndexIr;

				// test if COM port is valid
				hComm = CreateFile(szBuffer,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
				if (hComm != INVALID_HANDLE_VALUE)
				{
					VERIFY(CloseHandle(hComm));
					bAddWire = bAddIr = TRUE;
				}

				if (bAddWire)				// add item to wire combobox
				{
					SendDlgItemMessage(hDlg,IDC_WIRE,CB_ADDSTRING,0,(LPARAM) &szBuffer[4]);
					++wSelIndexWire;
				}
				if (bAddIr)					// add item to ir combobox
				{
					SendDlgItemMessage(hDlg,IDC_IR,CB_ADDSTRING,0,(LPARAM) &szBuffer[4]);
					++wSelIndexIr;
				}
			}
		}
		while (lRet == ERROR_SUCCESS);
		_ASSERT(lRet == ERROR_NO_MORE_ITEMS);
		RegCloseKey(hKey);
	}

	// add a setting not found, e.g. a pseudo terminal
	if (wSelectWire == 0 && lstrcmp(szWireSetting,_T(NO_SERIAL)) != 0)
		wSelectWire = (WPARAM) SendDlgItemMessage(hDlg,IDC_WIRE,CB_ADDSTRING,0,(LPARAM) szWireSetting);
	if (wSelectIr == 0 && lstrcmp(szIrSetting,_T(NO_SERIAL)) != 0)
		wSelectIr = (WPARAM) SendDlgItemMessage(hDlg,IDC_IR,CB_ADDSTRING,0,(LPARAM) szIrSetting);

	// set cursors
	SendDlgItemMessage(hDlg,IDC_WIRE,CB_SETCURSEL,wSelectWire,0L);
	SendDlgItemMessage(hDlg,IDC_IR  ,CB_SETCURSEL,wSelectIr  ,0L);
	return;
}

static INT_PTR CALLBACK SettingsGeneralProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hWndInsertAfter;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		// init speed checkbox
		CheckDlgButton(hDlg,IDC_REALSPEED,bRealSpeed);
		CheckDlgButton(hDlg,IDC_GRAYSCALE,bGrayscale);
		CheckDlgButton(hDlg,IDC_SHOWTITLE,bShowTitle);
		CheckDlgButton(hDlg,IDC_SHOWMENU,bShowMenu);
		CheckDlgButton(hDlg,IDC_ALWAYSONTOP,bAlwaysOnTop);
		#if defined _USRDLL					// DLL version
			CheckDlgButton(hDlg,IDC_SINGLEINSTANCE,FALSE);
			EnableWindow(GetDlgItem(hDlg,IDC_SINGLEINSTANCE),FALSE);
		#else
			CheckDlgButton(hDlg,IDC_SINGLEINSTANCE,bSingleInstance);
		#endif
		CheckDlgButton(hDlg,IDC_AUTOSAVE,bAutoSave);
		CheckDlgButton(hDlg,IDC_AUTOSAVEONEXIT,bAutoSaveOnExit);
		CheckDlgButton(hDlg,IDC_OBJECTLOADWARNING,bLoadObjectWarning);
		CheckDlgButton(hDlg,IDC_ALWAYSDISPLOG,bAlwaysDisplayLog);

		// set disassembler mode
		CheckDlgButton(hDlg,(disassembler_mode == HP_MNEMONICS) ? IDC_DISASM_HP : IDC_DISASM_CLASS,BST_CHECKED);
		return TRUE;
	case WM_NOTIFY:
		switch (((LPNMHDR) lParam)->code)
		{
		case PSN_KILLACTIVE:
			// get speed checkbox value
			bRealSpeed = IsDlgButtonChecked(hDlg,IDC_REALSPEED);
			bShowTitle = IsDlgButtonChecked(hDlg,IDC_SHOWTITLE);
			bShowMenu = IsDlgButtonChecked(hDlg,IDC_SHOWMENU);
			bAlwaysOnTop = IsDlgButtonChecked(hDlg,IDC_ALWAYSONTOP);
			bSingleInstance = IsDlgButtonChecked(hDlg,IDC_SINGLEINSTANCE);
			bAutoSave = IsDlgButtonChecked(hDlg,IDC_AUTOSAVE);
			bAutoSaveOnExit = IsDlgButtonChecked(hDlg,IDC_AUTOSAVEONEXIT);
			bLoadObjectWarning = IsDlgButtonChecked(hDlg,IDC_OBJECTLOADWARNING);
			bAlwaysDisplayLog = IsDlgButtonChecked(hDlg,IDC_ALWAYSDISPLOG);
			SetSpeed(bRealSpeed);			// set speed

			// LCD grayscale checkbox has been changed
			if (bGrayscale != (BOOL) IsDlgButtonChecked(hDlg,IDC_GRAYSCALE))
			{
				UINT nOldState = SwitchToState(SM_INVALID);
				SetLcdMode(!bGrayscale);	// set new display mode
				SwitchToState(nOldState);
			}

			// set disassembler mode
			disassembler_mode = IsDlgButtonChecked(hDlg,IDC_DISASM_HP) ? HP_MNEMONICS : CLASS_MNEMONICS;

			// bAlwaysOnTop maybe changed, so set new window Z order
			hWndInsertAfter = bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
			SetWindowPos(hWnd,hWndInsertAfter,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
			if (hDlgDebug != NULL)
			{
				SetWindowPos(GetLastActivePopup(hDlgDebug),hWndInsertAfter,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE);
			}
			InvalidateRect(hWnd,NULL,TRUE);
			return TRUE;
		}
		break;
	}
	return FALSE;
	UNREFERENCED_PARAMETER(wParam);
}

static INT_PTR CALLBACK SettingsMemoryProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPCTSTR szActPort2Filename = _T("");

	BOOL bPort2CfgChange = FALSE;
	BOOL bPort2AttChange = FALSE;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		// HP48SX/GX
		if (cCurrentRomType=='S' || cCurrentRomType=='G' || cCurrentRomType==0)
		{
			// init port1 enable checkbox
			CheckDlgButton(hDlg,IDC_PORT1EN,(Chipset.cards_status & PORT1_PRESENT) != 0);
			CheckDlgButton(hDlg,IDC_PORT1WR,(Chipset.cards_status & PORT1_WRITE) != 0);

			if (nArgc < 3)					// port2 filename from Emu48.ini file
			{
				szActPort2Filename = szPort2Filename;
			}
			else							// port2 filename given from command line
			{
				szActPort2Filename = ppArgv[2];
				EnableWindow(GetDlgItem(hDlg,IDC_PORT2),FALSE);
				EnableWindow(GetDlgItem(hDlg,IDC_PORT2LOAD),FALSE);
			}

			// init port2 shared and writeable checkbox and set port2 filename
			CheckDlgButton(hDlg,IDC_PORT2ISSHARED,bPort2IsShared);
			CheckDlgButton(hDlg,IDC_PORT2WR,IsFileWriteable(szActPort2Filename));
			SetDlgItemText(hDlg,IDC_PORT2,szActPort2Filename);
			if (nState == SM_INVALID)		// Invalid State
			{
				EnableWindow(GetDlgItem(hDlg,IDC_PORT1EN),FALSE);
				EnableWindow(GetDlgItem(hDlg,IDC_PORT1WR),FALSE);
			}
		}
		else								// HP38G/HP39G/HP40G/HP49G
		{
			EnableWindow(GetDlgItem(hDlg,IDC_PORT1EN),FALSE);
			EnableWindow(GetDlgItem(hDlg,IDC_PORT1WR),FALSE);
			EnableWindow(GetDlgItem(hDlg,IDC_PORT2ISSHARED),FALSE);
			EnableWindow(GetDlgItem(hDlg,IDC_PORT2WR),FALSE);
			EnableWindow(GetDlgItem(hDlg,IDC_PORT2),FALSE);
			EnableWindow(GetDlgItem(hDlg,IDC_PORT2LOAD),FALSE);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_PORT2LOAD:
			if (GetLoadObjectFilename(_T(BIN_FILTER),_T("BIN")))
			{
				TCHAR  szFilename[MAX_PATH];
				LPTSTR lpFilePart;

				// check if file path and Emu48 directory path is identical
				if (GetFullPathName(szBufferFilename,ARRAYSIZEOF(szFilename),szFilename,&lpFilePart))
				{
					*(lpFilePart-1) = 0;	// devide path and name

					// name is in the Emu48 directory -> use only name
					if (lstrcmpi(szEmuDirectory,szFilename) == 0)
						lstrcpy(szBufferFilename,lpFilePart);
				}
				SetDlgItemText(hDlg,IDC_PORT2,szBufferFilename);

				// adjust R/W checkbox
				CheckDlgButton(hDlg,IDC_PORT2WR,IsFileWriteable(szBufferFilename));
			}
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR) lParam)->code)
		{
		case PSN_KILLACTIVE:
			if (Chipset.Port1Size && cCurrentRomType!='X')
			{
				UINT nOldState = SwitchToState(SM_SLEEP);
				// save old card status
				BYTE byCardsStatus = Chipset.cards_status;

				// port1 disabled?
				Chipset.cards_status &= ~(PORT1_PRESENT | PORT1_WRITE);
				if (IsDlgButtonChecked(hDlg, IDC_PORT1EN))
				{
					Chipset.cards_status |= PORT1_PRESENT;
					if (IsDlgButtonChecked(hDlg, IDC_PORT1WR))
						Chipset.cards_status |= PORT1_WRITE;
				}

				// changed card status in slot1?
				if (   ((byCardsStatus ^ Chipset.cards_status) & (PORT1_PRESENT | PORT1_WRITE)) != 0
					&& (Chipset.IORam[CARDCTL] & ECDT) != 0 && (Chipset.IORam[TIMER2_CTRL] & RUN) != 0
				   )
				{
					Chipset.HST |= MP;		// set Module Pulled
					IOBit(SRQ2,NINT,FALSE);	// set NINT to low
					Chipset.SoftInt = TRUE;	// set interrupt
					bInterrupt = TRUE;
				}
				SwitchToState(nOldState);
			}
			// HP48SX/GX port2 change settings detection
			if (cCurrentRomType=='S' || cCurrentRomType=='G' || cCurrentRomType==0)
			{
				TCHAR szFilename[MAX_PATH];
				BOOL  bOldPort2IsShared = bPort2IsShared;

				szActPort2Filename = (nArgc < 3) ? szPort2Filename : ppArgv[2];

				// shared port
				bPort2IsShared = IsDlgButtonChecked(hDlg,IDC_PORT2ISSHARED);
				if (bPort2IsShared != bOldPort2IsShared)
				{
					bPort2CfgChange = TRUE;	// slot2 configuration changed
				}

				if (nArgc < 3)				// port2 filename from Emu48.ini file
				{
					// get current filename and notify difference
					GetDlgItemText(hDlg,IDC_PORT2,szFilename,ARRAYSIZEOF(szFilename));
					if (lstrcmp(szPort2Filename,szFilename) != 0)
					{
						lstrcpyn(szPort2Filename,szFilename,ARRAYSIZEOF(szPort2Filename));
						bPort2CfgChange = TRUE;	// slot2 configuration changed
					}
				}

				// R/W port
				if (   *szActPort2Filename != 0
					&& (BOOL) IsDlgButtonChecked(hDlg,IDC_PORT2WR) != IsFileWriteable(szActPort2Filename))
				{
					bPort2AttChange = TRUE;	// slot2 file R/W attribute changed
					bPort2CfgChange = TRUE;	// slot2 configuration changed
				}
			}

			if (bPort2CfgChange)			// slot2 configuration changed
			{
				UINT nOldState = SwitchToState(SM_INVALID);

				UnmapPort2();				// unmap port2

				if (bPort2AttChange)		// slot2 R/W mode changed
				{
					DWORD dwFileAtt;

					SetCurrentDirectory(szEmuDirectory);
					dwFileAtt = GetFileAttributes(szActPort2Filename);
					if (dwFileAtt != 0xFFFFFFFF)
					{
						if (IsDlgButtonChecked(hDlg,IDC_PORT2WR))
							dwFileAtt &= ~FILE_ATTRIBUTE_READONLY;
						else
							dwFileAtt |= FILE_ATTRIBUTE_READONLY;

						SetFileAttributes(szActPort2Filename,dwFileAtt);
					}
					SetCurrentDirectory(szCurrentDirectory);
				}

				if (cCurrentRomType)		// ROM defined
				{
					MapPort2(szActPort2Filename);

					// port2 changed and card detection enabled
					if (   (bPort2AttChange || Chipset.wPort2Crc != wPort2Crc)
						&& (Chipset.IORam[CARDCTL] & ECDT) != 0 && (Chipset.IORam[TIMER2_CTRL] & RUN) != 0
					   )
					{
						Chipset.HST |= MP;		// set Module Pulled
						IOBit(SRQ2,NINT,FALSE);	// set NINT to low
						Chipset.SoftInt = TRUE;	// set interrupt
						bInterrupt = TRUE;
					}
					// save fingerprint of port2
					Chipset.wPort2Crc = wPort2Crc;
				}
				SwitchToState(nOldState);
			}
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static INT_PTR CALLBACK SettingsPeripheralProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR cPort[8];
	LONG  i;
	UINT  uDevId;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		// set sound slider
		SendDlgItemMessage(hDlg,IDC_SOUND_SLIDER,TBM_SETRANGE,FALSE,MAKELONG(0,255));
		SendDlgItemMessage(hDlg,IDC_SOUND_SLIDER,TBM_SETTICFREQ,256/8,0);
		SendDlgItemMessage(hDlg,IDC_SOUND_SLIDER,TBM_SETPOS,TRUE,dwWaveVol);

		// set sound device
		SetSoundDeviceList(GetDlgItem(hDlg,IDC_SOUND_DEVICE),uWaveDevId);

		// UDP infrared printer settings
		SetDlgItemText(hDlg,IDC_IR_ADDR,szUdpServer);
		wsprintf(cPort,_T("%u"),wUdpPort);
		SetDlgItemText(hDlg,IDC_IR_PORT,cPort);

		// set combobox parameter
		SetCommList(hDlg,szSerialWire,szSerialIr);
		if (CommIsOpen())					// disable when port open
		{
			EnableWindow(GetDlgItem(hDlg,IDC_WIRE),FALSE);
			EnableWindow(GetDlgItem(hDlg,IDC_IR),FALSE);
		}

		if (cCurrentRomType=='X')			// HP49G
		{
			SendDlgItemMessage(hDlg,IDC_IR,CB_RESETCONTENT,0,0);
			EnableWindow(GetDlgItem(hDlg,IDC_IR),FALSE);
		}
		return TRUE;
	case WM_NOTIFY:
		switch (((LPNMHDR) lParam)->code)
		{
		case PSN_KILLACTIVE:
			// set sound data
			dwWaveVol = (DWORD) SendDlgItemMessage(hDlg,IDC_SOUND_SLIDER,TBM_GETPOS,0,0);
			i = (LONG) SendDlgItemMessage(hDlg,IDC_SOUND_DEVICE,CB_GETCURSEL,0,0);
			uDevId = (UINT) SendDlgItemMessage(hDlg,IDC_SOUND_DEVICE,CB_GETITEMDATA,i,0);
			if (uWaveDevId != uDevId)		// sound device id changed
			{
				UINT nActState;

				uWaveDevId = uDevId;		// set new sound device id

				nActState = SwitchToState(SM_SLEEP);

				// restart sound engine with new device id
				SoundClose();				// close waveform-audio output device
				SoundOpen(uWaveDevId);		// open waveform-audio output device

				SwitchToState(nActState);
			}
			// UDP infrared printer settings
			GetDlgItemText(hDlg,IDC_IR_ADDR,szUdpServer,ARRAYSIZEOF(szUdpServer));
			GetDlgItemText(hDlg,IDC_IR_PORT,cPort,ARRAYSIZEOF(cPort));
			wUdpPort = (WORD) _ttoi(cPort);
			ResetUdp();						// invalidate saved UDP address
			// set combobox parameter
			GetDlgItemText(hDlg,IDC_WIRE,szSerialWire,ARRAYSIZEOF(szSerialWire));
			if (*szSerialWire == 0)			// empty edit field
				lstrcpy(szSerialWire,_T(NO_SERIAL));
			if (cCurrentRomType!='X')		// HP49G Ir port is not connected
			{
				GetDlgItemText(hDlg,IDC_IR,szSerialIr,ARRAYSIZEOF(szSerialIr));
				if (*szSerialIr == 0)		// empty edit field
					lstrcpy(szSerialIr,_T(NO_SERIAL));
			}
			return TRUE;
		}
		break;
	}
	return FALSE;
	UNREFERENCED_PARAMETER(wParam);
}

//
// ID_VIEW_SETTINGS
//
static INT_PTR CALLBACK PropSheetProc(HWND hwndPropSheet, UINT uMsg, LPARAM lParam)
{
	switch(uMsg)
	{
	// called before the dialog is created, hwndPropSheet = NULL, lParam points to dialog resource
	case PSCB_PRECREATE:
		{
			LPDLGTEMPLATE lpTemplate = (LPDLGTEMPLATE) lParam;
			if(!(lpTemplate->style & WS_SYSMENU))
			{
				lpTemplate->style |= WS_SYSMENU;
			}
		}
		break;

	// called after the dialog is created
	case PSCB_INITIALIZED:
		break;
	}
	return 0;
	UNREFERENCED_PARAMETER(hwndPropSheet);
}

LRESULT OnViewSettings(VOID)
{
	PROPSHEETPAGE psp[3];
	PROPSHEETHEADER psh;

	// not in nState = SM_INVALID or port2 file must be closed from document
	_ASSERT(nState != SM_INVALID || pbyPort2 == NULL);

	psp[0].dwSize = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags = PSP_DEFAULT;
	psp[0].hInstance = hApp;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_SET_GENERAL);
	psp[0].hIcon = NULL;
	psp[0].pszTitle = NULL;
	psp[0].pfnDlgProc = SettingsGeneralProc;
	psp[0].lParam = 0;
	psp[0].pfnCallback = NULL;

	psp[1].dwSize = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags = PSP_DEFAULT;
	psp[1].hInstance = hApp;
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_SET_MEMORY);
	psp[1].hIcon = NULL;
	psp[1].pszTitle = NULL;
	psp[1].pfnDlgProc = SettingsMemoryProc;
	psp[1].lParam = 0;
	psp[1].pfnCallback = NULL;

	psp[2].dwSize = sizeof(PROPSHEETPAGE);
	psp[2].dwFlags = PSP_DEFAULT;
	psp[2].hInstance = hApp;
	psp[2].pszTemplate = MAKEINTRESOURCE(IDD_SET_PERIPHERAL);
	psp[2].hIcon = NULL;
	psp[2].pszTitle = NULL;
	psp[2].pfnDlgProc = SettingsPeripheralProc;
	psp[2].lParam = 0;
	psp[2].pfnCallback = NULL;

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_NOAPPLYNOW;
	psh.hwndParent = hWnd;
	psh.hInstance = hApp;
	psh.hIcon = NULL;
	psh.pszCaption = _T("Settings");
	psh.nPages = ARRAYSIZEOF(psp);
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE) &psp;
	psh.pfnCallback = (PFNPROPSHEETCALLBACK) PropSheetProc;

	if (PropertySheet(&psh) == -1)
		AbortMessage(_T("Settings Property Sheet Creation Error!"));

	WriteSettings();
	return 0;
}


//################
//#
//#    Clipboard Tool
//#
//################

VOID CopyItemsToClipboard(HWND hWnd)		// save selected Listbox Items to Clipboard
{
	LONG  i;
	LPINT lpnCount;

	// get number of selections
	if ((i = (LONG) SendMessage(hWnd,LB_GETSELCOUNT,0,0)) == 0)
		return;								// no items selected

	if ((lpnCount = (LPINT) malloc(i * sizeof(INT))) != NULL)
	{
		LPTSTR lpszData;
		HANDLE hClipObj;
		LONG j,lMem = 0;

		// get indexes of selected items
		i = (LONG) SendMessage(hWnd,LB_GETSELITEMS,i,(LPARAM) lpnCount);
		for (j = 0;j < i;++j)				// scan all selected items
		{
			// calculate total amount of characters
			lMem += (LONG) SendMessage(hWnd,LB_GETTEXTLEN,lpnCount[j],0) + 2;
		}
		// allocate clipboard data
		if ((hClipObj = GlobalAlloc(GMEM_MOVEABLE,(lMem + 1) * sizeof(*lpszData))) != NULL)
		{
			if ((lpszData = (LPTSTR) GlobalLock(hClipObj)))
			{
				for (j = 0;j < i;++j)		// scan all selected items
				{
					lpszData += SendMessage(hWnd,LB_GETTEXT,lpnCount[j],(LPARAM) lpszData);
					*lpszData++ = _T('\r');
					*lpszData++ = _T('\n');
				}
				*lpszData = 0;				// set EOS
				GlobalUnlock(hClipObj);		// unlock memory
			}

			if (OpenClipboard(hWnd))
			{
				if (EmptyClipboard())
					#if defined _UNICODE
						SetClipboardData(CF_UNICODETEXT,hClipObj);
					#else
						SetClipboardData(CF_TEXT,hClipObj);
					#endif
				else
					GlobalFree(hClipObj);
				CloseClipboard();
			}
			else							// clipboard open failed
			{
				GlobalFree(hClipObj);
			}
		}
		free(lpnCount);						// free item table
	}
	return;
}

//
// ID_VIEW_COPY
//
LRESULT OnViewCopy(VOID)
{
	if (OpenClipboard(hWnd))
	{
		if (EmptyClipboard())
		{
			// DIB bitmap
			#define WIDTHBYTES(bits) (((bits) + 31) / 32 * 4)
			#define PALVERSION       0x300

			BITMAP bm;
			LPBITMAPINFOHEADER lpbi;
			PLOGPALETTE ppal;
			HBITMAP hBmp;
			HDC hBmpDC;
			HANDLE hClipObj;
			WORD wBits;
			DWORD dwLen, dwSizeImage;

			_ASSERT(nLcdZoom >= 1 && nLcdZoom <= 4);
			hBmp = CreateCompatibleBitmap(hLcdDC,131*nLcdZoom*nGdiXZoom,64*nLcdZoom*nGdiYZoom);
			hBmpDC = CreateCompatibleDC(hLcdDC);
			hBmp = (HBITMAP) SelectObject(hBmpDC,hBmp);
			EnterCriticalSection(&csGDILock); // solving NT GDI problems
			{
				UINT nLines = (Chipset.lcounter == 0) ? 64 : (Chipset.lcounter + 1);

				// copy main display area
				StretchBlt(hBmpDC, 0, 0,
						   131*nLcdZoom*nGdiXZoom, nLines*nLcdZoom*nGdiYZoom,
						   hLcdDC, Chipset.boffset*nLcdZoom, 0,
						   131*nLcdZoom, nLines*nLcdZoom, SRCCOPY);
				// copy menu display area
				StretchBlt(hBmpDC, 0, nLines*nLcdZoom*nGdiYZoom,
						   131*nLcdZoom*nGdiXZoom, (64-nLines)*nLcdZoom*nGdiYZoom,
						   hLcdDC, 0, nLines*nLcdZoom,
						   131*nLcdZoom, (64-nLines)*nLcdZoom, SRCCOPY);
				GdiFlush();
			}
			LeaveCriticalSection(&csGDILock);
			hBmp = (HBITMAP) SelectObject(hBmpDC,hBmp);

			// fill BITMAP structure for size information
			GetObject(hBmp, sizeof(bm), &bm);

			wBits = bm.bmPlanes * bm.bmBitsPixel;
			// make sure bits per pixel is valid
			if (wBits <= 1)
				wBits = 1;
			else if (wBits <= 4)
				wBits = 4;
			else if (wBits <= 8)
				wBits = 8;
			else // if greater than 8-bit, force to 24-bit
				wBits = 24;

			dwSizeImage = WIDTHBYTES((DWORD)bm.bmWidth * wBits) * bm.bmHeight;

			// calculate memory size to store CF_DIB data
			dwLen = sizeof(BITMAPINFOHEADER) + dwSizeImage;
			if (wBits != 24)				// a 24 bitcount DIB has no color table
			{
				// add size for color table
				dwLen += (DWORD) (1 << wBits) * sizeof(RGBQUAD);
			}

			// memory allocation for clipboard data
			if ((hClipObj = GlobalAlloc(GMEM_MOVEABLE, dwLen)) != NULL)
			{
				lpbi = (LPBITMAPINFOHEADER ) GlobalLock(hClipObj);
				// initialize BITMAPINFOHEADER
				lpbi->biSize = sizeof(BITMAPINFOHEADER);
				lpbi->biWidth = bm.bmWidth;
				lpbi->biHeight = bm.bmHeight;
				lpbi->biPlanes = 1;
				lpbi->biBitCount = wBits;
				lpbi->biCompression = BI_RGB;
				lpbi->biSizeImage = dwSizeImage;
				lpbi->biXPelsPerMeter = 0;
				lpbi->biYPelsPerMeter = 0;
				lpbi->biClrUsed = 0;
				lpbi->biClrImportant = 0;
				// get bitmap color table and bitmap data
				GetDIBits(hBmpDC, hBmp, 0, lpbi->biHeight, (LPBYTE)lpbi + dwLen - dwSizeImage,
						  (LPBITMAPINFO)lpbi, DIB_RGB_COLORS);
				GlobalUnlock(hClipObj);
				SetClipboardData(CF_DIB, hClipObj);

				// get number of entries in the logical palette
				GetObject(hPalette,sizeof(WORD),&wBits);

				// memory allocation for temporary palette data
				if ((ppal = (PLOGPALETTE) calloc(sizeof(LOGPALETTE) + wBits * sizeof(PALETTEENTRY),1)) != NULL)
				{
					ppal->palVersion    = PALVERSION;
					ppal->palNumEntries = wBits;
					GetPaletteEntries(hPalette, 0, wBits, ppal->palPalEntry);
					SetClipboardData(CF_PALETTE, CreatePalette(ppal));
					free(ppal);
				}
			}
			DeleteDC(hBmpDC);
			DeleteObject(hBmp);
			#undef WIDTHBYTES
			#undef PALVERSION
		}
		CloseClipboard();
	}
	return 0;
}

//
// ID_TOOL_DISASM
//
static INT_PTR CALLBACK Disasm(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static DWORD dwAddress,dwAddressMax;

	enum MEM_MAPPING eMode;
	LONG  i;
	DWORD dwNxtAddr;
	TCHAR szAddress[256] = _T("0");

	switch (message)
	{
	case WM_INITDIALOG:
		VERIFY(SetMemRomType(cCurrentRomType)); // set current model

		// set fonts & cursor
		SendDlgItemMessage(hDlg,IDC_DISASM_MODULE,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_MODE_TEXT,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_MODE,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_ADDRESS,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_ADR,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_NEXT,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDC_DISASM_COPY,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));
		SendDlgItemMessage(hDlg,IDCANCEL,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),MAKELPARAM(FALSE,0));

		// fill disassembler mode combo box
		{
			// disassemble mode window
			HWND hWnd = GetDlgItem(hDlg,IDC_DISASM_MODE);

			if (hDlgDebug == NULL)			// debugger not open
			{
				LPCTSTR lpszModes[] = { _T("Map"), _T("NCE1"), _T("NCE2"), _T("CE1"), _T("CE2"), _T("NCE3") };

				for (eMode = MEM_MMU; eMode <= MEM_NCE3; eMode = (enum MEM_MAPPING) (eMode + 1))
				{
					if (GetMemAvail(eMode))
					{
						_ASSERT(eMode < ARRAYSIZEOF(lpszModes));
						i = (LONG) SendMessage(hWnd,CB_ADDSTRING,0,(LPARAM) lpszModes[eMode]);
						SendMessage(hWnd,CB_SETITEMDATA,i,(LPARAM) eMode);
					}
				}
				VERIFY(SendMessage(hWnd,CB_SETCURSEL,0,0) != LB_ERR);

				// disassemble with mapped modules
				VERIFY(SetMemMapType(MEM_MMU));
			}
			else							// debugger open
			{
				EnableWindow(hWnd,FALSE);
			}
		}

		SetDlgItemText(hDlg,IDC_DISASM_ADR,szAddress);
		dwAddressMax = GetMemDataSize();
		dwAddress = _tcstoul(szAddress,NULL,16);
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		// decode memory mode combo box
		case IDC_DISASM_MODE:
			// new combo box item selected
			if (HIWORD(wParam) == CBN_SELENDOK)
			{
				HWND hWnd = GetDlgItem(hDlg,IDC_DISASM_MODE);
				i = (LONG) SendMessage(hWnd,CB_GETCURSEL,0,0);
				eMode = (enum MEM_MAPPING) SendMessage(hWnd,CB_GETITEMDATA,i,0);
				VERIFY(SetMemMapType(eMode));
				dwAddressMax = GetMemDataSize();
			}
			break;
		case IDOK:
			SendDlgItemMessage(hDlg,IDC_DISASM_ADR,EM_SETSEL,0,-1);
			GetDlgItemText(hDlg,IDC_DISASM_ADR,szAddress,ARRAYSIZEOF(szAddress));
			// test if valid hex address
			for (i = 0; i < (LONG) lstrlen(szAddress); ++i)
			{
				if (_istxdigit((_TUCHAR) szAddress[i]) == FALSE)
					return FALSE;
			}
			dwAddress = _tcstoul(szAddress,NULL,16);
			// no break
		case IDC_DISASM_NEXT:
			if (dwAddress >= dwAddressMax)
				return FALSE;
			i = wsprintf(szAddress,(dwAddress <= 0xFFFFF) ? _T("%05lX   ") : _T("%06lX  "),dwAddress);
			// check if address content is a PCO (Primitive Code Object)
			dwNxtAddr = (dwAddress + 5) & 0xFFFFF;
			if (Read5(dwAddress) == dwNxtAddr)
			{
				if (disassembler_mode == HP_MNEMONICS)
				{
					_tcscat(&szAddress[i],_T("CON(5)  (*)+5"));
				}
				else
				{
					wsprintf(&szAddress[i],_T("dcr.5   $%05x"),dwNxtAddr);
				}
				dwAddress = dwNxtAddr;
			}
			else
			{
				dwAddress = disassemble(dwAddress,&szAddress[i]);
			}
			i = (LONG) SendDlgItemMessage(hDlg,IDC_DISASM_WIN,LB_ADDSTRING,0,(LPARAM) szAddress);
			SendDlgItemMessage(hDlg,IDC_DISASM_WIN,LB_SELITEMRANGE,FALSE,MAKELPARAM(0,i));
			SendDlgItemMessage(hDlg,IDC_DISASM_WIN,LB_SETSEL,TRUE,i);
			SendDlgItemMessage(hDlg,IDC_DISASM_WIN,LB_SETTOPINDEX,i,0);
			return TRUE;
		case IDC_DISASM_COPY:
			// copy selected items to clipboard
			CopyItemsToClipboard(GetDlgItem(hDlg,IDC_DISASM_WIN));
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg,IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
	UNREFERENCED_PARAMETER(lParam);
}

LRESULT OnToolDisasm(VOID)			// disasm dialogbox call
{
	if (pbyRom) SwitchToState(SM_SLEEP);
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_DISASM), hWnd, (DLGPROC)Disasm) == -1)
		AbortMessage(_T("Disassembler Dialog Box Creation Error!"));
	if (pbyRom) SwitchToState(SM_RUN);
	return 0;
}

//
// ID_HELP_TOPICS
//
LRESULT OnTopics(VOID)
{
	ShellExecute(hWnd,_T("open"),_T("Emu48.htm"),NULL,szEmuDirectory,SW_SHOWNORMAL);
	return 0;
}
