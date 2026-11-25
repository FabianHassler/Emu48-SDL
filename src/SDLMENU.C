/*
 *   SdlMenu.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Win32 menus and accelerators built from the resources in Emu48.rc, the
// menus drawn with Nuklear as menu bar or, without menu bar, as context menu

#include "pch.h"
#include "resource.h"
#include "Emu48.h"
#include "sdlmain.h"
#include "sdlnk.h"

#define MENU_ROW	(MENU_BAR_HEIGHT - 6)	// height of a menu row

//################
//#
//#    Win32 menus
//#
//################

typedef struct
{
	UINT  nId;								// command ID
	UINT  uFlags;							// MF_GRAYED, MF_CHECKED, MF_SEPARATOR, MF_POPUP
	HMENU hSubMenu;
	TCHAR szText[80];
} MENUITEM_T;

typedef struct
{
	UINT       nItems;
	MENUITEM_T sItem[24];
} MENU_T;

// find item by position or by command ID also in sub menus
static MENUITEM_T *FindItem(HMENU hMenu,UINT uItem,UINT uFlags,MENU_T **ppMenu)
{
	MENU_T *p = (MENU_T *) hMenu;
	MENUITEM_T *pItem = NULL;
	UINT i;

	if (p == NULL) return NULL;
	if (uFlags & MF_BYPOSITION)
	{
		if (uItem < p->nItems) pItem = &p->sItem[uItem];
	}
	else
	{
		for (i = 0; pItem == NULL && i < p->nItems; ++i)
		{
			if (p->sItem[i].hSubMenu != NULL)
			{
				if ((pItem = FindItem(p->sItem[i].hSubMenu,uItem,uFlags,ppMenu)) != NULL)
					return pItem;			// found in sub menu
			}
			else if (p->sItem[i].nId == uItem)
				pItem = &p->sItem[i];
		}
	}
	if (pItem && ppMenu) *ppMenu = p;
	return pItem;
}

BOOL DestroyMenu(HMENU hMenu)
{
	MENU_T *p = (MENU_T *) hMenu;
	UINT i;

	if (p == NULL) return FALSE;
	for (i = 0; i < p->nItems; ++i)
		DestroyMenu(p->sItem[i].hSubMenu);
	free(p);
	return TRUE;
}

// only insert by position
BOOL InsertMenu(HMENU hMenu,UINT uPosition,UINT uFlags,UINT_PTR uIDNewItem,LPCTSTR lpNewItem)
{
	MENU_T *p = (MENU_T *) hMenu;
	MENUITEM_T *pItem;

	_ASSERT(uFlags & MF_BYPOSITION);
	if (p->nItems >= ARRAYSIZEOF(p->sItem)) return FALSE;
	if (uPosition > p->nItems) uPosition = p->nItems; // append
	pItem = &p->sItem[uPosition];
	MoveMemory(pItem + 1,pItem,(p->nItems++ - uPosition) * sizeof(*pItem));
	ZeroMemory(pItem,sizeof(*pItem));
	pItem->uFlags = uFlags & ~MF_BYPOSITION;
	if (uFlags & MF_POPUP)
		pItem->hSubMenu = (HMENU) uIDNewItem;
	else
		pItem->nId = (UINT) uIDNewItem;
	lstrcpyn(pItem->szText,lpNewItem,ARRAYSIZEOF(pItem->szText));
	return TRUE;
}

BOOL DeleteMenu(HMENU hMenu,UINT uPosition,UINT uFlags)
{
	MENUITEM_T *pItem;
	MENU_T *p;

	if ((pItem = FindItem(hMenu,uPosition,uFlags,&p)) == NULL) return FALSE;
	DestroyMenu(pItem->hSubMenu);
	MoveMemory(pItem,pItem + 1,(--p->nItems - (pItem - p->sItem)) * sizeof(*pItem));
	return TRUE;
}

HMENU GetSubMenu(HMENU hMenu,INT nPos)
{
	MENUITEM_T *pItem = FindItem(hMenu,(UINT) nPos,MF_BYPOSITION,NULL);
	return pItem ? pItem->hSubMenu : NULL;
}

INT GetMenuItemCount(HMENU hMenu)
{
	return hMenu ? (INT) ((MENU_T *) hMenu)->nItems : -1;
}

UINT GetMenuItemID(HMENU hMenu,INT nPos)
{
	MENUITEM_T *pItem = FindItem(hMenu,(UINT) nPos,MF_BYPOSITION,NULL);
	return (pItem && pItem->hSubMenu == NULL) ? pItem->nId : (UINT) -1;
}

INT GetMenuString(HMENU hMenu,UINT uIDItem,LPTSTR lpString,INT cchMax,UINT flags)
{
	MENUITEM_T *pItem = FindItem(hMenu,uIDItem,flags,NULL);
	if (pItem == NULL) return 0;
	lstrcpyn(lpString,pItem->szText,cchMax);
	return lstrlen(lpString);
}

UINT GetMenuState(HMENU hMenu,UINT uId,UINT uFlags)
{
	MENUITEM_T *pItem = FindItem(hMenu,uId,uFlags,NULL);
	return pItem ? pItem->uFlags : (UINT) -1;
}

// set the uMask flag of an item, return the old flag
static UINT SetItemFlag(HMENU hMenu,UINT uItem,UINT uFlags,UINT uMask)
{
	MENUITEM_T *pItem = FindItem(hMenu,uItem,uFlags,NULL);
	UINT uOld;

	if (pItem == NULL) return (UINT) -1;
	uOld = pItem->uFlags & uMask;
	pItem->uFlags = (pItem->uFlags & ~uMask) | (uFlags & uMask);
	return uOld;
}

UINT EnableMenuItem(HMENU hMenu,UINT uIDEnableItem,UINT uEnable)
{
	return SetItemFlag(hMenu,uIDEnableItem,uEnable,MF_GRAYED);
}

DWORD CheckMenuItem(HMENU hMenu,UINT uIDCheckItem,UINT uCheck)
{
	return SetItemFlag(hMenu,uIDCheckItem,uCheck,MF_CHECKED);
}

HMENU CreatePopupMenu(VOID)
{
	return (HMENU) calloc(1,sizeof(MENU_T));
}

BOOL AppendMenu(HMENU hMenu,UINT uFlags,UINT_PTR uIDNewItem,LPCTSTR lpNewItem)
{
	return InsertMenu(hMenu,(UINT) -1,uFlags | MF_BYPOSITION,uIDNewItem,lpNewItem ? lpNewItem : _T(""));
}

// the menus of Emu48.rc, extracted by CMake, as table
enum { RC_NONE, RC_MENU, RC_BEGIN, RC_END, RC_POPUP, RC_ITEM };
#define MENUID(n)	},{ RC_MENU, NULL, n
#define BEGIN		},{ RC_BEGIN, NULL
#define END			},{ RC_END, NULL
#define POPUP		},{ RC_POPUP,
#define MENUITEM	},{ RC_ITEM,
#define SEPARATOR	_T(""), 0
#define GRAYED		MF_GRAYED
static const struct { INT nType; LPCTSTR szText; UINT nId; UINT uGrayed; } sMenuRc[] =
{
	{ RC_NONE, NULL
	#include "menu.rc"
	},{ RC_NONE, NULL }
};
#undef MENUID
#undef BEGIN
#undef END
#undef POPUP
#undef MENUITEM
#undef SEPARATOR
#undef GRAYED

HMENU LoadMenu(HINSTANCE hInstance,LPCTSTR lpMenuName)
{
	HMENU hRoot = NULL,hPopup = NULL,hLevel[4];
	INT   nLevel = 0;
	UINT  i;

	for (i = 0; sMenuRc[i].nType != RC_MENU || sMenuRc[i].nId != (UINT) (ULONG_PTR) lpMenuName; ++i)
		if (i == ARRAYSIZEOF(sMenuRc) - 1) return NULL;
	hRoot = hPopup = CreatePopupMenu();
	do										// up to the END of the menu
	{
		switch (sMenuRc[++i].nType)
		{
		case RC_BEGIN:
			hLevel[nLevel++] = hPopup;		// fill last popup
			break;
		case RC_END:
			--nLevel;
			break;
		case RC_POPUP:
			hPopup = CreatePopupMenu();
			InsertMenu(hLevel[nLevel-1],(UINT) -1,MF_BYPOSITION | MF_POPUP,(UINT_PTR) hPopup,sMenuRc[i].szText);
			break;
		case RC_ITEM:
			InsertMenu(hLevel[nLevel-1],(UINT) -1,MF_BYPOSITION | (sMenuRc[i].nId ? MF_STRING : MF_SEPARATOR) | sMenuRc[i].uGrayed,
					   sMenuRc[i].nId,sMenuRc[i].szText);
			break;
		}
	}
	while (nLevel > 0);
	return hRoot;
	UNREFERENCED_PARAMETER(hInstance);
}

// the IDR_MENU accelerators of Emu48.rc, extracted by CMake, as table
#define VIRTKEY		0x01
#define NOINVERT	0x02
#define SHIFT		0x04
#define CONTROL		0x08
#define ALT			0x10
static const struct { LPCTSTR szKey; UINT nId; BYTE byFlags[4]; } sAccelRc[] =
{
	#include "accel.rc"
};

// command of an accelerator key, Command instead of Control on macOS, 0 = none
UINT MenuAccelerator(const SDL_KeyboardEvent *pKey)
{
	UINT i,j;

	for (i = 0; i < ARRAYSIZEOF(sAccelRc); ++i)
	{
		BYTE byFlags = 0;

		for (j = 0; j < ARRAYSIZEOF(sAccelRc[i].byFlags); ++j) byFlags |= sAccelRc[i].byFlags[j];
		if (   pKey->key == (SDL_Keycode) SDL_tolower(sAccelRc[i].szKey[0])
			&& ((byFlags & CONTROL) != 0) == ((pKey->mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0)
			&& ((byFlags & SHIFT) != 0) == ((pKey->mod & SDL_KMOD_SHIFT) != 0)
			&& ((byFlags & ALT) != 0) == ((pKey->mod & SDL_KMOD_ALT) != 0))
			return sAccelRc[i].nId;
	}
	return 0;
}

//################
//#
//#    Menu bar and context menu
//#
//################

static struct nk_context *pNk = NULL;
static SDL_Renderer *pMenuRenderer = NULL;
static struct nk_user_font sFont[2];
static HMENU hMenuBar = NULL;				// menu bar of main window
static HMENU hMenuContext = NULL;			// main menu as context menu
static HMENU hContext = NULL;				// open context menu
static struct nk_vec2 ptContext;			// position of context menu
static BOOL  bMenuOpen = FALSE;				// a menu is open

// width of a menu with the given items
static float MenuWidth(struct nk_context *pNk,HMENU hMenu)
{
	MENU_T *p = (MENU_T *) hMenu;
	float fWidth = 0.0f;
	UINT i;

	for (i = 0; i < p->nItems; ++i)
	{
		TCHAR szLabel[ARRAYSIZEOF(p->sItem[i].szText)];

		StripAccel(p->sItem[i].szText,szLabel,ARRAYSIZEOF(szLabel));
		fWidth = __max(fWidth,NkTextWidth(pNk,szLabel));
		if (p->sItem[i].hSubMenu)			// sub menu shown as tree
			fWidth = __max(fWidth,MenuWidth(pNk,p->sItem[i].hSubMenu) + 20.0f);
	}
	return fWidth + 50.0f;
}

// menu items, sub menus as tree, the selected item is posted to the owner window
// as uMsg (WM_COMMAND or WM_SYSCOMMAND), return TRUE when an item was selected,
// the items are buttons in menu item style, they also work outside of popups
static BOOL DrawItems(struct nk_context *pNk,HMENU hMenu,HWND hOwner,UINT uMsg)
{
	MENU_T *p = (MENU_T *) hMenu;
	BOOL bSelected = FALSE,bCheck = FALSE;
	UINT i;

	for (i = 0; i < p->nItems; ++i)			// menu with check marks
		bCheck |= (p->sItem[i].uFlags & MF_CHECKED) != 0;

	for (i = 0; i < p->nItems; ++i)
	{
		MENUITEM_T *pItem = &p->sItem[i];
		TCHAR szLabel[ARRAYSIZEOF(pItem->szText)],szBuffer[ARRAYSIZEOF(pItem->szText)];
		LPCTSTR szText;
		nk_bool bItem;

		StripAccel(pItem->szText,szLabel,ARRAYSIZEOF(szLabel));
		if (pItem->uFlags & MF_SEPARATOR)
		{
			if (i == 0) continue;			// system menu without the window items
			nk_layout_row_dynamic(pNk,4,1);
			nk_rule_horizontal(pNk,pNk->style.window.border_color,nk_false);
			continue;
		}
		if (pItem->hSubMenu)				// tree with its own layout row
		{
			if (nk_tree_push_id(pNk,NK_TREE_NODE,szLabel,NK_MINIMIZED,(int) (INT_PTR) pItem->hSubMenu))
			{
				bSelected |= DrawItems(pNk,pItem->hSubMenu,hOwner,uMsg);
				nk_tree_pop(pNk);
			}
			continue;
		}
		nk_layout_row_dynamic(pNk,MENU_ROW,1);
		szText = NkTextEnd(pNk,szLabel,nk_widget_width(pNk) - 2.0f * pNk->style.contextual_button.padding.x
						   - (bCheck ? pNk->style.font->height + 8.0f : 0.0f),szBuffer,ARRAYSIZEOF(szBuffer));
		if (pItem->uFlags & MF_GRAYED) nk_widget_disable_begin(pNk);
		bItem = bCheck
			  ? nk_button_symbol_label_styled(pNk,&pNk->style.contextual_button,(pItem->uFlags & MF_CHECKED) ? NK_SYMBOL_CIRCLE_SOLID : NK_SYMBOL_NONE,szText,NK_TEXT_RIGHT)
			  : nk_button_label_styled(pNk,&pNk->style.contextual_button,szText);
		if (bItem)
		{
			PostMessage(hOwner,uMsg,pItem->nId,0); // execute after drawing
			bSelected = TRUE;
		}
		if (pItem->uFlags & MF_GRAYED) nk_widget_disable_end(pNk);
	}
	return bSelected;
}

// menu bar of a window, a system menu as first menu with the given title,
// return TRUE when a menu is open
static BOOL MenuBarDraw(struct nk_context *pNk,HMENU hMenu,HMENU hSysMenu,LPCTSTR szSysTitle,HWND hOwner,float fWidth)
{
	MENU_T *p = (MENU_T *) hMenu;
	BOOL  bOpen = FALSE;
	float fMenu;
	UINT  i;

	if (nk_begin(pNk,"MenuBar",nk_rect(0,0,fWidth,(float) MENU_BAR_HEIGHT),NK_WINDOW_NO_SCROLLBAR))
	{
		nk_menubar_begin(pNk);
		nk_layout_row_begin(pNk,NK_STATIC,MENU_ROW,(int) p->nItems + (hSysMenu != NULL));
		for (i = (hSysMenu != NULL) ? 0 : 1; i <= p->nItems; ++i)
		{
			TCHAR szLabel[ARRAYSIZEOF(p->sItem[0].szText)];
			HMENU hPopup = (i == 0) ? hSysMenu : p->sItem[i-1].hSubMenu;

			StripAccel((i == 0) ? szSysTitle : p->sItem[i-1].szText,szLabel,ARRAYSIZEOF(szLabel));
			nk_layout_row_push(pNk,NkTextWidth(pNk,szLabel) + 16.0f);
			// menu not wider than the window
			fMenu = __min(MenuWidth(pNk,hPopup),fWidth - nk_widget_bounds(pNk).x);
			if (nk_menu_begin_label(pNk,szLabel,NK_TEXT_CENTERED,nk_vec2(fMenu,600)))
			{
				bOpen = TRUE;
				if (DrawItems(pNk,hPopup,hOwner,(i == 0) ? WM_SYSCOMMAND : WM_COMMAND))
					nk_menu_close(pNk);
				nk_menu_end(pNk);
			}
		}
		nk_layout_row_end(pNk);
		nk_menubar_end(pNk);
	}
	nk_end(pNk);
	return bOpen;
}

// context menu at window position, moved left and up to end before fRight and above fBottom,
// return TRUE when it was closed
static BOOL MenuPopupDraw(struct nk_context *pNk,HMENU hMenu,HWND hOwner,float x,float y,float fRight,float fBottom)
{
	float fWidth = __min(MenuWidth(pNk,hMenu),fRight);
	static HMENU hLast = NULL;				// menu of fHeight
	static float fHeight;					// height of the menu content
	BOOL bClose;

	if (hMenu != hLast)						// new menu, estimate height
	{
		MENU_T *p = (MENU_T *) hMenu;
		UINT i;

		hLast = hMenu;
		for (fHeight = 8.0f, i = 0; i < p->nItems; ++i)
			fHeight += (p->sItem[i].uFlags & MF_SEPARATOR) ? 8.0f : MENU_ROW + 4.0f;
	}
	x = __max(0.0f,__min(x,fRight - fWidth));
	y = __max(0.0f,__min(y,fBottom - fHeight));
	nk_begin(pNk,"Context",nk_rect(x,y,fWidth,600),NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_DYNAMIC);
	nk_window_set_bounds(pNk,"Context",nk_rect(x,y,fWidth,600));
	nk_window_set_focus(pNk,"Context");		// the first click selects an item
	bClose = DrawItems(pNk,hMenu,hOwner,WM_COMMAND) // item selected or click outside
		  || (nk_input_is_mouse_pressed(&pNk->input,NK_BUTTON_LEFT) && !nk_window_is_hovered(pNk));
	nk_layout_row_dynamic(pNk,0.0f,1);		// content height from the position of the next row
	fHeight = nk_widget_bounds(pNk).y - y + 4.0f;
	nk_end(pNk);
	if (bClose)
	{
		nk_window_close(pNk,"Context");
		hLast = NULL;
	}
	return bClose;
}

// menu bar and open context menu of a window with the size fWidth x fHeight,
// the context menu is reset when closed, return TRUE when a menu is open
BOOL MenuDrawWindow(struct nk_context *pNk,HMENU hMenu,HMENU hSysMenu,LPCTSTR szSysTitle,HWND hOwner,
					HMENU *phContext,float xContext,float yContext,float fWidth,float fHeight)
{
	BOOL bOpen = FALSE;

	if (hMenu != NULL)
		bOpen = MenuBarDraw(pNk,hMenu,hSysMenu,szSysTitle,hOwner,fWidth);
	if (*phContext != NULL)
	{
		bOpen = TRUE;
		if (MenuPopupDraw(pNk,*phContext,hOwner,xContext,yContext,fWidth,fHeight))
			*phContext = NULL;
	}
	return bOpen;
}

VOID MenuInit(SDL_Window *pWindow,SDL_Renderer *pRenderer)
{
	pNk = NkCreate(pWindow,pRenderer,sFont);
	pMenuRenderer = pRenderer;
	hMenuBar = LoadMenu(NULL,MAKEINTRESOURCE(IDR_MENU));
	hMenuContext = LoadMenu(NULL,MAKEINTRESOURCE(IDM_MENU));
	return;
}

VOID MenuCleanup(VOID)
{
	DestroyMenu(hMenuContext);
	DestroyMenu(hMenuBar);
	nk_sdl_shutdown(pNk);
	return;
}

// menu bar of main window
HMENU GetMainMenu(VOID)
{
	return hMenuBar;
}

BOOL SetMenu(HWND hWnd,HMENU hMenu)
{
	hMenuBar = hMenu;						// the caller destroys the old menu
	return TRUE;
	UNREFERENCED_PARAMETER(hWnd);
}

// height of the menu bar
INT MenuBarHeight(VOID)
{
	return (hMenuBar != NULL) ? MENU_BAR_HEIGHT : 0;
}

// begin or end the input of a frame
VOID MenuInput(BOOL bBegin)
{
	if (bBegin) nk_input_begin(pNk); else nk_input_end(pNk);
	return;
}

// pass event to the menu, return TRUE when the event is for the menu
BOOL MenuEvent(SDL_Event *pEvent)
{
	float x,y;

	NkEvent(pNk,pEvent);
	switch (pEvent->type)
	{
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (bMenuOpen) return TRUE;			// click closes open menu
		if (pEvent->button.y < MenuBarHeight())
		{
			OnInitMenu(hMenuBar);			// update menu bar items like WM_INITMENU
			return TRUE;
		}
		return FALSE;
	case SDL_EVENT_MOUSE_BUTTON_UP:
	case SDL_EVENT_MOUSE_MOTION:
		SDL_GetMouseState(&x,&y);
		return bMenuOpen || y < MenuBarHeight();
	}
	return FALSE;
}

// open context menu at window position
VOID MenuContext(float x,float y)
{
	if (MenuBarHeight() > 0) return;		// only without menu bar
	OnInitMenu(hMenuContext);
	hContext = GetSubMenu(hMenuContext,0);
	ptContext = nk_vec2(x,y);
	return;
}

// draw menu bar and context menu
VOID MenuDraw(INT nWidth,INT nHeight)
{
	bMenuOpen = MenuDrawWindow(pNk,hMenuBar,NULL,NULL,hWnd,&hContext,ptContext.x,ptContext.y,(float) nWidth,(float) nHeight);
	NkRender(pNk,pMenuRenderer);
	return;
}
