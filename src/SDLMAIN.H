/*
 *   SdlMain.h
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// declarations shared by the SDL frontend files

#define MENU_BAR_HEIGHT	26					// height of a menu bar

#define FONT_GUI		0					// fonts of the Nuklear contexts
#define FONT_FIXED		1
#define FONT_BOLD		2					// flag for normal font drawn bold

// SdlMain.c
extern SDL_Window *pSdlWindow;
extern VOID    MainWindowUpdate(VOID);
extern VOID    DispatchPosted(SDL_Event *pEvent);
extern BYTE    VirtualKey(const SDL_KeyboardEvent *pKey);

// SdlDlg.c
struct nk_context;
struct nk_user_font;
typedef VOID (*NKDRAWPROC)(HWND hWnd, struct nk_context *pNk);
extern struct _SDLWND sMainWnd;
extern SDL_Window *GetParentWindow(HWND hWnd);
extern SDL_Window *GetSdlWindow(HWND hWnd);
extern LONG_PTR *GetUserData(HWND hWnd);
extern BOOL    IsDialog(HWND hWnd);
extern BOOL    DialogDestroy(HWND hDlg);
extern HWND    CreateNkWindow(HWND hWndParent, LPCTSTR szTitle, INT cx, INT cy, DLGPROC lpDialogFunc, NKDRAWPROC fnDraw);
extern const struct nk_user_font *NkFont(HWND hWnd, INT nFont);
extern BOOL    DialogEvent(SDL_Event *pEvent);
extern VOID    DialogInput(BOOL bBegin);
extern VOID    DialogUpdate(VOID);
extern struct nk_context *NkCreate(SDL_Window *pWindow, SDL_Renderer *pRenderer, struct nk_user_font *pUserFont);
extern VOID    NkEvent(struct nk_context *pNk, SDL_Event *pEvent);
extern VOID    NkRender(struct nk_context *pNk, SDL_Renderer *pRenderer);
extern float   NkTextWidth(struct nk_context *pNk, LPCTSTR szText);
extern LPCTSTR NkTextEnd(struct nk_context *pNk, LPCTSTR szText, float fWidth, LPTSTR szBuffer, INT nSize);
extern LPTSTR  StripAccel(LPCTSTR szText, LPTSTR szLabel, INT nSize);
extern BOOL    NkTextMetrics(HWND hWnd, INT nFont, TEXTMETRIC *lptm);
extern BOOL    NkTextOut(HWND hWnd, INT nFont, COLORREF crText, COLORREF crBk, INT x, INT y, const RECT *prc, LPCTSTR lpString, UINT c);
extern BOOL    NkFocusRect(HWND hWnd, const RECT *prc);
extern BOOL    NkBitBlt(HWND hWnd, INT x, INT y, INT cx, INT cy, HBITMAP hBmp, INT x1, INT y1);

// SdlHelp.c
extern VOID    HelpTopics(HWND hOwner);

// SdlUser.c
extern BOOL    FileDialog(HWND hOwner, SDL_FileDialogType eType, const SDL_DialogFileFilter *pFilter, INT nFilters, LPCTSTR szDefault, LPTSTR szFile, INT nMax);

// SdlMenu.c
extern VOID    MenuInit(SDL_Window *pWindow, SDL_Renderer *pRenderer);
extern VOID    MenuCleanup(VOID);
extern INT     MenuBarHeight(VOID);
extern VOID    MenuInput(BOOL bBegin);
extern BOOL    MenuEvent(SDL_Event *pEvent);
extern VOID    MenuContext(float x, float y);
extern VOID    MenuDraw(INT nWidth, INT nHeight);
extern HMENU   GetMainMenu(VOID);
extern BOOL    MenuDrawWindow(struct nk_context *pNk, HMENU hMenu, HMENU hSysMenu, LPCTSTR szSysTitle, HWND hOwner, HMENU *phContext, float xContext, float yContext, float fWidth, float fHeight);
extern UINT    MenuAccelerator(const SDL_KeyboardEvent *pKey);

// SdlGdi.c
extern UINT    GetRegionRects(HRGN hRgn, const RECT **ppRects);
extern UINT    DibColors(const BITMAPINFOHEADER *pbmih);
extern LPBYTE  GetBitmapBits(HBITMAP hBmp, INT *pnWidth, INT *pnHeight, INT *pnStride);
extern INT     GetFontType(HFONT hFont);
