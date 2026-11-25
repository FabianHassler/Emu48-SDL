/*
 *   SdlHelp.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// help window with the manual doc/MANUAL.md, compiled into the program by
// CMake: the topics on the left, the chapter of the selected topic on the
// right, drawn with Nuklear from the Markdown subset used by the manual

#include "pch.h"
#include "Emu48.h"
#include "sdlmain.h"
#include "sdlnk.h"

#define TOPIC_WIDTH	210.0f					// width of the topic list
#define LINE_SPACE	1.35f					// line height in font heights
#define INDENT		18.0f					// indent of a list level

enum { BLK_HEAD, BLK_PARA, BLK_ITEM, BLK_CODE, BLK_ROW };

typedef struct
{
	BYTE   byType;							// BLK_*
	BYTE   byLevel;							// heading level, list level, header row
	BYTE   byIndent;						// list level of paragraphs, code and tables
	LPTSTR szText;							// text with Markdown marks, code lines, table cells with '|'
	TCHAR  szAnchor[64];					// anchor of a heading like on GitHub
} BLOCK_T;

typedef struct								// text piece of a single style
{
	LPCTSTR p;
	INT     nLen;
	BOOL    bSpace;							// space before the piece
	BOOL    bBreak;							// line break before the piece
	BYTE    byStyle;						// S_*
	LPCTSTR pLink;							// link target, NULL = none
	INT     nLink;
} PIECE_T;

enum { S_TEXT = 0, S_BOLD = 1, S_CODE = 2 };

static const char szManual[] =
{
	#include "manual.rc"
};

static BLOCK_T *pBlock = NULL;				// the manual as blocks
static INT      nBlocks = 0;
static HWND     hHelp = NULL;				// help window
static INT      nChapter = 0;				// block of the shown chapter
static INT      nScrollTo = -1;				// block to scroll to, -1 = none
static INT      nHistory[32];				// chapters of the back button
static INT      nHistLen = 0;
static nk_uint  uTopicX,uTopicY,uTextX,uTextY;	// scroll offsets
static float    fTextWidth = 500.0f;		// width of the text of the last frame
static TCHAR    szLink[256];				// link clicked in this frame

//################
//#
//#    Markdown parser
//#
//################

static VOID AddBlock(BYTE byType,BYTE byLevel,BYTE byIndent,LPCTSTR szText,INT nLen)
{
	BLOCK_T *p;

	if ((nBlocks & 63) == 0) pBlock = (BLOCK_T *) realloc(pBlock,(nBlocks + 64) * sizeof(*pBlock));
	p = &pBlock[nBlocks++];
	ZeroMemory(p,sizeof(*p));
	p->byType = byType;
	p->byLevel = byLevel;
	p->byIndent = byIndent;
	p->szText = (LPTSTR) malloc(nLen + 1);
	CopyMemory(p->szText,szText,nLen);
	p->szText[nLen] = 0;
	return;
}

// append a line to the last block, separated by cSep
static VOID AppendLine(LPCTSTR szLine,INT nLen,TCHAR cSep)
{
	BLOCK_T *p = &pBlock[nBlocks-1];
	INT nOld = lstrlen(p->szText);

	p->szText = (LPTSTR) realloc(p->szText,nOld + nLen + 2);
	p->szText[nOld] = cSep;
	CopyMemory(p->szText + nOld + 1,szLine,nLen);
	p->szText[nOld + 1 + nLen] = 0;
	return;
}

// anchor of a heading like GitHub: lower case, spaces as '-', without punctuation
static VOID MakeAnchor(LPTSTR szAnchor,LPCTSTR szText,INT nSize)
{
	INT i = 0,j,n;

	for (; *szText && i < nSize - 1; ++szText)
	{
		TCHAR c = *szText;

		if (c >= _T('A') && c <= _T('Z')) c += _T('a') - _T('A');
		if (c == _T(' ')) c = _T('-');
		if ((c >= _T('a') && c <= _T('z')) || (c >= _T('0') && c <= _T('9')) || c == _T('-') || c == _T('_') || (BYTE) c >= 0x80)
			szAnchor[i++] = c;
	}
	szAnchor[i] = 0;

	// a repeated anchor gets the number of the repetition like on GitHub, "general-1"
	for (j = n = 0; j < nBlocks - 1; ++j)
	{
		LPCTSTR pAnchor = pBlock[j].szAnchor;

		if (   pBlock[j].byType == BLK_HEAD && strncmp(pAnchor,szAnchor,i) == 0
			&& (pAnchor[i] == 0 || (pAnchor[i] == _T('-') && lstrlen(pAnchor + i + 1) == 1 && pAnchor[i+1] >= _T('1') && pAnchor[i+1] <= _T('9'))))
			++n;
	}
	if (n > 0) wsprintf(szAnchor + i,_T("-%d"),n);
	return;
}

// the manual as blocks: headings, paragraphs, list items, code and table rows
static VOID ParseManual(VOID)
{
	LPCTSTR p = szManual;
	BOOL bList = FALSE;						// inside a list
	BOOL bOpen = FALSE;						// last block takes continuation lines
	INT  nContent = 0;						// indent of the text of the last list item

	while (*p)
	{
		LPCTSTR szLine = p;
		INT nLen,nIndent = 0;
		BOOL bBreak;

		while (*p && *p != _T('\n')) ++p;
		nLen = (INT) (p - szLine);
		if (*p) ++p;
		while (nIndent < nLen && szLine[nIndent] == _T(' ')) ++nIndent;
		szLine += nIndent;
		nLen -= nIndent;

		if (nLen == 0)						// blank line ends a paragraph
		{
			bOpen = FALSE;
			continue;
		}
		if (nIndent == 0 && szLine[0] != _T('-') && !bOpen)
			bList = FALSE;
		if (bList ? nIndent >= nContent + 4 : nIndent >= 4)
		{
			INT nStrip = bList ? nContent + 4 : 4;

			szLine -= nIndent - nStrip;		// code keeps its indent after the block indent
			nLen += nIndent - nStrip;
			if (nBlocks > 0 && pBlock[nBlocks-1].byType == BLK_CODE && !bOpen)
				AppendLine(szLine,nLen,_T('\n'));
			else
				AddBlock(BLK_CODE,0,(BYTE) (bList ? nContent / 2 : 0),szLine,nLen);
			bOpen = FALSE;
			continue;
		}
		if (szLine[0] == _T('#'))
		{
			INT nLevel = 0;

			while (szLine[nLevel] == _T('#')) ++nLevel;
			AddBlock(BLK_HEAD,(BYTE) nLevel,0,szLine + nLevel + 1,nLen - nLevel - 1);
			MakeAnchor(pBlock[nBlocks-1].szAnchor,pBlock[nBlocks-1].szText,ARRAYSIZEOF(pBlock[nBlocks-1].szAnchor) - 4);
			bList = bOpen = FALSE;
			continue;
		}
		if (szLine[0] == _T('|'))			// table row, the separator row marks the header
		{
			if (szLine[1] == _T('-'))
			{
				if (nBlocks > 0 && pBlock[nBlocks-1].byType == BLK_ROW) pBlock[nBlocks-1].byLevel = 1;
				continue;
			}
			AddBlock(BLK_ROW,0,(BYTE) (bList ? nContent / 2 : 0),szLine,nLen);
			bOpen = FALSE;
			continue;
		}
		bBreak = nLen > 0 && szLine[nLen-1] == _T('\\'); // hard line break
		if (bBreak) --nLen;
		if (szLine[0] == _T('-') && szLine[1] == _T(' '))
		{
			AddBlock(BLK_ITEM,(BYTE) (nIndent / 2),0,szLine + 2,nLen - 2);
			bList = bOpen = TRUE;
			nContent = nIndent + 2;
		}
		else if (bOpen)						// continuation line
		{
			AppendLine(szLine,nLen,_T(' '));
		}
		else
		{
			AddBlock(BLK_PARA,0,(BYTE) (bList && nIndent > 0 ? nIndent / 2 : 0),szLine,nLen);
			bOpen = TRUE;
		}
		if (bBreak) AppendLine(_T(""),0,_T('\n'));
	}
	return;
}

//################
//#
//#    Drawing
//#
//################

static struct nk_color crText    = { 200, 200, 200, 255 };
static struct nk_color crBold    = { 255, 255, 255, 255 };
static struct nk_color crCode    = { 230, 195, 130, 255 };
static struct nk_color crCodeBk  = {  58,  58,  58, 255 };
static struct nk_color crLink    = { 110, 165, 255, 255 };
static struct nk_color crLine    = {  80,  80,  80, 255 };

// text of a block as pieces of a single style, links as target, Markdown marks removed
static INT SplitPieces(LPCTSTR p,PIECE_T **ppPiece)
{
	static PIECE_T *pPiece = NULL;
	static INT nMax = 0;
	INT  n = 0;
	BYTE byStyle = S_TEXT;
	LPCTSTR pLink = NULL,pLinkEnd = NULL;	// link text
	LPCTSTR pTarget = NULL;					// link target
	INT  nTarget = 0;
	BOOL bSpace = FALSE,bBreak = FALSE;

	while (*p)
	{
		LPCTSTR q;

		if ((*p == _T(' ') || *p == _T('\n')) && byStyle != S_CODE)
		{
			if (*p == _T('\n')) bBreak = TRUE; else bSpace = TRUE;
			++p;
			continue;
		}
		if (byStyle != S_CODE)
		{
			if (p[0] == _T('*') && p[1] == _T('*')) { byStyle ^= S_BOLD; p += 2; continue; }
			if (p[0] == _T('*')) { ++p; continue; }	// italic, drawn upright
			if (p[0] == _T('`')) { byStyle = S_CODE; ++p; continue; }
			if (p[0] == _T('[') && (q = strstr(p,_T("]("))) != NULL && strchr(q,_T(')')) != NULL)
			{
				pLink = p + 1;				// [text](target)
				pLinkEnd = q;
				pTarget = q + 2;
				nTarget = (INT) (strchr(q,_T(')')) - pTarget);
				++p;
				continue;
			}
			if (p == pLinkEnd)
			{
				p = pTarget + nTarget + 1;
				pLink = pLinkEnd = NULL;
				continue;
			}
			if (p[0] == _T('<') && strncmp(p + 1,_T("http"),4) == 0 && (q = strchr(p,_T('>'))) != NULL)
			{
				pLink = p + 1;				// <url>
				pLinkEnd = q;
				pTarget = p + 1;
				nTarget = (INT) (q - pTarget);
				++p;
				continue;
			}
		}
		else if (*p == _T('`'))
		{
			byStyle = S_TEXT;
			++p;
			continue;
		}

		// text up to a space or a style change, code with its spaces
		for (q = p; *q && q != pLinkEnd; ++q)
		{
			if (byStyle == S_CODE) { if (*q == _T('`')) break; continue; }
			if (*q == _T(' ') || *q == _T('\n')) break;
			if (*q == _T('*') || *q == _T('`') || (*q == _T('[') && pLink == NULL) || (*q == _T('<') && strncmp(q + 1,_T("http"),4) == 0))
				break;
		}
		if (q == p) { ++p; continue; }		// a single mark character as text
		if (n == nMax) pPiece = (PIECE_T *) realloc(pPiece,(nMax += 64) * sizeof(*pPiece));
		pPiece[n].p = p;
		pPiece[n].nLen = (INT) (q - p);
		pPiece[n].bSpace = bSpace;
		pPiece[n].bBreak = bBreak;
		pPiece[n].byStyle = byStyle;
		pPiece[n].pLink = pLink ? pTarget : NULL;
		pPiece[n].nLink = nTarget;
		++n;
		bSpace = bBreak = FALSE;
		p = q;
	}
	*ppPiece = pPiece;
	return n;
}

static const struct nk_user_font *PieceFont(HWND hWnd,const PIECE_T *p)
{
	return NkFont(hWnd,p->byStyle == S_CODE ? FONT_FIXED : FONT_GUI);
}

// wrapped text with styles and links at x,y, width fWidth, drawn when pCanvas isn't NULL, returns the height
static float DrawRich(struct nk_context *pNk,struct nk_command_buffer *pCanvas,LPCTSTR szText,
					  float x,float y,float fWidth,struct nk_color crNormal,BOOL bBold)
{
	const float fLine = NkFont(hHelp,FONT_GUI)->height * LINE_SPACE;
	PIECE_T *pPiece;
	INT   i,j,nPieces = SplitPieces(szText,&pPiece);
	float fX = 0.0f,fY = 0.0f;

	for (i = 0; i < nPieces; i = j)
	{
		float fWord = 0.0f,fSpace;

		// a word are the pieces up to the next space, its width with the space before
		fSpace = (pPiece[i].bSpace && fX > 0.0f) ? NkFont(hHelp,FONT_GUI)->width(NkFont(hHelp,FONT_GUI)->userdata,0.0f,_T(" "),1) : 0.0f;
		for (j = i; j < nPieces && (j == i || (!pPiece[j].bSpace && !pPiece[j].bBreak)); ++j)
		{
			const struct nk_user_font *pFont = PieceFont(hHelp,&pPiece[j]);
			fWord += pFont->width(pFont->userdata,0.0f,pPiece[j].p,pPiece[j].nLen);
		}
		if (pPiece[i].bBreak || (fX > 0.0f && fX + fSpace + fWord > fWidth))
		{
			fX = 0.0f;						// next line
			fY += fLine;
			fSpace = 0.0f;
		}
		fX += fSpace;
		for (; i < j; ++i)
		{
			const PIECE_T *p = &pPiece[i];
			const struct nk_user_font *pFont = PieceFont(hHelp,p);
			float fW = pFont->width(pFont->userdata,0.0f,p->p,p->nLen);
			struct nk_rect rc = nk_rect(x + fX,y + fY + (fLine - pFont->height) / 2.0f,fW,pFont->height);
			struct nk_color cr = crNormal;

			if (pCanvas != NULL)
			{
				if (p->byStyle == S_BOLD || bBold) cr = crBold; // bold as bright text, Nuklear has no bold font
				if (p->byStyle == S_CODE)
				{
					cr = crCode;
					nk_fill_rect(pCanvas,nk_rect(rc.x - 1.0f,rc.y - 1.0f,rc.w + 2.0f,rc.h + 2.0f),2.0f,crCodeBk);
				}
				if (p->pLink)
				{
					cr = crLink;
					if (nk_input_is_mouse_hovering_rect(&pNk->input,rc))
						nk_stroke_line(pCanvas,rc.x,rc.y + rc.h,rc.x + rc.w,rc.y + rc.h,1.0f,crLink);
					if (nk_input_is_mouse_click_in_rect(&pNk->input,NK_BUTTON_LEFT,rc)) // released, pressed in rc
						lstrcpyn(szLink,p->pLink,__min(p->nLink + 1,(INT) ARRAYSIZEOF(szLink)));
				}
				nk_draw_text(pCanvas,rc,p->p,p->nLen,pFont,nk_rgba(0,0,0,0),cr);
			}
			fX += fW;
		}
	}
	return nPieces ? fY + fLine : 0.0f;
}

// height of a block, drawn into rc when pCanvas isn't NULL
static float DrawBlock(struct nk_context *pNk,struct nk_command_buffer *pCanvas,const BLOCK_T *pBlk,struct nk_rect rc)
{
	const struct nk_user_font *pFont = NkFont(hHelp,FONT_GUI);
	const struct nk_user_font *pFixed = NkFont(hHelp,FONT_FIXED);
	const float fLine = pFont->height * LINE_SPACE;
	float x = rc.x + pBlk->byIndent * INDENT,w = rc.w - pBlk->byIndent * INDENT;
	float h = 0.0f;

	switch (pBlk->byType)
	{
	case BLK_HEAD:
		h = DrawRich(pNk,pCanvas,pBlk->szText,x,rc.y,w,crBold,TRUE) + (pBlk->byLevel <= 2 ? 6.0f : 2.0f);
		if (pCanvas)
		{
			if (pBlk->byLevel <= 2)
				nk_stroke_line(pCanvas,x,rc.y + h - 3.0f,x + w,rc.y + h - 3.0f,1.0f,crLine);
		}
		break;
	case BLK_PARA:
		h = DrawRich(pNk,pCanvas,pBlk->szText,x,rc.y,w,crText,FALSE);
		break;
	case BLK_ITEM:
		x += pBlk->byLevel * INDENT;
		w -= pBlk->byLevel * INDENT;
		if (pCanvas)
			nk_draw_text(pCanvas,nk_rect(x + 4.0f,rc.y + (fLine - pFont->height) / 2.0f,INDENT,pFont->height),
						 "\xE2\x80\xA2",3,pFont,nk_rgba(0,0,0,0),crText);
		h = DrawRich(pNk,pCanvas,pBlk->szText,x + INDENT,rc.y,w - INDENT,crText,FALSE);
		break;
	case BLK_CODE:
		{
			const float fCode = pFixed->height * 1.25f;
			LPCTSTR p = pBlk->szText;
			INT nLines = 1;

			for (; *p; ++p) if (*p == _T('\n')) ++nLines;
			h = nLines * fCode + 8.0f;
			if (pCanvas)
			{
				float y = rc.y + 4.0f;

				nk_fill_rect(pCanvas,nk_rect(x,rc.y,w,h),3.0f,crCodeBk);
				for (p = pBlk->szText; *p; y += fCode)
				{
					LPCTSTR q = p;

					while (*q && *q != _T('\n')) ++q;
					nk_draw_text(pCanvas,nk_rect(x + 8.0f,y,w - 16.0f,fCode),p,(int) (q - p),pFixed,crCodeBk,crText);
					p = *q ? q + 1 : q;
				}
			}
		}
		break;
	case BLK_ROW:							// table row, equal columns, the cells wrapped
		{
			LPCTSTR p = pBlk->szText + 1;	// after the first '|'
			TCHAR szCell[128];
			INT   i,j,nCells = 0;
			float fCell;

			for (; *p; ++p) if (*p == _T('|')) ++nCells;
			if (nCells == 0) break;
			fCell = __min(w / nCells,130.0f);
			for (j = 0; j < 2; ++j)			// height, then drawing
			{
				if (j == 1)
				{
					if (pCanvas == NULL) break;
					if (pBlk->byLevel) nk_fill_rect(pCanvas,nk_rect(x,rc.y,fCell * nCells,h),0.0f,crCodeBk);
					nk_stroke_line(pCanvas,x,rc.y + h,x + fCell * nCells,rc.y + h,1.0f,crLine);
				}
				for (p = pBlk->szText + 1, i = 0; i < nCells; ++i)
				{
					LPCTSTR q = strchr(p,_T('|'));
					INT nLen = (INT) (q - p);
					float fH;

					while (nLen > 0 && *p == _T(' ')) { ++p; --nLen; }
					while (nLen > 0 && p[nLen-1] == _T(' ')) --nLen;
					lstrcpyn(szCell,p,__min(nLen + 1,(INT) ARRAYSIZEOF(szCell)));
					fH = DrawRich(pNk,j ? pCanvas : NULL,szCell,x + i * fCell + 6.0f,rc.y + 2.0f,fCell - 10.0f,crText,pBlk->byLevel != 0);
					if (j == 0) h = __max(h,fH + 4.0f);
					p = q + 1;
				}
			}
		}
		break;
	}
	return h;
}

// first block after the chapter of block n
static INT ChapterEnd(INT n)
{
	for (++n; n < nBlocks; ++n)
		if (pBlock[n].byType == BLK_HEAD && pBlock[n].byLevel <= 2) break;
	return n;
}

// show the chapter of block n and scroll to block n, bHistory = remember the shown chapter
static VOID ShowBlock(INT n,BOOL bHistory)
{
	INT nNew = n;

	while (nNew > 0 && !(pBlock[nNew].byType == BLK_HEAD && pBlock[nNew].byLevel <= 2)) --nNew;
	if (bHistory && nNew != nChapter)
	{
		if (nHistLen == ARRAYSIZEOF(nHistory))
			MoveMemory(&nHistory[0],&nHistory[1],--nHistLen * sizeof(nHistory[0]));
		nHistory[nHistLen++] = nChapter;
	}
	nChapter = nNew;
	nScrollTo = n;
	return;
}

// a clicked link: a heading of the manual or a web page
static VOID FollowLink(LPCTSTR szTarget)
{
	INT i;

	if (szTarget[0] != _T('#'))
	{
		SDL_OpenURL(szTarget);
		return;
	}
	for (i = 0; i < nBlocks; ++i)
	{
		if (pBlock[i].byType == BLK_HEAD && lstrcmp(pBlock[i].szAnchor,szTarget + 1) == 0)
		{
			ShowBlock(i,TRUE);
			return;
		}
	}
	return;
}

static VOID DrawHelp(HWND hDlg,struct nk_context *pNk)
{
	struct nk_rect rc = nk_window_get_content_region(pNk);
	const float fRow = NkFont(hDlg,FONT_GUI)->height + 10.0f;
	float h = rc.h - fRow - 12.0f;
	INT i,nEnd;

	szLink[0] = 0;
	pNk->style.window.padding = nk_vec2(6.0f,6.0f);
	pNk->style.window.group_padding = nk_vec2(8.0f,6.0f);

	// back button
	nk_layout_row_static(pNk,fRow - 4.0f,80,1);
	if (nk_button_label(pNk,"\xE2\x86\x90 Back") && nHistLen > 0)
	{
		nChapter = nHistory[--nHistLen];
		nScrollTo = nChapter;
	}

	nk_layout_row_begin(pNk,NK_STATIC,h,2);
	nk_layout_row_push(pNk,TOPIC_WIDTH);
	if (nk_group_scrolled_offset_begin(pNk,&uTopicX,&uTopicY,"Topics",0))
	{
		nk_layout_row_dynamic(pNk,fRow - 6.0f,1);
		for (i = 0; i < nBlocks; ++i)
		{
			BLOCK_T *p = &pBlock[i];
			TCHAR szTopic[96];
			nk_bool bSel;

			// the chapters and their sections, the table of contents is the topic list
			if (p->byType != BLK_HEAD || lstrcmp(p->szAnchor,_T("contents")) == 0)
				continue;
			wsprintf(szTopic,_T("%*s%.80s"),__max(p->byLevel - 2,0) * 3,_T(""),p->szText);
			bSel = (i == nChapter);
			if (nk_selectable_label(pNk,szTopic,NK_TEXT_LEFT,&bSel))
				ShowBlock(i,TRUE);
		}
		nk_group_scrolled_end(pNk);
	}
	nk_layout_row_push(pNk,rc.w - TOPIC_WIDTH - 2.0f * pNk->style.window.spacing.x);
	if (nScrollTo >= 0)						// scroll to the block with the widths of the last frame
	{
		float y = 0.0f;

		for (i = nChapter; i < nScrollTo; ++i)
			y += DrawBlock(pNk,NULL,&pBlock[i],nk_rect(0.0f,0.0f,fTextWidth,0.0f)) + pNk->style.window.spacing.y + 4.0f;
		uTextY = (nk_uint) y;
		uTextX = 0;
		nScrollTo = -1;
	}
	if (nk_group_scrolled_offset_begin(pNk,&uTextX,&uTextY,"Text",0))
	{
		fTextWidth = nk_window_get_content_region(pNk).w - 4.0f;
		nEnd = ChapterEnd(nChapter);
		for (i = nChapter; i < nEnd; ++i)
		{
			struct nk_rect rcBlk;
			float fH = DrawBlock(pNk,NULL,&pBlock[i],nk_rect(0.0f,0.0f,fTextWidth,0.0f)) + 4.0f;

			nk_layout_row_dynamic(pNk,fH,1);
			if (nk_widget(&rcBlk,pNk) != NK_WIDGET_INVALID)
				DrawBlock(pNk,nk_window_get_canvas(pNk),&pBlock[i],rcBlk);
		}
		nk_group_scrolled_end(pNk);
	}
	nk_layout_row_end(pNk);
	if (szLink[0]) FollowLink(szLink);
	return;
}

static INT_PTR CALLBACK HelpProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		DialogDestroy(hDlg);
		return TRUE;
	case WM_COMMAND:						// Escape closes the window
		if (LOWORD(wParam) == IDCANCEL) DialogDestroy(hDlg);
		return TRUE;
	case WM_DESTROY:
		hHelp = NULL;
		return TRUE;
	}
	return FALSE;
	UNREFERENCED_PARAMETER(lParam);
}

// open the help window or bring it to the front
VOID HelpTopics(HWND hOwner)
{
	if (pBlock == NULL) ParseManual();
	if (hHelp != NULL)
	{
		ForceForegroundWindow(hHelp);
		return;
	}
	hHelp = CreateNkWindow(hOwner,_T("Emu48 Help"),720,540,HelpProc,DrawHelp);
	return;
}
