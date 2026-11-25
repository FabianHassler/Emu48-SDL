/*
 *   SdlDlg.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Win32 dialog boxes on Nuklear: the dialog templates of Emu48.rc are
// compiled into a table, each dialog is a window with its controls drawn
// by Nuklear and the original dialog procedures get the Win32 messages

#include "pch.h"
#include "resource.h"
#include "Emu48.h"
#include "sdlmain.h"
#include "sdlnk.h"

#define FONT_SIZE	14.0f					// font size in points
#define SHEET_ROW	24.0f					// height of tabs and buttons of property sheets

//################
//#
//#    Dialog templates
//#
//################

enum { RC_NONE, RC_DIALOG, RC_STYLE, RC_EXSTYLE, RC_CAPTION, RC_MENU, RC_FONTFIXED, RC_BEGIN, RC_END,
	   RC_LTEXT, RC_RTEXT, RC_CTEXT, RC_PUSHBUTTON, RC_DEFPUSHBUTTON, RC_GROUPBOX,
	   RC_EDITTEXT, RC_COMBOBOX, RC_LISTBOX, RC_ICON, RC_CONTROL };

typedef struct
{
	INT     nDlg;							// dialog ID of RC_DIALOG
	INT     nType;
	LPCTSTR szText;
	INT     nId;
	INT     x,y,cx,cy;						// dialog units
	ULONGLONG qwStyle;						// styles, removed styles (NOT) in upper 32 bits
	LPCTSTR szClass;						// window class of RC_CONTROL
} DLGITEM_T;

// the dialog section of Emu48.rc, extracted by CMake, as table
#define NOT				((ULONGLONG) 1 << 32) *
#define DISCARDABLE
#define DIALOG			, RC_DIALOG, NULL, 0,
#define DIALOGEX		DIALOG
#define STYLE			}, { 0, RC_STYLE, NULL, 0, 0, 0, 0, 0,
#define EXSTYLE			}, { 0, RC_EXSTYLE, NULL, 0, 0, 0, 0, 0,
#define CAPTION			}, { 0, RC_CAPTION,
#define MENU			}, { 0, RC_MENU, NULL,
#define FONTFIXED		}, { 0, RC_FONTFIXED
#define BEGIN			}, { 0, RC_BEGIN
#define END				}, { 0, RC_END }, {
#define LTEXT			}, { 0, RC_LTEXT,
#define RTEXT			}, { 0, RC_RTEXT,
#define CTEXT			}, { 0, RC_CTEXT,
#define PUSHBUTTON		}, { 0, RC_PUSHBUTTON,
#define DEFPUSHBUTTON	}, { 0, RC_DEFPUSHBUTTON,
#define GROUPBOX		}, { 0, RC_GROUPBOX,
#define EDITTEXT		}, { 0, RC_EDITTEXT, NULL,
#define COMBOBOX		}, { 0, RC_COMBOBOX, NULL,
#define LISTBOX			}, { 0, RC_LISTBOX, NULL,
#define ICON			}, { 0, RC_ICON, (LPCTSTR) (INT_PTR)
#define CONTROL			}, { 0, RC_CONTROL,
static const DLGITEM_T sDlgRc[] =
{
	{
	#include "dialog.rc"
	0, RC_NONE }
};
#undef NOT
#undef DISCARDABLE
#undef DIALOG
#undef DIALOGEX
#undef STYLE
#undef EXSTYLE
#undef CAPTION
#undef MENU
#undef FONTFIXED
#undef BEGIN
#undef END
#undef LTEXT
#undef RTEXT
#undef CTEXT
#undef PUSHBUTTON
#undef DEFPUSHBUTTON
#undef GROUPBOX
#undef EDITTEXT
#undef COMBOBOX
#undef LISTBOX
#undef ICON
#undef CONTROL

//################
//#
//#    Nuklear windows
//#
//################

// system fonts of macOS and Linux, Nuklear font as fallback
static const LPCTSTR szFontFile[2][6] =
{
	{
		_T("/System/Library/Fonts/SFNS.ttf"),
		_T("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
		_T("/usr/share/fonts/dejavu/DejaVuSans.ttf"),
		_T("/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf"),
		_T("/usr/share/fonts/TTF/DejaVuSans.ttf"),
		_T("/usr/share/fonts/truetype/DejaVuSans.ttf")
	},
	{										// fixed pitch
		_T("/System/Library/Fonts/SFNSMono.ttf"),
		_T("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"),
		_T("/usr/share/fonts/dejavu/DejaVuSansMono.ttf"),
		_T("/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf"),
		_T("/usr/share/fonts/TTF/DejaVuSansMono.ttf"),
		_T("/usr/share/fonts/truetype/DejaVuSansMono.ttf")
	}
};

// Latin-1 and the punctuation, arrows and minus sign of the manual
static const nk_rune cGlyphRanges[] = { 0x0020, 0x00FF, 0x2014, 0x2026, 0x2190, 0x2192, 0x2212, 0x2212, 0 };

static float fDensity = 1.0f;				// pixels per point of the fonts

// the font is rendered in pixels, Nuklear uses points
static float FontWidth(nk_handle hFont,float fHeight,const char *szText,int nLen)
{
	const struct nk_user_font *p = (const struct nk_user_font *) hFont.ptr;
	return p->width(p->userdata,p->height,szText,nLen) / fDensity;
	UNREFERENCED_PARAMETER(fHeight);
}

static void FontQuery(nk_handle hFont,float fHeight,struct nk_user_font_glyph *pGlyph,nk_rune nChar,nk_rune nNext)
{
	const struct nk_user_font *p = (const struct nk_user_font *) hFont.ptr;
	p->query(p->userdata,p->height,pGlyph,nChar,nNext);
	pGlyph->offset.x /= fDensity;
	pGlyph->offset.y /= fDensity;
	pGlyph->width /= fDensity;
	pGlyph->height /= fDensity;
	pGlyph->xadvance /= fDensity;
	return;
	UNREFERENCED_PARAMETER(fHeight);
}

// Nuklear context of a window with the system fonts, sUserFont[FONT_GUI] and sUserFont[FONT_FIXED]
struct nk_context *NkCreate(SDL_Window *pWindow,SDL_Renderer *pRenderer,struct nk_user_font *pUserFont)
{
	struct nk_context *pNk = nk_sdl_init(pWindow,pRenderer,nk_sdl_allocator());
	struct nk_font_atlas *pAtlas;
	struct nk_font *pFont[2];
	LPVOID pData[2];
	size_t nSize;
	UINT   i,j;

	fDensity = SDL_GetWindowPixelDensity(pWindow);
	pAtlas = nk_sdl_font_stash_begin(pNk);
	for (j = 0; j < 2; ++j)
	{
		pData[j] = NULL;
		pFont[j] = NULL;
		for (i = 0; pData[j] == NULL && i < ARRAYSIZEOF(szFontFile[j]); ++i)
			pData[j] = SDL_LoadFile(szFontFile[j][i],&nSize);
		if (pData[j] != NULL)
		{
			struct nk_font_config sConfig = nk_font_config(FONT_SIZE * fDensity);

			sConfig.range = cGlyphRanges;
			pFont[j] = nk_font_atlas_add_from_memory(pAtlas,pData[j],nSize,FONT_SIZE * fDensity,&sConfig);
		}
		if (pFont[j] == NULL)
			pFont[j] = nk_font_atlas_add_default(pAtlas,13.0f * fDensity,NULL);
	}
	nk_sdl_font_stash_end(pNk);

	for (j = 0; j < 2; ++j)
	{
		SDL_free(pData[j]);
		pUserFont[j] = pFont[j]->handle;	// font with sizes in points
		pUserFont[j].userdata = nk_handle_ptr(&pFont[j]->handle);
		pUserFont[j].height /= fDensity;
		pUserFont[j].width = FontWidth;
		pUserFont[j].query = FontQuery;
	}
	nk_style_set_font(pNk,&pUserFont[FONT_GUI]);

	// light check marks, the dark ones of the default style look like unchecked boxes
	pNk->style.checkbox.cursor_normal = pNk->style.checkbox.cursor_hover = nk_style_item_color(nk_rgb(230,230,230));
	pNk->style.option.cursor_normal = pNk->style.option.cursor_hover = nk_style_item_color(nk_rgb(230,230,230));
	pNk->style.checkbox.normal = pNk->style.option.normal = nk_style_item_color(nk_rgb(60,60,60));
	pNk->style.contextual_button.text_alignment = NK_TEXT_LEFT; // menu items
	return pNk;
}

// draw the Nuklear commands into the window
VOID NkRender(struct nk_context *pNk,SDL_Renderer *pRenderer)
{
	SDL_SetRenderScale(pRenderer,fDensity,fDensity);
	nk_sdl_render(pNk,NK_ANTI_ALIASING_ON);
	return;
}

// pass event to Nuklear, letter keys only with Control or Command as shortcut,
// else Nuklear takes a fast press and release without Control as shortcut,
// and Nuklear takes all clicks after the first as double clicks
VOID NkEvent(struct nk_context *pNk,SDL_Event *pEvent)
{
	SDL_Event e = *pEvent;

	if (   (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
		&& e.key.key >= SDLK_A && e.key.key <= SDLK_Z)
	{
		if ((e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) == 0) return; // letters come as text input
		e.key.mod |= SDL_KMOD_CTRL;			// Command+C like Control+C
	}
	// like Win32 the clicks after a double click are a single click and a double click again
	if ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) && e.button.clicks > 1)
		e.button.clicks = (e.button.clicks % 2) ? 1 : 2;
	nk_sdl_handle_event(pNk,&e);
	return;
}

// width of a text in the font of the context
float NkTextWidth(struct nk_context *pNk,LPCTSTR szText)
{
	const struct nk_user_font *pFont = pNk->style.font;
	return pFont->width(pFont->userdata,pFont->height,szText,lstrlen(szText));
}

// text fitting into fWidth, a long text as "..." and its end (file and device names)
LPCTSTR NkTextEnd(struct nk_context *pNk,LPCTSTR szText,float fWidth,LPTSTR szBuffer,INT nSize)
{
	LPCTSTR p = szText;

	if (NkTextWidth(pNk,szText) <= fWidth) return szText;
	fWidth -= NkTextWidth(pNk,_T("..."));
	while (*p && NkTextWidth(pNk,p) > fWidth)
		do ++p; while ((*p & 0xC0) == 0x80);	// next UTF-8 character
	lstrcpy(szBuffer,_T("..."));
	lstrcpyn(szBuffer + 3,p,nSize - 3);
	return szBuffer;
}

// text without '&' accelerator marks, the tab before a shortcut key as spaces, buffer with nSize characters
LPTSTR StripAccel(LPCTSTR szText,LPTSTR szLabel,INT nSize)
{
	LPTSTR p = szLabel;

	for (; *szText && p < szLabel + nSize - 4; ++szText)
	{
		if (*szText == _T('&') && *++szText == 0) break;
		if (*szText == _T('\t'))
		{
			lstrcpy(p,_T("    "));
			p += 4;
			continue;
		}
		*p++ = *szText;
	}
	*p = 0;
	return szLabel;
}

//################
//#
//#    Windows
//#
//################

typedef struct
{
	LPTSTR szText;
	LPARAM lData;
	BOOL   bSel;
} LISTITEM_T;

struct _SDLWND
{
	HWND        hDlg;						// dialog of a control, property sheet of a page
	const DLGITEM_T *pItem;					// template of control or dialog
	DWORD       dwStyle;
	LPTSTR      szText;						// text, buffer size for edit controls
	INT         nTextSize;
	HFONT       hFont;						// font of WM_SETFONT, NULL = dialog font
	BOOL        bCheck;						// button state
	BOOL        bEditFocus;					// activate edit field with selected text
	INT         nDropped;					// drop down list of combo box: 0 = closed, 1 = opened in this frame, 2 = open
	LISTITEM_T *pList;						// list and combo box items
	INT         nItems,nSel,nCaret,nAnchor;
	INT         nTop;						// item to scroll to, -1 = none
	INT         nTopIndex;					// first visible item
	float       fItemHeight;				// height of a list box item
	INT         nMin,nMax;					// trackbar range
	TBBUTTON   *pButton;					// toolbar buttons
	INT         nButtons;
	SIZE        sButton;					// size of a toolbar button image
	HBITMAP     hBitmap;					// toolbar button images
	// dialog
	DLGPROC     lpDialogFunc;
	HWND        hCtrl;						// controls
	INT         nCtrls;
	HWND        hToolbar;
	HWND        hFocus;						// control with keyboard focus
	HWND        hOwner;						// owner of a modeless dialog
	HWND        hNext;						// next modeless dialog
	BOOL        bModeless;
	BOOL        bDestroyed;					// modeless dialog destroyed, memory freed later
	BOOL        bFixed;						// fixed pitch dialog font
	HMENU       hMenu,hSysMenu;				// menu bar and system menu
	HMENU       hContext;					// open context menu, owned by the caller
	float       xContext,yContext;			// position of context menu
	BOOL        bMenuOpen;					// a menu is open
	BOOL        bIgnoreUp;					// mouse button release not for the dialog
	LONG_PTR    lUserData;					// GWLP_USERDATA
	INT_PTR     nResult;
	BOOL        bEnd;
	float       fBaseX,fBaseY;				// points per dialog unit
	float       fClientTop;					// height of the menu bar
	float       fWindowHeight;				// window height from template
	float       fHeight;					// window height needed by an open combo box
	HWND        hPage;						// property sheet pages, bEnd = initialized
	INT         nPages,nPage,nNextPage;
	NKDRAWPROC  fnDraw;						// drawing of a window without dialog template
	SDL_Window *pWindow;
	SDL_Renderer *pRenderer;
	struct nk_context *pNk;
	struct nk_user_font sFont[2];
	HBITMAP     hTexBitmap[4];				// bitmaps drawn into the window
	SDL_Texture *pTexture[4];
};

struct _SDLWND sMainWnd;					// main window handle
static SDL_Window *pTopWindow = NULL;		// window of topmost modal dialog
static HWND hModeless = NULL;				// list of modeless dialogs
static INT  nModal = 0;						// nesting level of modal dialogs

// dialog with the window of a dialog or property sheet page
#define WINDLG(h)	((h)->pNk ? (h) : (h)->hDlg)

// control of a dialog
static __inline BOOL IsCtrl(HWND hWnd)
{
	return hWnd->pItem && hWnd->pItem->nType != RC_DIALOG;
}

// position of a control in the window below the menu bar in points
static struct nk_rect CtrlRect(HWND hCtrl)
{
	const DLGITEM_T *pItem = hCtrl->pItem;
	HWND hDlg = hCtrl->hDlg;

	return nk_rect(pItem->x * hDlg->fBaseX,pItem->y * hDlg->fBaseY,pItem->cx * hDlg->fBaseX,pItem->cy * hDlg->fBaseY);
}

// parent window of dialog, message and file boxes of a window, the topmost modal dialog
// or the window itself, on macOS a child window brings its parent to the front
SDL_Window *GetParentWindow(HWND hWnd)
{
	return pTopWindow ? pTopWindow : GetSdlWindow(hWnd);
}

SDL_Window *GetSdlWindow(HWND hWnd)
{
	return (hWnd == NULL || hWnd == &sMainWnd) ? pSdlWindow : WINDLG(hWnd)->pWindow;
}

LONG_PTR *GetUserData(HWND hWnd)
{
	return &hWnd->lUserData;
}

// modeless dialog not destroyed
BOOL IsDialog(HWND hWnd)
{
	HWND h;

	for (h = hModeless; h != NULL; h = h->hNext)
		if (h == hWnd) return !h->bDestroyed;
	return FALSE;
}

// copy of a text in UTF-8, Latin-1 texts are converted, no carriage returns
static VOID SetText(HWND hWnd,LPCTSTR szText)
{
	LPCTSTR p = szText;
	size_t  nLen = lstrlen(szText);
	LPTSTR  szUtf8 = NULL;
	INT     i,j;

	while (*p && SDL_StepUTF8(&p,&nLen) != SDL_INVALID_UNICODE_CODEPOINT) { }
	if (*p)									// no valid UTF-8, convert from Latin-1
		szText = szUtf8 = SDL_iconv_string("UTF-8","ISO-8859-1",szText,lstrlen(szText) + 1);
	nLen = lstrlen(szText) + 1;
	if ((INT) nLen > hWnd->nTextSize)
	{
		free(hWnd->szText);
		hWnd->szText = (LPTSTR) malloc(nLen);
		hWnd->nTextSize = (INT) nLen;
	}
	for (i = j = 0; szText[i]; ++i)
		if (szText[i] != _T('\r')) hWnd->szText[j++] = szText[i];
	hWnd->szText[j] = 0;
	SDL_free(szUtf8);
	return;
}

static VOID ResetList(HWND hWnd)
{
	while (hWnd->nItems > 0) free(hWnd->pList[--hWnd->nItems].szText);
	hWnd->nSel = -1;
	hWnd->nCaret = 0;
	return;
}

// insert item into list or combo box, sorted or at position, -1 = end
static INT AddItem(HWND hWnd,LPCTSTR szText,BOOL bSort,INT nPos)
{
	INT i = (nPos < 0 || nPos > hWnd->nItems) ? hWnd->nItems : nPos;

	hWnd->pList = (LISTITEM_T *) realloc(hWnd->pList,(hWnd->nItems + 1) * sizeof(*hWnd->pList));
	while (bSort && i > 0 && SDL_strcasecmp(hWnd->pList[i-1].szText,szText) > 0) --i;
	MoveMemory(&hWnd->pList[i+1],&hWnd->pList[i],(hWnd->nItems++ - i) * sizeof(*hWnd->pList));
	ZeroMemory(&hWnd->pList[i],sizeof(*hWnd->pList));
	hWnd->pList[i].szText = SDL_strdup(szText);
	if (hWnd->nSel >= i) ++hWnd->nSel;
	return i;
}

// find item of list or combo box, exact or prefix match
static INT FindItem(HWND hWnd,LPCTSTR szText,BOOL bExact)
{
	INT i;

	for (i = 0; i < hWnd->nItems; ++i)
	{
		if (bExact ? SDL_strcasecmp(hWnd->pList[i].szText,szText) == 0
				   : SDL_strncasecmp(hWnd->pList[i].szText,szText,lstrlen(szText)) == 0)
			return i;
	}
	return LB_ERR;
}

// valid index of a list item, invalid indices like on Windows also for negative numbers
static __inline BOOL IsItem(HWND hWnd,WPARAM wParam)
{
	return (UINT) wParam < (UINT) hWnd->nItems;
}

// combo box with edit field
static __inline BOOL IsDropDown(HWND hWnd)
{
	return hWnd->pItem->nType == RC_COMBOBOX && (hWnd->dwStyle & CBS_DROPDOWNLIST) == CBS_DROPDOWN;
}

// select combo box item, copy item text into the edit field
static INT SelectItem(HWND hWnd,INT nSel)
{
	hWnd->nSel = nSel;
	if (IsDropDown(hWnd) && nSel >= 0) SetText(hWnd,hWnd->pList[nSel].szText);
	return nSel;
}

// number of fully visible list box items
static INT VisibleItems(HWND hWnd)
{
	// height without frame and padding of the group
	INT n = (INT) ((hWnd->pItem->cy * WINDLG(hWnd->hDlg)->fBaseY - 4.0f) / hWnd->fItemHeight);
	return __max(n,1);
}

// scroll list box item into view
static VOID ShowItem(HWND hWnd,INT nItem)
{
	if (nItem < hWnd->nTopIndex)
		hWnd->nTop = nItem;
	if (nItem >= hWnd->nTopIndex + VisibleItems(hWnd))
		hWnd->nTop = nItem - VisibleItems(hWnd) + 1;
	return;
}

// messages of list boxes, combo boxes, edit controls, buttons, trackbars and toolbars
static LRESULT CtrlMessage(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	INT i,n;

	switch (Msg)
	{
	case LB_ADDSTRING:
		return AddItem(hWnd,(LPCTSTR) lParam,(hWnd->dwStyle & LBS_SORT) != 0,-1);
	case CB_ADDSTRING:
		return AddItem(hWnd,(LPCTSTR) lParam,(hWnd->dwStyle & CBS_SORT) != 0,-1);
	case LB_INSERTSTRING:
		return AddItem(hWnd,(LPCTSTR) lParam,FALSE,(INT) wParam);
	case LB_DELETESTRING:
		if (!IsItem(hWnd,wParam)) return LB_ERR;
		free(hWnd->pList[wParam].szText);
		MoveMemory(&hWnd->pList[wParam],&hWnd->pList[wParam+1],(--hWnd->nItems - wParam) * sizeof(*hWnd->pList));
		if (hWnd->nSel == (INT) wParam) hWnd->nSel = -1;
		if (hWnd->nSel > (INT) wParam) --hWnd->nSel;
		if (hWnd->nCaret >= hWnd->nItems) hWnd->nCaret = __max(hWnd->nItems - 1,0);
		return hWnd->nItems;
	case LB_RESETCONTENT:
	case CB_RESETCONTENT:
		ResetList(hWnd);
		return 0;
	case LB_GETCOUNT:
	case CB_GETCOUNT:
		return hWnd->nItems;
	case LB_GETCURSEL:
	case CB_GETCURSEL:
		return hWnd->nSel;
	case LB_SETCURSEL:
		hWnd->nSel = (IsItem(hWnd,wParam)) ? (INT) wParam : -1;
		for (i = 0; i < hWnd->nItems; ++i) hWnd->pList[i].bSel = (i == hWnd->nSel);
		if (hWnd->nSel >= 0)				// caret on selected item
		{
			hWnd->nCaret = hWnd->nSel;
			ShowItem(hWnd,hWnd->nSel);
		}
		return hWnd->nSel;
	case CB_SETCURSEL:
		return SelectItem(hWnd,(IsItem(hWnd,wParam)) ? (INT) wParam : -1);
	case CB_SELECTSTRING:
		if ((i = FindItem(hWnd,(LPCTSTR) lParam,FALSE)) != CB_ERR) SelectItem(hWnd,i);
		return i;
	case CB_FINDSTRINGEXACT:
		return FindItem(hWnd,(LPCTSTR) lParam,TRUE);
	case LB_GETTEXT:
	case CB_GETLBTEXT:
		if (!IsItem(hWnd,wParam)) return LB_ERR;
		lstrcpy((LPTSTR) lParam,hWnd->pList[wParam].szText);
		return lstrlen((LPTSTR) lParam);
	case LB_GETTEXTLEN:
		return (IsItem(hWnd,wParam)) ? lstrlen(hWnd->pList[wParam].szText) : LB_ERR;
	case LB_SETITEMDATA:
	case CB_SETITEMDATA:
		if (!IsItem(hWnd,wParam)) return LB_ERR;
		hWnd->pList[wParam].lData = lParam;
		return TRUE;
	case LB_GETITEMDATA:
	case CB_GETITEMDATA:
		return (IsItem(hWnd,wParam)) ? hWnd->pList[wParam].lData : LB_ERR;
	case LB_SETSEL:							// item lParam, -1 = all
		for (i = 0; i < hWnd->nItems; ++i)
			if ((INT) lParam == -1 || i == (INT) lParam) hWnd->pList[i].bSel = (BOOL) wParam;
		return 0;
	case LB_SELITEMRANGE:
		for (i = LOWORD(lParam); i <= HIWORD(lParam) && i < hWnd->nItems; ++i)
			hWnd->pList[i].bSel = (BOOL) wParam;
		return 0;
	case LB_GETSEL:
		return (IsItem(hWnd,wParam)) ? hWnd->pList[wParam].bSel : LB_ERR;
	case LB_GETSELCOUNT:
	case LB_GETSELITEMS:
		for (i = n = 0; i < hWnd->nItems; ++i)
		{
			if (hWnd->pList[i].bSel)
			{
				if (Msg == LB_GETSELITEMS && n < (INT) wParam) ((LPINT) lParam)[n] = i;
				++n;
			}
		}
		return n;
	case LB_SETTOPINDEX:
		hWnd->nTop = (INT) wParam;
		return 0;
	case LB_GETTOPINDEX:
		return hWnd->nTopIndex;
	case LB_SETCARETINDEX:
		if (!IsItem(hWnd,wParam)) return LB_ERR;
		hWnd->nCaret = (INT) wParam;
		ShowItem(hWnd,hWnd->nCaret);
		return 0;
	case LB_GETCARETINDEX:
		return hWnd->nCaret;
	case LB_GETITEMRECT:					// only for InvalidateRect(), items are always redrawn
		SetRect((LPRECT) lParam,0,0,0,0);
		return 0;
	case BM_SETCHECK:
		hWnd->bCheck = (wParam == BST_CHECKED);
		return 0;
	case WM_SETFONT:
		hWnd->hFont = (HFONT) wParam;
		return 0;
	case TBM_SETRANGE:
		hWnd->nMin = LOWORD(lParam);
		hWnd->nMax = HIWORD(lParam);
		return 0;
	case TBM_SETPOS:
		hWnd->nSel = (INT) lParam;
		return 0;
	case TBM_GETPOS:
		return hWnd->nSel;
	case TB_ENABLEBUTTON:
		for (i = 0; i < hWnd->nButtons; ++i)
		{
			if (hWnd->pButton[i].idCommand == (INT) wParam)
			{
				hWnd->pButton[i].fsState = LOWORD(lParam)
										 ? (hWnd->pButton[i].fsState | TBSTATE_ENABLED)
										 : (hWnd->pButton[i].fsState & ~TBSTATE_ENABLED);
				return TRUE;
			}
		}
		return FALSE;
	}
	return 0;								// TBM_SETTICFREQ, EM_SETSEL, WM_SETREDRAW, ...
}

LRESULT SendMessage(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	if (hWnd == NULL || hWnd == &sMainWnd || hWnd->bDestroyed) return 0;
	if (hWnd->lpDialogFunc) return hWnd->lpDialogFunc(hWnd,Msg,wParam,lParam);
	return CtrlMessage(hWnd,Msg,wParam,lParam);
}

HWND GetDlgItem(HWND hDlg,INT nIDDlgItem)
{
	INT i;

	for (i = 0; i < hDlg->nCtrls; ++i)
		if (hDlg->hCtrl[i].pItem->nId == nIDDlgItem) return &hDlg->hCtrl[i];
	return NULL;
}

LRESULT SendDlgItemMessage(HWND hDlg,INT nIDDlgItem,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	return SendMessage(GetDlgItem(hDlg,nIDDlgItem),Msg,wParam,lParam);
}

BOOL SetWindowText(HWND hWnd,LPCTSTR lpString)
{
	if (hWnd == &sMainWnd) return SDL_SetWindowTitle(pSdlWindow,lpString);
	SetText(hWnd,lpString);
	if (hWnd->pWindow) SDL_SetWindowTitle(hWnd->pWindow,hWnd->szText);
	return TRUE;
}

INT GetWindowText(HWND hWnd,LPTSTR lpString,INT nMaxCount)
{
	LPCTSTR szText = (hWnd->szText) ? hWnd->szText : _T("");

	if (hWnd->pItem && hWnd->pItem->nType == RC_COMBOBOX && !IsDropDown(hWnd))
		szText = IsItem(hWnd,hWnd->nSel) ? hWnd->pList[hWnd->nSel].szText : _T("");
	lstrcpyn(lpString,szText,nMaxCount);
	return lstrlen(lpString);
}

INT GetWindowTextLength(HWND hWnd)
{
	TCHAR szText[1024];
	return GetWindowText(hWnd,szText,ARRAYSIZEOF(szText));
}

BOOL SetDlgItemText(HWND hDlg,INT nIDDlgItem,LPCTSTR lpString)
{
	HWND hCtrl = GetDlgItem(hDlg,nIDDlgItem);
	if (hCtrl) SetText(hCtrl,lpString);
	return hCtrl != NULL;
}

UINT GetDlgItemText(HWND hDlg,INT nIDDlgItem,LPTSTR lpString,INT cchMax)
{
	HWND hCtrl = GetDlgItem(hDlg,nIDDlgItem);

	if (hCtrl == NULL)
	{
		*lpString = 0;
		return 0;
	}
	return GetWindowText(hCtrl,lpString,cchMax);
}

BOOL CheckDlgButton(HWND hDlg,INT nIDButton,UINT uCheck)
{
	HWND hCtrl = GetDlgItem(hDlg,nIDButton);
	if (hCtrl) hCtrl->bCheck = (uCheck == BST_CHECKED);
	return hCtrl != NULL;
}

UINT IsDlgButtonChecked(HWND hDlg,INT nIDButton)
{
	HWND hCtrl = GetDlgItem(hDlg,nIDButton);
	return (hCtrl && hCtrl->bCheck) ? BST_CHECKED : BST_UNCHECKED;
}

BOOL EnableWindow(HWND hWnd,BOOL bEnable)
{
	BOOL bDisabled = (hWnd->dwStyle & WS_DISABLED) != 0;
	hWnd->dwStyle = bEnable ? (hWnd->dwStyle & ~WS_DISABLED) : (hWnd->dwStyle | WS_DISABLED);
	return bDisabled;
}

BOOL EndDialog(HWND hDlg,INT_PTR nResult)
{
	hDlg->nResult = nResult;
	hDlg->bEnd = TRUE;
	return TRUE;
}

HWND GetParent(HWND hWnd)
{
	return IsCtrl(hWnd) ? hWnd->hDlg : hWnd->hOwner;
}

INT GetDlgCtrlID(HWND hWnd)
{
	return (hWnd && IsCtrl(hWnd)) ? hWnd->pItem->nId : 0;
}

HMENU GetMenu(HWND hWnd)
{
	return (hWnd == &sMainWnd) ? GetMainMenu() : hWnd->hMenu;
}

// system menu, shown as first menu of the menu bar
HMENU GetSystemMenu(HWND hWnd,BOOL bRevert)
{
	if (hWnd == &sMainWnd) return NULL;
	if (hWnd->hSysMenu == NULL) hWnd->hSysMenu = CreatePopupMenu();
	return hWnd->hSysMenu;
	UNREFERENCED_PARAMETER(bRevert);
}

// screen position of a window
BOOL GetWindowRect(HWND hWnd,LPRECT lpRect)
{
	struct nk_rect rc;
	int x,y,w,h;

	SDL_GetWindowPosition(GetSdlWindow(hWnd),&x,&y);
	if (!IsCtrl(hWnd))						// dialog window
	{
		SDL_GetWindowSize(GetSdlWindow(hWnd),&w,&h);
		return SetRect(lpRect,x,y,x+w,y+h);
	}
	rc = CtrlRect(hWnd);
	x += (INT) rc.x;
	y += (INT) (rc.y + WINDLG(hWnd->hDlg)->fClientTop);
	return SetRect(lpRect,x,y,x + (INT) rc.w,y + (INT) rc.h);
}

BOOL ShowWindow(HWND hWnd,INT nCmdShow)
{
	if (nCmdShow == SW_RESTORE) SDL_RestoreWindow(GetSdlWindow(hWnd));
	return TRUE;
}

HWND GetActiveWindow(VOID)
{
	SDL_Window *pWindow = SDL_GetKeyboardFocus();
	HWND h;

	for (h = hModeless; h != NULL; h = h->hNext)
		if (!h->bDestroyed && h->pWindow == pWindow) return h;
	return (pWindow == pSdlWindow) ? &sMainWnd : NULL;
}

VOID ForceForegroundWindow(HWND hWnd)
{
	SDL_RaiseWindow(GetSdlWindow(hWnd));
	return;
}

// the control at a position in dialog units, the dialog when there is none
HWND ChildWindowFromPointEx(HWND hWndParent,POINT pt,UINT uFlags)
{
	INT i;

	for (i = 0; i < hWndParent->nCtrls; ++i)
	{
		HWND hCtrl = &hWndParent->hCtrl[i];
		struct nk_rect rc = CtrlRect(hCtrl);

		if (   (hCtrl->dwStyle & WS_VISIBLE) == 0
			|| ((uFlags & CWP_SKIPDISABLED) && (hCtrl->dwStyle & WS_DISABLED)))
			continue;
		if (NK_INBOX(pt.x,pt.y,rc.x,rc.y,rc.w,rc.h)) return hCtrl;
	}
	return hWndParent;
}

//################
//#
//#    Messages to dialog procedures
//#
//################

#define WM_LISTKEY	(WM_USER + 0x7000)		// key for list box, wParam = virtual key, lParam = list box

// messages queued while drawing or handling events, sent after drawing
static struct { HWND hDlg; UINT uMsg; WPARAM wParam; LPARAM lParam; } sQueue[16];
static INT nQueue = 0;

static VOID QueueMessage(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (nQueue < (INT) ARRAYSIZEOF(sQueue))
	{
		sQueue[nQueue].hDlg = hDlg;
		sQueue[nQueue].uMsg = uMsg;
		sQueue[nQueue].wParam = wParam;
		sQueue[nQueue].lParam = lParam;
		++nQueue;
	}
	return;
}

// notification of a control
static VOID Notify(HWND hCtrl,UINT uCode)
{
	QueueMessage(hCtrl->hDlg,WM_COMMAND,MAKEWPARAM(hCtrl->pItem->nId,uCode),(LPARAM) hCtrl);
	return;
}

// move keyboard focus to control of a dialog
static VOID SetFocusCtrl(HWND hDlg,HWND hCtrl)
{
	HWND hOld = hDlg->hFocus;

	if (hCtrl && (hCtrl->pItem->nType == RC_EDITTEXT || IsDropDown(hCtrl)))
		hCtrl->bEditFocus = TRUE;			// like Win32 the text is selected
	if (hOld == hCtrl) return;
	hDlg->hFocus = hCtrl;
	if (hOld && hOld->pItem->nType == RC_LISTBOX && (hOld->dwStyle & LBS_NOTIFY))
		Notify(hOld,LBN_KILLFOCUS);
	if (hCtrl && hCtrl->pItem->nType == RC_LISTBOX && (hCtrl->dwStyle & LBS_NOTIFY))
		Notify(hCtrl,LBN_SETFOCUS);
	if (hCtrl && hCtrl->pItem->nType == RC_COMBOBOX)
		Notify(hCtrl,CBN_SETFOCUS);
	return;
}

// edit field, activated with selected text when it got the focus
static nk_flags EditString(struct nk_context *pNk,HWND hCtrl,nk_flags uFlags,int *pnLen,nk_plugin_filter pFilter)
{
	if (hCtrl->bEditFocus)
	{
		nk_edit_focus(pNk,NK_EDIT_ALWAYS_INSERT_MODE);
		pNk->current->edit.sel_start = 0;
		pNk->current->edit.sel_end = pNk->current->edit.cursor = (int) SDL_utf8strlen(hCtrl->szText);
		hCtrl->bEditFocus = FALSE;
	}
	return nk_edit_string(pNk,uFlags,hCtrl->szText,pnLen,hCtrl->nTextSize,pFilter);
}

HWND SetFocus(HWND hWnd)
{
	HWND hDlg,hOld = NULL;

	if (hWnd == NULL) return NULL;
	if (IsCtrl(hWnd))
	{
		hDlg = hWnd->hDlg;
		hOld = hDlg->hFocus;
		SetFocusCtrl(hDlg,hWnd);
	}
	SDL_RaiseWindow(GetSdlWindow(hWnd));
	return hOld;
}

// select list box item by mouse or keyboard
static VOID SelectListItem(HWND hCtrl,INT nItem,SDL_Keymod eMod)
{
	INT i;

	hCtrl->nCaret = nItem;
	if (hCtrl->dwStyle & LBS_NOSEL) return;
	if ((hCtrl->dwStyle & LBS_EXTENDEDSEL) && (eMod & (SDL_KMOD_GUI | SDL_KMOD_CTRL)))
	{
		hCtrl->pList[nItem].bSel = !hCtrl->pList[nItem].bSel;
		hCtrl->nSel = hCtrl->nAnchor = nItem;
	}
	else if ((hCtrl->dwStyle & LBS_EXTENDEDSEL) && (eMod & SDL_KMOD_SHIFT))
	{
		for (i = 0; i < hCtrl->nItems; ++i)
			hCtrl->pList[i].bSel = (i >= __min(hCtrl->nAnchor,nItem) && i <= __max(hCtrl->nAnchor,nItem));
		hCtrl->nSel = nItem;
	}
	else
	{
		CtrlMessage(hCtrl,LB_SETCURSEL,nItem,0);
		hCtrl->nAnchor = nItem;
	}
	if (hCtrl->dwStyle & LBS_NOTIFY) Notify(hCtrl,LBN_SELCHANGE);
	return;
}

// key for list box with focus, like WM_KEYDOWN of the list box
static VOID ListKey(HWND hCtrl,BYTE byKey)
{
	HWND hDlg = hCtrl->hDlg;
	INT  nItem = hCtrl->nCaret;
	INT  nAction = -1;						// default action

	if (hCtrl->dwStyle & LBS_WANTKEYBOARDINPUT)
		nAction = (INT) hDlg->lpDialogFunc(hDlg,WM_VKEYTOITEM,MAKEWPARAM(byKey,nItem),(LPARAM) hCtrl);
	if (nAction == -2 || hDlg->bDestroyed || hCtrl->nItems == 0) return;
	if (nAction >= 0)						// item to select
	{
		nItem = nAction;
	}
	else
	{
		switch (byKey)
		{
		case VK_UP:    --nItem; break;
		case VK_DOWN:  ++nItem; break;
		case VK_PRIOR: nItem -= VisibleItems(hCtrl) - 1; break;
		case VK_NEXT:  nItem += VisibleItems(hCtrl) - 1; break;
		case VK_HOME:  nItem = 0; break;
		case VK_END:   nItem = hCtrl->nItems - 1; break;
		default:       return;
		}
	}
	nItem = __max(0,__min(nItem,hCtrl->nItems - 1));
	SelectListItem(hCtrl,nItem,SDL_KMOD_NONE);
	ShowItem(hCtrl,nItem);
	return;
}

// message to a dialog procedure with the default actions
static VOID DialogMessage(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (hDlg->bDestroyed) return;
	switch (uMsg)
	{
	case WM_LISTKEY:
		ListKey((HWND) lParam,(BYTE) wParam);
		break;
	case WM_CLOSE:							// like DefDlgProc()
		if (!hDlg->lpDialogFunc(hDlg,WM_CLOSE,0,0))
			DialogMessage(hDlg,WM_COMMAND,IDCANCEL,0);
		break;
	case WM_COMMAND:						// like DefDlgProc(), modal dialogs end on IDCANCEL
		if (   !hDlg->lpDialogFunc(hDlg,WM_COMMAND,wParam,lParam)
			&& LOWORD(wParam) == IDCANCEL && !hDlg->bModeless && !hDlg->bDestroyed)
			EndDialog(hDlg,IDCANCEL);
		break;
	default:
		hDlg->lpDialogFunc(hDlg,uMsg,wParam,lParam);
	}
	return;
}

// send the queued messages
static VOID SendQueue(VOID)
{
	while (nQueue > 0)
	{
		HWND   hDlg = sQueue[0].hDlg;
		UINT   uMsg = sQueue[0].uMsg;
		WPARAM wParam = sQueue[0].wParam;
		LPARAM lParam = sQueue[0].lParam;

		MoveMemory(&sQueue[0],&sQueue[1],--nQueue * sizeof(sQueue[0]));
		DialogMessage(hDlg,uMsg,wParam,lParam);
	}
	return;
}

//################
//#
//#    Drawing
//#
//################

static struct nk_color NkColor(COLORREF cr)
{
	return nk_rgb(GetRValue(cr),GetGValue(cr),GetBValue(cr));
}

static struct nk_rect NkRect(const RECT *prc)
{
	return nk_rect((float) prc->left,(float) prc->top,(float) (prc->right - prc->left),(float) (prc->bottom - prc->top));
}

// text left in the rectangle and vertically centered
static VOID DrawLabel(struct nk_context *pNk,struct nk_rect rc,LPCTSTR szText,struct nk_color crText)
{
	const struct nk_user_font *pFont = pNk->style.font;

	nk_draw_text(nk_window_get_canvas(pNk),nk_rect(rc.x,rc.y + (rc.h - pFont->height) / 2.0f,rc.w,pFont->height),
				 szText,lstrlen(szText),pFont,nk_rgba(0,0,0,0),crText);
	return;
}

// font of a dialog font type, -1 = dialog font
static const struct nk_user_font *DlgFont(HWND hDlg,INT nFont)
{
	if (nFont < 0) nFont = hDlg->bFixed ? FONT_FIXED : FONT_GUI;
	return &WINDLG(hDlg)->sFont[nFont & FONT_FIXED];
}

// texture of a bitmap drawn into a dialog window, button face color transparent
static SDL_Texture *GetTexture(HWND hDlg,HBITMAP hBmp,INT *pnWidth,INT *pnHeight)
{
	SDL_Surface *pSurface;
	LPBYTE pbyBits;
	INT    i,x,y,nStride;

	hDlg = WINDLG(hDlg);
	pbyBits = GetBitmapBits(hBmp,pnWidth,pnHeight,&nStride);
	for (i = 0; i < (INT) ARRAYSIZEOF(hDlg->hTexBitmap) && hDlg->hTexBitmap[i] != NULL; ++i)
		if (hDlg->hTexBitmap[i] == hBmp) return hDlg->pTexture[i];
	if (i == (INT) ARRAYSIZEOF(hDlg->hTexBitmap)) return NULL;

	if ((pSurface = SDL_CreateSurface(*pnWidth,*pnHeight,SDL_PIXELFORMAT_ARGB8888)) == NULL)
		return NULL;
	for (y = 0; y < *pnHeight; ++y)
	{
		const DWORD *pdwSrc = (const DWORD *) (pbyBits + y * nStride);
		DWORD *pdwDest = (DWORD *) ((LPBYTE) pSurface->pixels + y * pSurface->pitch);

		for (x = 0; x < *pnWidth; ++x)
		{
			DWORD dwColor = pdwSrc[x] & 0x00FFFFFF;
			pdwDest[x] = (dwColor == 0x00C0C0C0) ? 0 : (dwColor | 0xFF000000);
		}
	}
	hDlg->hTexBitmap[i] = hBmp;
	hDlg->pTexture[i] = SDL_CreateTextureFromSurface(hDlg->pRenderer,pSurface);
	SDL_SetTextureScaleMode(hDlg->pTexture[i],SDL_SCALEMODE_NEAREST);
	SDL_DestroySurface(pSurface);
	return hDlg->pTexture[i];
}

// text output of the DC of a dialog
BOOL NkTextOut(HWND hWnd,INT nFont,COLORREF crText,COLORREF crBk,INT x,INT y,const RECT *prc,LPCTSTR lpString,UINT c)
{
	struct nk_context *pNk = WINDLG(hWnd)->pNk;
	struct nk_command_buffer *pCanvas = nk_window_get_canvas(pNk);
	const struct nk_user_font *pFont = DlgFont(hWnd,nFont);
	struct nk_rect rc = nk_rect((float) x,(float) y,4096.0f,pFont->height);

	if (prc != NULL)
	{
		if (crBk != (COLORREF) -1) nk_fill_rect(pCanvas,NkRect(prc),0.0f,NkColor(crBk));
		rc.w = (float) prc->right - x;
	}
	nk_draw_text(pCanvas,rc,lpString,(int) c,pFont,NkColor(crBk),NkColor(crText));
	if (nFont >= 0 && (nFont & FONT_BOLD))	// bold by drawing twice, Nuklear draws texts at whole points
	{
		rc.x += 1.0f;
		nk_draw_text(pCanvas,rc,lpString,(int) c,pFont,NkColor(crBk),NkColor(crText));
	}
	return TRUE;
}

BOOL NkFocusRect(HWND hWnd,const RECT *prc)
{
	nk_stroke_rect(nk_window_get_canvas(WINDLG(hWnd)->pNk),NkRect(prc),0.0f,1.0f,nk_rgb(128,128,128));
	return TRUE;
}

BOOL NkBitBlt(HWND hWnd,INT x,INT y,INT cx,INT cy,HBITMAP hBmp,INT x1,INT y1)
{
	struct nk_command_buffer *pCanvas = nk_window_get_canvas(WINDLG(hWnd)->pNk);
	SDL_Texture *pTexture;
	struct nk_image sImage;
	INT nWidth,nHeight;

	if ((pTexture = GetTexture(hWnd,hBmp,&nWidth,&nHeight)) == NULL) return FALSE;
	cx = __min(cx,nWidth - x1);				// not outside bitmap
	cy = __min(cy,nHeight - y1);
	sImage = nk_subimage_ptr(pTexture,(nk_ushort) nWidth,(nk_ushort) nHeight,nk_rect((float) x1,(float) y1,(float) cx,(float) cy));
	nk_draw_image(pCanvas,nk_rect((float) x,(float) y,(float) cx,(float) cy),&sImage,nk_rgb(255,255,255));
	return TRUE;
}

BOOL NkTextMetrics(HWND hWnd,INT nFont,TEXTMETRIC *lptm)
{
	const struct nk_user_font *pFont = DlgFont(hWnd,nFont);
	LPCTSTR szAlphabet = _T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	ZeroMemory(lptm,sizeof(*lptm));
	lptm->tmHeight = (LONG) SDL_ceilf(pFont->height);
	lptm->tmAscent = lptm->tmHeight;
	lptm->tmAveCharWidth = (LONG) SDL_ceilf(pFont->width(pFont->userdata,pFont->height,szAlphabet,lstrlen(szAlphabet)) / 52.0f);
	lptm->tmMaxCharWidth = lptm->tmAveCharWidth;
	return TRUE;
}

// window class of a control
static BOOL IsClass(const DLGITEM_T *pItem,LPCTSTR szClass)
{
	return pItem->nType == RC_CONTROL && lstrcmpi(pItem->szClass,szClass) == 0;
}

// static control, mouse clicks go to the dialog
static BOOL IsStatic(HWND hCtrl)
{
	switch (hCtrl->pItem->nType)
	{
	case RC_LTEXT:
	case RC_RTEXT:
	case RC_CTEXT:
	case RC_GROUPBOX:
	case RC_ICON:
	case RC_DIALOG:
		return TRUE;
	}
	return FALSE;
}

// a radio button checks itself and unchecks the other buttons of its group
static VOID CheckRadio(HWND hCtrl)
{
	HWND hDlg = hCtrl->hDlg, h;

	for (h = hCtrl; h > hDlg->hCtrl && (h->dwStyle & WS_GROUP) == 0; --h) { }
	for (; h < hDlg->hCtrl + hDlg->nCtrls; ++h)
	{
		if ((h->dwStyle & BS_TYPEMASK) == BS_AUTORADIOBUTTON) h->bCheck = (h == hCtrl);
		if (h + 1 < hDlg->hCtrl + hDlg->nCtrls && (h[1].dwStyle & WS_GROUP)) break;
	}
	return;
}

// scrollable area of a control
static nk_bool BeginGroup(struct nk_context *pNk,HWND hCtrl)
{
	TCHAR szName[16];
	nk_flags uFlags = 0;
	nk_uint x,y;

	if (hCtrl->dwStyle & WS_BORDER) uFlags |= NK_WINDOW_BORDER;
	if ((hCtrl->dwStyle & WS_VSCROLL) == 0) uFlags |= NK_WINDOW_NO_SCROLLBAR;
	wsprintf(szName,_T("G%d"),(INT) (hCtrl - hCtrl->hDlg->hCtrl));
	if (hCtrl->nTop >= 0)					// scroll to item of LB_SETTOPINDEX
	{
		nk_group_set_scroll(pNk,szName,0,(nk_uint) (hCtrl->nTop * hCtrl->fItemHeight));
		hCtrl->nTopIndex = hCtrl->nTop;
		hCtrl->nTop = -1;
	}
	else if (hCtrl->fItemHeight > 0.0f)		// list box
	{
		nk_group_get_scroll(pNk,szName,&x,&y);
		hCtrl->nTopIndex = (INT) (y / hCtrl->fItemHeight + 0.5f);
	}
	return nk_group_begin(pNk,szName,uFlags);
}

// mouse button pressed in this frame inside the rectangle
static BOOL IsPressed(struct nk_context *pNk,enum nk_buttons eButton,struct nk_rect rc)
{
	const struct nk_mouse_button *pButton = &pNk->input.mouse.buttons[eButton];
	// pressed or pressed and released in this frame
	return (pButton->down ? pButton->clicked > 0 : pButton->clicked > 1) && NK_INBOX(pButton->clicked_pos.x,pButton->clicked_pos.y,rc.x,rc.y,rc.w,rc.h);
}

// list box item, owner drawn by the dialog procedure
static VOID DrawListItem(struct nk_context *pNk,HWND hCtrl,INT nItem,struct nk_rect rc)
{
	HWND hDlg = hCtrl->hDlg;
	BOOL bSel = hCtrl->pList[nItem].bSel;

	if (hCtrl->dwStyle & LBS_OWNERDRAWFIXED)
	{
		DRAWITEMSTRUCT sDis;

		ZeroMemory(&sDis,sizeof(sDis));
		sDis.CtlID = hCtrl->pItem->nId;
		sDis.itemID = nItem;
		sDis.itemState = (bSel ? ODS_SELECTED : 0) | ((hDlg->hFocus == hCtrl && nItem == hCtrl->nCaret) ? ODS_FOCUS : 0);
		sDis.hwndItem = hCtrl;
		sDis.hDC = GetDC(hDlg);
		SetRect(&sDis.rcItem,(INT) SDL_floorf(rc.x),(INT) SDL_floorf(rc.y),(INT) SDL_ceilf(rc.x + rc.w),(INT) SDL_ceilf(rc.y + rc.h));
		sDis.itemData = hCtrl->pList[nItem].lData;
		hDlg->lpDialogFunc(hDlg,WM_DRAWITEM,sDis.CtlID,(LPARAM) &sDis);
		ReleaseDC(hDlg,sDis.hDC);
	}
	else
	{
		struct nk_color crText = bSel ? NkColor(GetSysColor(COLOR_HIGHLIGHTTEXT)) : pNk->style.text.color;

		if (bSel) nk_fill_rect(nk_window_get_canvas(pNk),rc,0.0f,NkColor(GetSysColor(COLOR_HIGHLIGHT)));
		if (hCtrl->dwStyle & WS_DISABLED) crText.a /= 2;
		DrawLabel(pNk,nk_rect(rc.x + 2.0f,rc.y,rc.w - 2.0f,rc.h),hCtrl->pList[nItem].szText,crText);
	}
	return;
}

// list box with selectable items
static VOID DrawList(struct nk_context *pNk,HWND hCtrl)
{
	struct nk_vec2 sSpacing = pNk->style.window.spacing;
	struct nk_vec2 sPadding = pNk->style.window.group_padding;
	INT i;

	pNk->style.window.spacing = nk_vec2(0.0f,0.0f);
	pNk->style.window.group_padding = nk_vec2(1.0f,1.0f);
	if (BeginGroup(pNk,hCtrl))
	{
		nk_layout_row_dynamic(pNk,hCtrl->fItemHeight,1);
		for (i = 0; i < hCtrl->nItems; ++i)
		{
			struct nk_rect rc;
			enum nk_widget_layout_states eState = nk_widget(&rc,pNk);

			if (eState == NK_WIDGET_INVALID) continue;
			// enabled, visible and not covered by a menu
			if (eState == NK_WIDGET_VALID && !WINDLG(hCtrl->hDlg)->bMenuOpen)
			{
				if (IsPressed(pNk,NK_BUTTON_LEFT,rc))
				{
					SetFocusCtrl(hCtrl->hDlg,hCtrl);
					SelectListItem(hCtrl,i,SDL_GetModState());
				}
				// double click, middle click or Option+click
				if (   IsPressed(pNk,NK_BUTTON_DOUBLE,rc)
					|| IsPressed(pNk,NK_BUTTON_MIDDLE,rc)
					|| (IsPressed(pNk,NK_BUTTON_LEFT,rc) && (SDL_GetModState() & SDL_KMOD_ALT)))
				{
					if (hCtrl->dwStyle & LBS_NOTIFY) Notify(hCtrl,LBN_DBLCLK);
				}
			}
			DrawListItem(pNk,hCtrl,i,rc);
		}
		nk_group_end(pNk);
	}
	pNk->style.window.spacing = sSpacing;
	pNk->style.window.group_padding = sPadding;
	return;
}

// read only multi line text as scrollable lines, Nuklear has no input for read only edit fields
static VOID DrawText(struct nk_context *pNk,HWND hCtrl)
{
	LPCTSTR p,q;

	if (BeginGroup(pNk,hCtrl))
	{
		nk_layout_row_dynamic(pNk,pNk->style.font->height,1);
		for (p = hCtrl->szText; *p; p = *q ? q + 1 : q)
		{
			if ((q = strchr(p,_T('\n'))) == NULL) q = p + lstrlen(p);
			nk_text(pNk,p,(int) (q - p),NK_TEXT_LEFT);
		}
		nk_group_end(pNk);
	}
	return;
}

// group box frame with title
static VOID DrawGroup(struct nk_context *pNk,LPCTSTR szLabel)
{
	struct nk_command_buffer *pCanvas = nk_window_get_canvas(pNk);
	const struct nk_user_font *pFont = pNk->style.font;
	struct nk_rect rc;
	float  fWidth = NkTextWidth(pNk,szLabel);

	nk_widget(&rc,pNk);
	rc.y += pFont->height / 2;
	rc.h -= pFont->height / 2;
	nk_stroke_rect(pCanvas,rc,2.0f,1.0f,pNk->style.window.border_color);
	nk_draw_text(pCanvas,nk_rect(rc.x + 6.0f,rc.y - pFont->height / 2,fWidth + 4.0f,pFont->height),szLabel,lstrlen(szLabel),
				 pFont,pNk->style.window.background,pNk->style.text.color);
	return;
}

// static text with the color of WM_CTLCOLORSTATIC
static VOID DrawStatic(struct nk_context *pNk,HWND hCtrl,LPCTSTR szLabel,nk_flags uAlign)
{
	HWND hDlg = hCtrl->hDlg;
	HDC  hDC = GetDC(hDlg);
	struct nk_color crText = pNk->style.text.color;

	if (hDlg->lpDialogFunc(hDlg,WM_CTLCOLORSTATIC,(WPARAM) hDC,(LPARAM) hCtrl))
		crText = NkColor(GetTextColor(hDC));
	ReleaseDC(hDlg,hDC);
	if (uAlign == 0)						// multi line text
		nk_label_colored_wrap(pNk,szLabel,crText);
	else
		nk_label_colored(pNk,szLabel,uAlign,crText);
	return;
}

// drop down list of a combo box below the combo box, aligned right when it doesn't fit into the window
static VOID DrawDropList(struct nk_context *pNk,HWND hCtrl,struct nk_rect rc)
{
	struct nk_rect rcWnd = nk_window_get_bounds(pNk);
	struct nk_rect rcClip = pNk->current->layout->clip;
	float fRow = pNk->style.font->height + 6.0f;
	float fList = __min(hCtrl->nItems,10) * (fRow + 4.0f) + 8.0f;
	float fWidth = rc.w;
	BOOL  bClose = FALSE;
	INT   i;

	for (i = 0; i < hCtrl->nItems; ++i)	// list as wide as the longest item
		fWidth = __max(fWidth,NkTextWidth(pNk,hCtrl->pList[i].szText) + 24.0f);
	fWidth = __min(fWidth,rcWnd.w);
	rc = nk_layout_space_rect_to_screen(pNk,rc);
	// the list must fit into the window
	WINDLG(hCtrl->hDlg)->fHeight = __max(WINDLG(hCtrl->hDlg)->fHeight,rc.y + rc.h + fList + 8.0f);
	if (rc.x + fWidth > rcWnd.x + rcWnd.w)	// aligned right
		rc.x = __max(rcWnd.x,rc.x + rc.w - fWidth);
	if (nk_popup_begin(pNk,NK_POPUP_STATIC,"DropList",NK_WINDOW_BORDER,
					   nk_rect(rc.x - rcClip.x,rc.y + rc.h - rcClip.y,fWidth,fList)))
	{
		nk_layout_row_dynamic(pNk,fRow,1);
		for (i = 0; i < hCtrl->nItems; ++i)
		{
			TCHAR szBuffer[256];
			LPCTSTR szText = NkTextEnd(pNk,hCtrl->pList[i].szText,nk_widget_width(pNk) - 12.0f,szBuffer,ARRAYSIZEOF(szBuffer));

			if (nk_button_label_styled(pNk,&pNk->style.contextual_button,szText))
			{
				if (i != hCtrl->nSel)
				{
					SelectItem(hCtrl,i);
					Notify(hCtrl,CBN_SELENDOK);
				}
				bClose = TRUE;
			}
		}
		// click outside of the list, not the click that opened it
		if (   hCtrl->nDropped == 2 && nk_input_is_mouse_pressed(&pNk->input,NK_BUTTON_LEFT)
			&& !nk_input_is_mouse_hovering_rect(&pNk->input,nk_window_get_bounds(pNk)))
			bClose = TRUE;
		if (bClose) nk_popup_close(pNk);
		nk_popup_end(pNk);
		hCtrl->nDropped = 2;
	}
	else
	{
		bClose = TRUE;
	}
	if (bClose) hCtrl->nDropped = 0;
	return;
}

// combo box with drop down list and optional edit field
static VOID DrawCombo(struct nk_context *pNk,HWND hCtrl,struct nk_rect rc)
{
	BOOL bClicked;
	INT  nLen;

	if (IsDropDown(hCtrl))					// edit field and drop down button
	{
		nk_layout_space_push(pNk,nk_rect(rc.x,rc.y,rc.w - rc.h,rc.h));
		nLen = lstrlen(hCtrl->szText);
		if (EditString(pNk,hCtrl,NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,&nLen,nk_filter_default) & NK_EDIT_ACTIVATED)
			SetFocusCtrl(hCtrl->hDlg,hCtrl);
		hCtrl->szText[nLen] = 0;
		nk_layout_space_push(pNk,nk_rect(rc.x + rc.w - rc.h,rc.y,rc.h,rc.h));
		bClicked = nk_button_symbol(pNk,NK_SYMBOL_TRIANGLE_DOWN);
	}
	else									// selected item left and drop down symbol right
	{
		struct nk_rect rcScreen = nk_layout_space_rect_to_screen(pNk,rc);
		TCHAR szBuffer[256];
		LPCTSTR szText = (hCtrl->nSel >= 0) ? hCtrl->pList[hCtrl->nSel].szText : _T("");
		float fSize = pNk->style.font->height / 2.0f;
		float x = rcScreen.x + rcScreen.w - fSize - 8.0f;	// position of the symbol
		float y = rcScreen.y + (rcScreen.h - fSize) / 2.0f;
		struct nk_color crText = pNk->style.button.text_normal;

		if (hCtrl->dwStyle & WS_DISABLED) crText.a /= 2;
		nk_layout_space_push(pNk,rc);
		bClicked = nk_button_label(pNk,_T(""));
		nk_fill_triangle(nk_window_get_canvas(pNk),x,y,x + fSize,y,x + fSize / 2.0f,y + fSize,crText);
		// text before the symbol
		szText = NkTextEnd(pNk,szText,x - rcScreen.x - 10.0f,szBuffer,ARRAYSIZEOF(szBuffer));
		DrawLabel(pNk,nk_rect(rcScreen.x + 6.0f,rcScreen.y,x - rcScreen.x - 6.0f,rcScreen.h),szText,crText);
	}
	if (bClicked)
	{
		SetFocusCtrl(hCtrl->hDlg,hCtrl);
		hCtrl->nDropped = 1;
	}
	if (hCtrl->nDropped) DrawDropList(pNk,hCtrl,rc);
	return;
}

static VOID DrawCtrl(struct nk_context *pNk,HWND hCtrl,struct nk_rect rc)
{
	const DLGITEM_T *pItem = hCtrl->pItem;
	TCHAR szLabel[256];
	nk_bool bCheck;
	INT nLen;

	if (pItem->nType == RC_COMBOBOX)		// combo box with its own layout
	{
		DrawCombo(pNk,hCtrl,rc);
		return;
	}
	nk_layout_space_push(pNk,rc);
	StripAccel(hCtrl->szText ? hCtrl->szText : _T(""),szLabel,ARRAYSIZEOF(szLabel));
	switch (pItem->nType)
	{
	case RC_LTEXT:
		DrawStatic(pNk,hCtrl,szLabel,(pItem->cy > 12) ? 0 : NK_TEXT_LEFT);
		break;
	case RC_RTEXT:
		DrawStatic(pNk,hCtrl,szLabel,NK_TEXT_RIGHT);
		break;
	case RC_CTEXT:
		DrawStatic(pNk,hCtrl,szLabel,NK_TEXT_CENTERED);
		break;
	case RC_PUSHBUTTON:
	case RC_DEFPUSHBUTTON:
		if (nk_button_label(pNk,szLabel)) Notify(hCtrl,BN_CLICKED);
		break;
	case RC_GROUPBOX:
		DrawGroup(pNk,szLabel);
		break;
	case RC_EDITTEXT:
		if ((hCtrl->dwStyle & (ES_MULTILINE | ES_READONLY)) == (ES_MULTILINE | ES_READONLY))
		{
			DrawText(pNk,hCtrl);
			break;
		}
		nLen = lstrlen(hCtrl->szText);
		EditString(pNk,hCtrl,(hCtrl->dwStyle & ES_MULTILINE) ? NK_EDIT_BOX : NK_EDIT_FIELD | NK_EDIT_SIG_ENTER
				   | ((hCtrl->dwStyle & ES_READONLY) ? NK_EDIT_READ_ONLY : 0),
				   &nLen,(hCtrl->dwStyle & ES_NUMBER) ? nk_filter_decimal : nk_filter_default);
		if (nLen != lstrlen(hCtrl->szText))
		{
			hCtrl->szText[nLen] = 0;
			Notify(hCtrl,EN_CHANGE);
		}
		break;
	case RC_LISTBOX:
		DrawList(pNk,hCtrl);
		break;
	case RC_CONTROL:
		if (IsClass(pItem,_T("msctls_trackbar32")))
		{
			nk_slider_int(pNk,hCtrl->nMin,&hCtrl->nSel,hCtrl->nMax,1);
			break;
		}
		switch (hCtrl->dwStyle & BS_TYPEMASK)
		{
		case BS_AUTOCHECKBOX:
			bCheck = hCtrl->bCheck;
			if (nk_checkbox_label(pNk,szLabel,&bCheck))
			{
				hCtrl->bCheck = bCheck;
				Notify(hCtrl,BN_CLICKED);
			}
			break;
		case BS_AUTORADIOBUTTON:
			if (nk_option_label(pNk,szLabel,hCtrl->bCheck) && !hCtrl->bCheck)
			{
				CheckRadio(hCtrl);
				Notify(hCtrl,BN_CLICKED);
			}
			break;
		default:
			nk_spacing(pNk,1);
		}
		break;
	default:								// RC_ICON
		nk_spacing(pNk,1);
	}
	return;
}

// toolbar buttons with tooltips
static VOID DrawToolbar(struct nk_context *pNk,HWND hDlg)
{
	HWND  hTb = hDlg->hToolbar;
	float x = 2.0f;
	INT   i,nWidth,nHeight;
	SDL_Texture *pTexture = GetTexture(hDlg,hTb->hBitmap,&nWidth,&nHeight);
	struct nk_style_button sButton = pNk->style.button;

	// light buttons for the button images made for the Windows button face color
	pNk->style.button.normal = nk_style_item_color(nk_rgb(200,200,200));
	pNk->style.button.hover = nk_style_item_color(nk_rgb(225,225,225));
	pNk->style.button.active = nk_style_item_color(nk_rgb(170,170,170));

	for (i = 0; i < hTb->nButtons; ++i)
	{
		const TBBUTTON *pButton = &hTb->pButton[i];
		struct nk_image sImage;
		BOOL bDisabled = (pButton->fsState & TBSTATE_ENABLED) == 0;

		if (pButton->fsStyle & TBSTYLE_SEP)
		{
			x += 8.0f;
			continue;
		}
		nk_layout_space_push(pNk,nk_rect(x,2.0f,hTb->sButton.cx + 8.0f,hTb->sButton.cy + 7.0f));
		x += hTb->sButton.cx + 9.0f;
		if (bDisabled) nk_widget_disable_begin(pNk);
		if (nk_widget_is_hovered(pNk))		// tooltip text from the dialog procedure
		{
			TOOLTIPTEXT sTip;

			ZeroMemory(&sTip,sizeof(sTip));
			sTip.hdr.hwndFrom = hTb;
			sTip.hdr.idFrom = pButton->idCommand;
			sTip.hdr.code = TTN_GETDISPINFO;
			sTip.lpszText = sTip.szText;
			hDlg->lpDialogFunc(hDlg,WM_NOTIFY,pButton->idCommand,(LPARAM) &sTip);
			if (((ULONG_PTR) sTip.lpszText >> 16) == 0) // string resource
			{
				LoadString(sTip.hinst,(UINT) (ULONG_PTR) sTip.lpszText,sTip.szText,ARRAYSIZEOF(sTip.szText));
				sTip.lpszText = sTip.szText;
			}
			if (*sTip.lpszText) nk_tooltip(pNk,sTip.lpszText);
		}
		sImage = nk_subimage_ptr(pTexture,(nk_ushort) nWidth,(nk_ushort) nHeight,
								 nk_rect((float) (pButton->iBitmap * hTb->sButton.cx),0.0f,(float) hTb->sButton.cx,(float) hTb->sButton.cy));
		if (nk_button_image(pNk,sImage))
			QueueMessage(hDlg,WM_COMMAND,MAKEWPARAM(pButton->idCommand,BN_CLICKED),(LPARAM) hTb);
		if (bDisabled) nk_widget_disable_end(pNk);
	}
	pNk->style.button = sButton;
	return;
}

// controls of a dialog or property sheet page
static VOID DrawControls(struct nk_context *pNk,HWND hDlg)
{
	INT i;

	nk_layout_space_begin(pNk,NK_STATIC,hDlg->pItem->cy * hDlg->fBaseY,INT_MAX);
	if (hDlg->hToolbar) DrawToolbar(pNk,hDlg);
	for (i = 0; i < hDlg->nCtrls; ++i)
	{
		HWND hCtrl = &hDlg->hCtrl[i];
		struct nk_rect rc = CtrlRect(hCtrl);

		if ((hCtrl->dwStyle & WS_VISIBLE) == 0) continue;
		// combo box height is the height of the dropped down list
		if (hCtrl->pItem->nType == RC_COMBOBOX) rc.h = pNk->style.font->height + 8.0f;
		nk_style_push_font(pNk,DlgFont(hDlg,GetFontType(hCtrl->hFont)));
		if (hCtrl->dwStyle & WS_DISABLED) nk_widget_disable_begin(pNk);
		DrawCtrl(pNk,hCtrl,rc);
		if (hCtrl->dwStyle & WS_DISABLED) nk_widget_disable_end(pNk);
		nk_style_pop_font(pNk);
	}
	nk_layout_space_end(pNk);
	return;
}

// page tabs of a property sheet
static VOID DrawTabs(struct nk_context *pNk,HWND hDlg)
{
	INT i;

	nk_layout_row_dynamic(pNk,SHEET_ROW,hDlg->nPages);
	for (i = 0; i < hDlg->nPages; ++i)
	{
		nk_bool bActive = (i == hDlg->nPage);
		if (nk_selectable_label(pNk,hDlg->hPage[i].szText,NK_TEXT_CENTERED,&bActive) && i != hDlg->nPage)
			hDlg->nNextPage = i;
	}
	return;
}

// OK and Cancel buttons of a property sheet
static VOID DrawSheetButtons(struct nk_context *pNk,HWND hDlg)
{
	nk_layout_row_dynamic(pNk,SHEET_ROW,4);
	nk_spacing(pNk,2);
	if (nk_button_label(pNk,_T("OK")))     QueueMessage(hDlg,WM_COMMAND,IDOK,0);
	if (nk_button_label(pNk,_T("Cancel"))) QueueMessage(hDlg,WM_COMMAND,IDCANCEL,0);
	return;
}

static VOID DrawDialog(HWND hDlg)
{
	struct nk_context *pNk = hDlg->pNk;
	int w,h,nOld;

	SDL_GetWindowSize(hDlg->pWindow,&w,&h);
	hDlg->fHeight = 0.0f;
	if (nk_begin(pNk,"Dialog",nk_rect(0,hDlg->fClientTop,(float) w,h - hDlg->fClientTop),NK_WINDOW_NO_SCROLLBAR))
	{
		if (hDlg->fnDraw)
		{
			hDlg->fnDraw(hDlg,pNk);
		}
		else
		{
			if (hDlg->hPage) DrawTabs(pNk,hDlg);
			DrawControls(pNk,hDlg->hPage ? &hDlg->hPage[hDlg->nPage] : hDlg);
			if (hDlg->hPage) DrawSheetButtons(pNk,hDlg);
		}
	}
	nk_end(pNk);

	// menus after the controls, they are in front
	hDlg->bMenuOpen = MenuDrawWindow(pNk,hDlg->hMenu,hDlg->hSysMenu,hDlg->szText,hDlg,
									 &hDlg->hContext,hDlg->xContext,hDlg->yContext,(float) w,(float) h);

	// enlarge window for an open combo box list
	if (hDlg->fnDraw == NULL)
	{
		h = (int) __max(hDlg->fWindowHeight,hDlg->fHeight);
		SDL_GetWindowSize(hDlg->pWindow,&w,&nOld);
		if (h != nOld) SDL_SetWindowSize(hDlg->pWindow,w,h);
	}

	SDL_RenderClear(hDlg->pRenderer);
	NkRender(pNk,hDlg->pRenderer);
	SDL_RenderPresent(hDlg->pRenderer);
	return;
}

//################
//#
//#    Dialog boxes
//#
//################

// dialog button for the Return key, the default push button or IDOK
static INT DefaultButton(HWND hDlg)
{
	INT i;

	for (i = 0; i < hDlg->nCtrls; ++i)
		if (hDlg->hCtrl[i].pItem->nType == RC_DEFPUSHBUTTON) return hDlg->hCtrl[i].pItem->nId;
	return IDOK;
}

// event of the dialog window, return FALSE for other windows
static BOOL HandleEvent(HWND hDlg,SDL_Event *pEvent)
{
	HWND  hPage = hDlg->hPage ? &hDlg->hPage[hDlg->nPage] : hDlg;
	POINT pt;								// mouse position in the client area
	int   nX,nY;
	BYTE  byKey;

	if (SDL_GetWindowFromEvent(pEvent) != hDlg->pWindow) return FALSE;
	pt.x = (LONG) pEvent->button.x;
	pt.y = (LONG) (pEvent->button.y - hDlg->fClientTop);
	switch (pEvent->type)
	{
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		QueueMessage(hDlg,WM_CLOSE,0,0);
		break;
	case SDL_EVENT_KEY_DOWN:
		if (pEvent->key.key == SDLK_ESCAPE)
		{
			QueueMessage(hPage,WM_COMMAND,IDCANCEL,0);
			return TRUE;
		}
		if (pEvent->key.key == SDLK_RETURN && !hDlg->hPage)
		{
			QueueMessage(hDlg,WM_COMMAND,DefaultButton(hDlg),(LPARAM) GetDlgItem(hDlg,DefaultButton(hDlg)));
			return TRUE;
		}
		// key for the list box with the keyboard focus
		if (   hPage->hFocus && hPage->hFocus->pItem->nType == RC_LISTBOX
			&& !hDlg->pNk->text_edit.active && (byKey = VirtualKey(&pEvent->key)) != 0)
		{
			QueueMessage(hPage,WM_LISTKEY,byKey,(LPARAM) hPage->hFocus);
			return TRUE;
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		// mouse button release ends a menu or is outside the controls
		hDlg->bIgnoreUp = hDlg->bMenuOpen || pt.y < 0 || (hDlg->hToolbar && pt.y < hDlg->hToolbar->sButton.cy + 9);
		if (pEvent->button.button == SDL_BUTTON_RIGHT && !hDlg->bIgnoreUp)
		{
			SDL_GetWindowPosition(hDlg->pWindow,&nX,&nY);
			QueueMessage(hPage,WM_CONTEXTMENU,(WPARAM) ChildWindowFromPointEx(hPage,pt,CWP_SKIPDISABLED),
						 MAKELPARAM(nX + pt.x,nY + pt.y + (LONG) hDlg->fClientTop));
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		// click on static control or dialog background
		if (   pEvent->button.button == SDL_BUTTON_LEFT && !hDlg->bIgnoreUp && pt.y >= 0 && hPage->pItem != NULL
			&& IsStatic(ChildWindowFromPointEx(hPage,pt,CWP_SKIPDISABLED)))
			QueueMessage(hPage,WM_LBUTTONUP,0,MAKELPARAM(pt.x,pt.y));
		break;
	}
	NkEvent(hDlg->pNk,pEvent);
	return TRUE;
}

// controls of a dialog template
static BOOL LoadDialog(HWND hDlg,INT nId)
{
	const DLGITEM_T *pItem;
	INT nLevel = 0,i;

	for (pItem = sDlgRc; pItem < sDlgRc + ARRAYSIZEOF(sDlgRc); ++pItem)
		if (pItem->nType == RC_DIALOG && pItem->nDlg == nId) break;
	if (pItem == sDlgRc + ARRAYSIZEOF(sDlgRc)) return FALSE;
	hDlg->pItem = pItem;

	// dialog properties and controls up to the END of the dialog
	SetText(hDlg,_T(""));
	hDlg->hCtrl = (HWND) calloc(1,sizeof(*hDlg));
	for (++pItem; pItem->nType != RC_END || --nLevel > 0; ++pItem)
	{
		switch (pItem->nType)
		{
		case RC_STYLE:     hDlg->dwStyle = (DWORD) pItem->qwStyle; break;
		case RC_CAPTION:   SetText(hDlg,pItem->szText); break;
		case RC_MENU:      hDlg->hMenu = LoadMenu(NULL,MAKEINTRESOURCE(pItem->nId)); break;
		case RC_FONTFIXED: hDlg->bFixed = TRUE; break;
		case RC_BEGIN:     ++nLevel; break;
		case RC_EXSTYLE:   break;
		default:								// control
			hDlg->hCtrl = (HWND) realloc(hDlg->hCtrl,(hDlg->nCtrls + 1) * sizeof(*hDlg));
			ZeroMemory(&hDlg->hCtrl[hDlg->nCtrls],sizeof(*hDlg));
			hDlg->hCtrl[hDlg->nCtrls].pItem = pItem;
			++hDlg->nCtrls;
		}
	}
	for (i = 0; i < hDlg->nCtrls; ++i)		// after realloc
	{
		HWND hCtrl = &hDlg->hCtrl[i];
		ULONGLONG qwStyle = hCtrl->pItem->qwStyle;

		// default styles of the resource compiler without the NOT styles
		if (hCtrl->pItem->nType == RC_LISTBOX) qwStyle |= LBS_NOTIFY | WS_BORDER;
		if (hCtrl->pItem->nType == RC_EDITTEXT) qwStyle |= WS_BORDER | WS_TABSTOP;
		hCtrl->hDlg = hDlg;
		hCtrl->dwStyle = (DWORD) (qwStyle & ~(qwStyle >> 32)) | WS_VISIBLE;
		hCtrl->nSel = hCtrl->nTop = -1;
		SetText(hCtrl,(hCtrl->pItem->nType == RC_ICON || hCtrl->pItem->szText == NULL) ? _T("") : hCtrl->pItem->szText);
		if ((hCtrl->pItem->nType == RC_EDITTEXT || hCtrl->pItem->nType == RC_COMBOBOX) && hCtrl->nTextSize < 256)
		{
			hCtrl->szText = (LPTSTR) realloc(hCtrl->szText,256);
			hCtrl->nTextSize = 256;			// buffer for input
		}
		if (IsClass(hCtrl->pItem,_T("msctls_trackbar32"))) hCtrl->nMax = 100;
	}
	return TRUE;
}

// window centered over the parent window, optionally its child window, size in dialog units and additional points
static BOOL NewWindow(HWND hDlg,SDL_Window *pParent,BOOL bChild,INT cx,INT cy,float fExtra)
{
	int x,y,w,h;

	SDL_GetWindowPosition(pParent,&x,&y);
	SDL_GetWindowSize(pParent,&w,&h);
	if (!SDL_CreateWindowAndRenderer(hDlg->szText,1,1,SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN,&hDlg->pWindow,&hDlg->pRenderer))
		return FALSE;
	hDlg->pNk = NkCreate(hDlg->pWindow,hDlg->pRenderer,hDlg->sFont);
	hDlg->pNk->style.window.padding = nk_vec2(0.0f,0.0f);
	hDlg->pNk->style.button.padding = nk_vec2(0.0f,0.0f); // dialog units are smaller than our font
	// dialog base units from the average character width and height of the font
	nk_style_set_font(hDlg->pNk,DlgFont(hDlg,-1));
	hDlg->fBaseX = NkTextWidth(hDlg->pNk,_T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")) / 52.0f / 4.0f;
	hDlg->fBaseY = DlgFont(hDlg,-1)->height / 8.0f;
	nk_style_set_font(hDlg->pNk,&hDlg->sFont[FONT_GUI]);
	hDlg->fClientTop = hDlg->hMenu ? (float) MENU_BAR_HEIGHT : 0.0f;
	hDlg->fWindowHeight = cy * hDlg->fBaseY + hDlg->fClientTop + fExtra;
	SDL_SetWindowSize(hDlg->pWindow,(int) (cx * hDlg->fBaseX),(int) hDlg->fWindowHeight);
	SDL_SetWindowPosition(hDlg->pWindow,x + (w - (int) (cx * hDlg->fBaseX)) / 2,y + (h - (int) hDlg->fWindowHeight) / 2);
	SDL_ShowWindow(hDlg->pWindow);
	SDL_StartTextInput(hDlg->pWindow);
	if (bChild) SDL_SetWindowParent(hDlg->pWindow,pParent);
	return TRUE;
}

// list box item heights, WM_INITDIALOG and keyboard focus
static VOID InitDialog(HWND hDlg,LPARAM lParam)
{
	INT i;

	for (i = 0; i < hDlg->nCtrls; ++i)
	{
		HWND hCtrl = &hDlg->hCtrl[i];

		if (hCtrl->pItem->nType != RC_LISTBOX) continue;
		hCtrl->fItemHeight = DlgFont(hDlg,-1)->height + (hDlg->bFixed ? 0.0f : 2.0f);
		if (hCtrl->dwStyle & LBS_OWNERDRAWFIXED)
		{
			MEASUREITEMSTRUCT sMis;

			ZeroMemory(&sMis,sizeof(sMis));
			sMis.CtlID = hCtrl->pItem->nId;
			sMis.itemHeight = (UINT) hCtrl->fItemHeight;
			hDlg->lpDialogFunc(hDlg,WM_MEASUREITEM,sMis.CtlID,(LPARAM) &sMis);
			hCtrl->fItemHeight = (float) sMis.itemHeight;
		}
	}
	if (hDlg->lpDialogFunc(hDlg,WM_INITDIALOG,0,lParam))
	{
		for (i = 0; i < hDlg->nCtrls; ++i)	// focus to first control with tab stop
		{
			if (hDlg->hCtrl[i].dwStyle & WS_TABSTOP)
			{
				SetFocusCtrl(hDlg,&hDlg->hCtrl[i]);
				break;
			}
		}
	}
	return;
}

static VOID FreeControls(HWND hDlg)
{
	INT i;

	for (i = 0; i < hDlg->nCtrls; ++i)
	{
		ResetList(&hDlg->hCtrl[i]);
		free(hDlg->hCtrl[i].pList);
		free(hDlg->hCtrl[i].szText);
	}
	free(hDlg->hCtrl);
	free(hDlg->szText);
	hDlg->hCtrl = NULL;
	hDlg->nCtrls = 0;
	hDlg->szText = NULL;
	return;
}

static VOID DestroyDialog(HWND hDlg)
{
	INT i;

	for (i = 0; i < hDlg->nPages; ++i) FreeControls(&hDlg->hPage[i]);
	free(hDlg->hPage);
	FreeControls(hDlg);
	if (hDlg->hToolbar)
	{
		DeleteObject(hDlg->hToolbar->hBitmap);
		free(hDlg->hToolbar->pButton);
		free(hDlg->hToolbar);
	}
	DestroyMenu(hDlg->hMenu);
	DestroyMenu(hDlg->hSysMenu);
	for (i = 0; i < (INT) ARRAYSIZEOF(hDlg->pTexture); ++i)
		SDL_DestroyTexture(hDlg->pTexture[i]);
	if (hDlg->pNk) nk_sdl_shutdown(hDlg->pNk);
	SDL_DestroyRenderer(hDlg->pRenderer);
	SDL_DestroyWindow(hDlg->pWindow);
	hDlg->pNk = NULL;
	hDlg->pWindow = NULL;
	return;
}

// send notification to a property sheet page
static VOID PageNotify(HWND hPage,UINT uCode)
{
	NMHDR sHdr = { hPage, 0, uCode };
	hPage->lpDialogFunc(hPage,WM_NOTIFY,0,(LPARAM) &sHdr);
	return;
}

// show property sheet page, initialize it on first use
static VOID ShowPage(HWND hDlg,INT nPage)
{
	HWND hPage = &hDlg->hPage[nPage];

	hDlg->nPage = hDlg->nNextPage = nPage;
	if (!hPage->bEnd)						// not initialized
	{
		InitDialog(hPage,hPage->nResult);
		hPage->bEnd = TRUE;
	}
	return;
}

// run modal dialog or property sheet until EndDialog(), the modeless dialogs are drawn without input
static INT_PTR ModalLoop(HWND hDlg)
{
	SDL_Window *pParent = pTopWindow;

	pTopWindow = hDlg->pWindow;
	SDL_SetWindowModal(hDlg->pWindow,true);
	++nModal;
	while (!hDlg->bEnd)
	{
		SDL_Event e;

		nk_input_begin(hDlg->pNk);
		DialogInput(TRUE);
		if (SDL_WaitEventTimeout(&e,10))
		{
			do
			{
				if (!HandleEvent(hDlg,&e))
				{
					if (e.type == SDL_EVENT_WINDOW_EXPOSED && SDL_GetWindowFromEvent(&e) == pSdlWindow)
						InvalidateRect(hWnd,NULL,FALSE); // main window
					DispatchPosted(&e);		// other input blocked by modal dialog
				}
			}
			while (!hDlg->bEnd && SDL_PollEvent(&e));
		}
		DialogInput(FALSE);
		nk_input_end(hDlg->pNk);
		if (hDlg->bEnd) break;
		DrawDialog(hDlg);
		SendQueue();
		if (hDlg->hPage && hDlg->nNextPage != hDlg->nPage)
		{
			PageNotify(&hDlg->hPage[hDlg->nPage],PSN_KILLACTIVE); // leave page
			ShowPage(hDlg,hDlg->nNextPage);
		}
		MainWindowUpdate();					// calculator keeps running
		DialogUpdate();
	}
	--nModal;
	pTopWindow = pParent;
	DestroyDialog(hDlg);
	return hDlg->nResult;
}

INT_PTR DialogBoxParam(HINSTANCE hInstance,LPCTSTR lpTemplateName,HWND hWndParent,DLGPROC lpDialogFunc,LPARAM dwInitParam)
{
	struct _SDLWND sDlg;

	ZeroMemory(&sDlg,sizeof(sDlg));
	if (   !LoadDialog(&sDlg,(INT) (ULONG_PTR) lpTemplateName)
		|| !NewWindow(&sDlg,GetParentWindow(hWndParent),TRUE,sDlg.pItem->cx,sDlg.pItem->cy,0.0f))
	{
		DestroyDialog(&sDlg);
		return -1;
	}
	sDlg.hOwner = hWndParent;
	sDlg.lpDialogFunc = lpDialogFunc;
	InitDialog(&sDlg,dwInitParam);
	return ModalLoop(&sDlg);
	UNREFERENCED_PARAMETER(hInstance);
}

//################
//#
//#    Modeless dialog boxes
//#
//################

HWND CreateDialog(HINSTANCE hInstance,LPCTSTR lpTemplateName,HWND hWndParent,DLGPROC lpDialogFunc)
{
	HWND hDlg = (HWND) calloc(1,sizeof(*hDlg));

	// a dialog owned by a dialog stays in front of its owner
	if (   !LoadDialog(hDlg,(INT) (ULONG_PTR) lpTemplateName)
		|| !NewWindow(hDlg,GetSdlWindow(hWndParent),IsDialog(hWndParent),hDlg->pItem->cx,hDlg->pItem->cy,0.0f))
	{
		DestroyDialog(hDlg);
		free(hDlg);
		return NULL;
	}
	hDlg->hOwner = hWndParent;
	hDlg->bModeless = TRUE;
	hDlg->lpDialogFunc = lpDialogFunc;
	hDlg->hNext = hModeless;				// add to list of modeless dialogs
	hModeless = hDlg;
	InitDialog(hDlg,0);
	return hDlg;
	UNREFERENCED_PARAMETER(hInstance);
}

// resizable modeless window without dialog template, drawn by fnDraw, size in points
HWND CreateNkWindow(HWND hWndParent,LPCTSTR szTitle,INT cx,INT cy,DLGPROC lpDialogFunc,NKDRAWPROC fnDraw)
{
	HWND hDlg = (HWND) calloc(1,sizeof(*hDlg));

	SetText(hDlg,szTitle);
	if (!NewWindow(hDlg,GetSdlWindow(hWndParent),FALSE,1,1,0.0f))
	{
		DestroyDialog(hDlg);
		free(hDlg);
		return NULL;
	}
	SDL_SetWindowSize(hDlg->pWindow,cx,cy);
	SDL_SetWindowMinimumSize(hDlg->pWindow,cx / 2,cy / 2);
	SDL_SetWindowResizable(hDlg->pWindow,true);
	hDlg->hOwner = hWndParent;
	hDlg->bModeless = TRUE;
	hDlg->lpDialogFunc = lpDialogFunc;
	hDlg->fnDraw = fnDraw;
	hDlg->hNext = hModeless;				// add to list of modeless dialogs
	hModeless = hDlg;
	return hDlg;
}

// font of a window, FONT_GUI or FONT_FIXED
const struct nk_user_font *NkFont(HWND hWnd,INT nFont)
{
	return DlgFont(hWnd,nFont);
}

// destroy modeless dialog and the dialogs it owns, the memory is freed by DialogUpdate()
BOOL DialogDestroy(HWND hDlg)
{
	HWND h;

	if (!hDlg->bModeless || hDlg->bDestroyed) return FALSE;
	for (h = hModeless; h != NULL; h = h->hNext)
		if (h->hOwner == hDlg) DialogDestroy(h);
	hDlg->lpDialogFunc(hDlg,WM_DESTROY,0,0);
	hDlg->bDestroyed = TRUE;
	DestroyDialog(hDlg);
	return TRUE;
}

// event of a modeless dialog window, return FALSE for other windows
BOOL DialogEvent(SDL_Event *pEvent)
{
	HWND hDlg;

	for (hDlg = hModeless; hDlg != NULL; hDlg = hDlg->hNext)
		if (!hDlg->bDestroyed && HandleEvent(hDlg,pEvent)) return TRUE;
	return FALSE;
}

// begin or end the input of the modeless dialogs
VOID DialogInput(BOOL bBegin)
{
	HWND hDlg;

	for (hDlg = hModeless; hDlg != NULL; hDlg = hDlg->hNext)
	{
		if (hDlg->bDestroyed) continue;
		if (bBegin)
			nk_input_begin(hDlg->pNk);
		else
			nk_input_end(hDlg->pNk);
	}
	return;
}

// draw the modeless dialogs, send their messages and free destroyed dialogs
VOID DialogUpdate(VOID)
{
	HWND hDlg,*phDlg;

	for (hDlg = hModeless; hDlg != NULL; hDlg = hDlg->hNext)
	{
		if (hDlg->bDestroyed) continue;
		DrawDialog(hDlg);
		SendQueue();
	}
	if (nModal > 0) return;					// dialogs may be in use
	for (phDlg = &hModeless; *phDlg != NULL;)
	{
		hDlg = *phDlg;
		if (hDlg->bDestroyed)
		{
			*phDlg = hDlg->hNext;
			free(hDlg);
		}
		else
		{
			phDlg = &hDlg->hNext;
		}
	}
	return;
}

// open context menu of a dialog at a screen position
BOOL TrackPopupMenu(HMENU hMenu,UINT uFlags,INT x,INT y,INT nReserved,HWND hWnd,const RECT *prcRect)
{
	HWND hDlg = WINDLG(hWnd);
	int  nX,nY;

	SDL_GetWindowPosition(hDlg->pWindow,&nX,&nY);
	hDlg->hContext = hMenu;
	hDlg->xContext = (float) (x - nX);
	hDlg->yContext = (float) (y - nY);
	return TRUE;
	UNREFERENCED_PARAMETER(uFlags);
	UNREFERENCED_PARAMETER(nReserved);
	UNREFERENCED_PARAMETER(prcRect);
}

// toolbar of a dialog above the controls
HWND CreateToolbarEx(HWND hwnd,DWORD ws,UINT wID,INT nBitmaps,HINSTANCE hBMInst,UINT_PTR wBMID,const TBBUTTON *lpButtons,INT iNumButtons,INT dxButton,INT dyButton,INT dxBitmap,INT dyBitmap,UINT uStructSize)
{
	HWND hTb = (HWND) calloc(1,sizeof(*hTb));

	hTb->hDlg = hwnd;
	hTb->pButton = (TBBUTTON *) malloc(iNumButtons * sizeof(*lpButtons));
	CopyMemory(hTb->pButton,lpButtons,iNumButtons * sizeof(*lpButtons));
	hTb->nButtons = iNumButtons;
	hTb->sButton.cx = dxBitmap;
	hTb->sButton.cy = dyBitmap;
	hTb->hBitmap = LoadBitmap(hBMInst,MAKEINTRESOURCE(wBMID));
	hwnd->hToolbar = hTb;
	return hTb;
	UNREFERENCED_PARAMETER(ws);
	UNREFERENCED_PARAMETER(wID);
	UNREFERENCED_PARAMETER(nBitmaps);
	UNREFERENCED_PARAMETER(dxButton);
	UNREFERENCED_PARAMETER(dyButton);
	UNREFERENCED_PARAMETER(uStructSize);
}

//################
//#
//#    Property sheets
//#
//################

// property sheet procedure, OK applies the settings of the active page
static INT_PTR CALLBACK SheetProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (uMsg == WM_COMMAND && LOWORD(wParam) == IDOK)
	{
		PageNotify(&hDlg->hPage[hDlg->nPage],PSN_KILLACTIVE);
		EndDialog(hDlg,TRUE);
	}
	if (uMsg == WM_COMMAND && LOWORD(wParam) == IDCANCEL)
		EndDialog(hDlg,FALSE);
	return TRUE;
	UNREFERENCED_PARAMETER(lParam);
}

// property sheet with page tabs above and buttons below the largest page
INT_PTR PropertySheet(PROPSHEETHEADER *lppsph)
{
	struct _SDLWND sDlg;
	DLGTEMPLATE sTemplate;
	INT i,cx = 0,cy = 0;

	ZeroMemory(&sDlg,sizeof(sDlg));
	ZeroMemory(&sTemplate,sizeof(sTemplate));
	if ((lppsph->dwFlags & PSH_USECALLBACK) && lppsph->pfnCallback)
		lppsph->pfnCallback(NULL,PSCB_PRECREATE,(LPARAM) &sTemplate);
	SetText(&sDlg,lppsph->pszCaption);
	sDlg.lpDialogFunc = SheetProc;
	sDlg.nPages = (INT) lppsph->nPages;
	sDlg.hPage = (HWND) calloc(sDlg.nPages,sizeof(sDlg));
	for (i = 0; i < sDlg.nPages; ++i)
	{
		HWND hPage = &sDlg.hPage[i];

		if (!LoadDialog(hPage,(INT) (ULONG_PTR) lppsph->ppsp[i].pszTemplate))
		{
			DestroyDialog(&sDlg);
			return -1;
		}
		hPage->hDlg = &sDlg;				// window of the pages
		hPage->lpDialogFunc = lppsph->ppsp[i].pfnDlgProc;
		hPage->nResult = (INT_PTR) &lppsph->ppsp[i]; // WM_INITDIALOG parameter
		cx = __max(cx,hPage->pItem->cx);
		cy = __max(cy,hPage->pItem->cy);
	}
	if (!NewWindow(&sDlg,GetParentWindow(lppsph->hwndParent),TRUE,cx,cy,2.0f * (SHEET_ROW + 4.0f) + 4.0f))
	{
		DestroyDialog(&sDlg);
		return -1;
	}
	for (i = 0; i < sDlg.nPages; ++i)		// pages use the window of the sheet
	{
		sDlg.hPage[i].fBaseX = sDlg.fBaseX;
		sDlg.hPage[i].fBaseY = sDlg.fBaseY;
	}
	if ((lppsph->dwFlags & PSH_USECALLBACK) && lppsph->pfnCallback)
		lppsph->pfnCallback(&sDlg,PSCB_INITIALIZED,0);
	ShowPage(&sDlg,(INT) lppsph->nStartPage);
	return ModalLoop(&sDlg);
}

//################
//#
//#    Folder selection
//#
//################

static TCHAR szFolder[MAX_PATH];			// selected folder

static INT_PTR CALLBACK BrowseProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (uMsg == BFFM_SETSELECTION)			// start folder
		lstrcpyn(szFolder,(LPCTSTR) lParam,ARRAYSIZEOF(szFolder));
	return 0;
	UNREFERENCED_PARAMETER(hDlg);
	UNREFERENCED_PARAMETER(wParam);
}

static VOID MallocFree(LPMALLOC pThis,LPVOID pv) { free(pv); }
static ULONG MallocRelease(LPMALLOC pThis) { return 0; }

LONG SHGetMalloc(LPMALLOC *ppMalloc)
{
	static const IMallocVtbl sVtbl = { MallocFree, MallocRelease };
	static struct IMalloc sMalloc = { &sVtbl };
	*ppMalloc = &sMalloc;
	return 0;
}

LPITEMIDLIST SHBrowseForFolder(BROWSEINFO *lpbi)
{
	struct _SDLWND sBrowse;

	ZeroMemory(&sBrowse,sizeof(sBrowse));
	sBrowse.lpDialogFunc = BrowseProc;		// receives BFFM_SETSELECTION
	szFolder[0] = 0;
	if (lpbi->lpfn) lpbi->lpfn(&sBrowse,BFFM_INITIALIZED,0,lpbi->lParam);
	if (!FileDialog(lpbi->hwndOwner,SDL_FILEDIALOG_OPENFOLDER,NULL,0,szFolder,szFolder,ARRAYSIZEOF(szFolder)))
		return NULL;
	return strdup(szFolder);				// freed by IMalloc
}

BOOL SHGetPathFromIDList(LPITEMIDLIST pidl,LPTSTR pszPath)
{
	lstrcpyn(pszPath,pidl,MAX_PATH);
	return TRUE;
}
