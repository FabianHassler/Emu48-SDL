/*
 *   SdlGdi.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Win32 GDI subset on memory bitmaps
//
// Bitmaps created with CreateDIBSection() keep the caller's pixel format,
// all other bitmaps use 32 bit top down pixels (B,G,R,X). The window DC
// is a memory DC, its bitmap is shown in the window by SdlMain.c.

#include "pch.h"
#include "Emu48.h"
#include "sdlmain.h"

#define WIDTHBYTES(bits)	((((bits) + 31) / 32) * 4)

enum GDITYPE { G_BITMAP, G_PALETTE, G_REGION, G_FONT, G_STOCK };

struct _SDLGDIOBJ
{
	enum GDITYPE eType;

	// G_BITMAP
	INT     nWidth;
	INT     nHeight;
	INT     nBitCount;						// 1, 4, 8, 24 or 32
	INT     nStride;						// bytes per row
	BOOL    bBottomUp;						// row order of DIB
	LPBYTE  pbyBits;
	BOOL    bOwnBits;
	RGBQUAD sColors[256];					// color table for <= 8 bit

	// G_PALETTE
	UINT    nEntries;

	// G_REGION
	UINT    nRects;
	RECT    *pRects;

	// G_FONT
	INT     nFont;							// FONT_GUI or FONT_FIXED, FONT_BOLD

	// G_STOCK
	INT     nStock;
};

struct _SDLDC
{
	HBITMAP hBitmap;						// selected bitmap
	POINT   ptOrg;							// window origin
	POINT   ptPen;							// current pen position
	DWORD   dwPenColor;						// 0x00RRGGBB
	HWND    hWnd;							// window drawn with Nuklear, NULL = memory DC
	HFONT   hFont;							// selected font, NULL = window font
	COLORREF crText,crBk;
};

static struct _SDLGDIOBJ sStock[20];		// stock objects

//################
//#
//#    Pixel access
//#
//################

static __inline LPBYTE PixelRow(HBITMAP hBmp,INT y)
{
	if (hBmp->bBottomUp) y = hBmp->nHeight - 1 - y;
	return hBmp->pbyBits + y * hBmp->nStride;
}

// get pixel as 0x00RRGGBB
static DWORD GetPixel32(HBITMAP hBmp,INT x,INT y)
{
	LPBYTE p = PixelRow(hBmp,y);
	RGBQUAD c;

	switch (hBmp->nBitCount)
	{
	case 32: p += x * 4; return (p[2] << 16) | (p[1] << 8) | p[0];
	case 24: p += x * 3; return (p[2] << 16) | (p[1] << 8) | p[0];
	case 8:  c = hBmp->sColors[p[x]]; break;
	case 4:  c = hBmp->sColors[(p[x/2] >> ((x & 1) ? 0 : 4)) & 0xF]; break;
	case 1:  c = hBmp->sColors[(p[x/8] >> (7 - (x & 7))) & 0x1]; break;
	default: return 0;
	}
	return (c.rgbRed << 16) | (c.rgbGreen << 8) | c.rgbBlue;
}

// set pixel from 0x00RRGGBB, only for true color bitmaps
static VOID SetPixel32(HBITMAP hBmp,INT x,INT y,DWORD dwColor)
{
	LPBYTE p = PixelRow(hBmp,y) + x * (hBmp->nBitCount / 8);

	_ASSERT(hBmp->nBitCount == 32 || hBmp->nBitCount == 24);
	p[0] = (BYTE) dwColor;
	p[1] = (BYTE) (dwColor >> 8);
	p[2] = (BYTE) (dwColor >> 16);
	return;
}

//################
//#
//#    Objects
//#
//################

// no. of colors in the color table of a DIB
UINT DibColors(const BITMAPINFOHEADER *pbmih)
{
	if (pbmih->biBitCount > 8) return 0;
	return pbmih->biClrUsed ? pbmih->biClrUsed : (1 << pbmih->biBitCount);
}

// describe the pixels of a bitmap, also of a DIB in memory
static VOID SetBitmap(HBITMAP hBmp,INT nWidth,INT nHeight,INT nBitCount,BOOL bBottomUp,LPBYTE pbyBits)
{
	ZeroMemory(hBmp,sizeof(*hBmp));
	hBmp->eType = G_BITMAP;
	hBmp->nWidth = nWidth;
	hBmp->nHeight = nHeight;
	hBmp->nBitCount = nBitCount;
	hBmp->nStride = WIDTHBYTES(nWidth * nBitCount);
	hBmp->bBottomUp = bBottomUp;
	hBmp->pbyBits = pbyBits;
	return;
}

// bitmap with own pixel buffer
static HBITMAP NewBitmap(INT nWidth,INT nHeight,INT nBitCount,BOOL bBottomUp)
{
	HBITMAP hBmp = (HBITMAP) malloc(sizeof(*hBmp));
	if (hBmp == NULL) return NULL;

	SetBitmap(hBmp,nWidth,nHeight,nBitCount,bBottomUp,NULL);
	hBmp->bOwnBits = TRUE;
	if ((hBmp->pbyBits = (LPBYTE) calloc(hBmp->nStride,nHeight ? nHeight : 1)) == NULL)
	{
		free(hBmp);
		return NULL;
	}
	return hBmp;
}

HBITMAP CreateCompatibleBitmap(HDC hdc,INT cx,INT cy)
{
	return NewBitmap(cx,cy,32,FALSE);
	UNREFERENCED_PARAMETER(hdc);
}

HBITMAP CreateDIBSection(HDC hdc,const BITMAPINFO *pbmi,UINT usage,VOID **ppvBits,HANDLE hSection,DWORD offset)
{
	const BITMAPINFOHEADER *pbmih = &pbmi->bmiHeader;
	HBITMAP hBmp;

	_ASSERT(pbmih->biCompression == BI_RGB);
	hBmp = NewBitmap(pbmih->biWidth,abs(pbmih->biHeight),pbmih->biBitCount,pbmih->biHeight > 0);
	if (hBmp == NULL) return NULL;

	CopyMemory(hBmp->sColors,pbmi->bmiColors,DibColors(pbmih) * sizeof(RGBQUAD));
	*ppvBits = hBmp->pbyBits;
	return hBmp;
	UNREFERENCED_PARAMETER(hdc);
	UNREFERENCED_PARAMETER(usage);
	UNREFERENCED_PARAMETER(hSection);
	UNREFERENCED_PARAMETER(offset);
}

// decode a RLE8 or RLE4 bitmap into a bottom up 8 bit DIB
static LPBYTE DecodeRle(const BITMAPINFOHEADER *pbmih,LPBYTE pbySrc)
{
	INT    nWidth  = pbmih->biWidth;
	INT    nHeight = abs(pbmih->biHeight);
	INT    nStride = WIDTHBYTES(nWidth * 8);
	BOOL   bRle4   = (pbmih->biCompression == BI_RLE4);
	LPBYTE pbyEnd  = pbySrc + pbmih->biSizeImage;
	LPBYTE pbyDest;
	INT    x = 0, y = 0, i;

	if ((pbyDest = (LPBYTE) calloc(nStride,nHeight)) == NULL) return NULL;

	while (pbySrc + 1 < pbyEnd && y < nHeight)
	{
		BYTE byCount = *pbySrc++;
		BYTE byValue = *pbySrc++;

		if (byCount > 0)					// encoded run
		{
			for (i = 0; i < byCount; ++i, ++x)
			{
				if (x < nWidth)
					pbyDest[y * nStride + x] = bRle4 ? ((i & 1) ? (byValue & 0xF) : (byValue >> 4)) : byValue;
			}
		}
		else if (byValue == 0)				// end of line
		{
			x = 0; ++y;
		}
		else if (byValue == 1)				// end of bitmap
		{
			break;
		}
		else if (byValue == 2)				// delta
		{
			if (pbySrc + 1 >= pbyEnd) break;
			x += *pbySrc++;
			y += *pbySrc++;
		}
		else								// absolute run of byValue pixels
		{
			INT nBytes = bRle4 ? (byValue + 1) / 2 : byValue;

			if (pbySrc + nBytes > pbyEnd) break;
			for (i = 0; i < byValue; ++i, ++x)
			{
				BYTE byPixel = bRle4 ? ((i & 1) ? (pbySrc[i/2] & 0xF) : (pbySrc[i/2] >> 4)) : pbySrc[i];
				if (x < nWidth) pbyDest[y * nStride + x] = byPixel;
			}
			pbySrc += (nBytes + 1) & ~1;	// word aligned
		}
	}
	return pbyDest;
}

// create a 32 bit device bitmap from DIB data
HBITMAP CreateDIBitmap(HDC hdc,const BITMAPINFOHEADER *pbmih,DWORD flInit,const VOID *pjBits,const BITMAPINFO *pbmi,UINT iUsage)
{
	struct _SDLGDIOBJ sDib;
	HBITMAP hBmp;
	INT x,y;

	if (pbmih->biCompression != BI_RGB && pbmih->biCompression != BI_RLE8 && pbmih->biCompression != BI_RLE4)
		return NULL;

	hBmp = NewBitmap(pbmih->biWidth,abs(pbmih->biHeight),32,FALSE);
	if (hBmp == NULL || (flInit & CBM_INIT) == 0) return hBmp;

	// describe the source DIB, RLE decoded to an 8 bit DIB
	if (pbmih->biCompression == BI_RGB)
		SetBitmap(&sDib,pbmih->biWidth,abs(pbmih->biHeight),pbmih->biBitCount,pbmih->biHeight > 0,(LPBYTE) pjBits);
	else
		SetBitmap(&sDib,pbmih->biWidth,abs(pbmih->biHeight),8,TRUE,DecodeRle(pbmih,(LPBYTE) pjBits));
	if (sDib.pbyBits == NULL)
	{
		DeleteObject(hBmp);
		return NULL;
	}
	CopyMemory(sDib.sColors,pbmi->bmiColors,DibColors(pbmih) * sizeof(RGBQUAD));

	for (y = 0; y < hBmp->nHeight; ++y)
		for (x = 0; x < hBmp->nWidth; ++x)
			SetPixel32(hBmp,x,y,GetPixel32(&sDib,x,y));
	if (sDib.pbyBits != pjBits) free(sDib.pbyBits);
	return hBmp;
	UNREFERENCED_PARAMETER(hdc);
	UNREFERENCED_PARAMETER(iUsage);
}

// get bitmap information or the pixels as 24 or 32 bit bottom up DIB
INT GetDIBits(HDC hdc,HBITMAP hbm,UINT start,UINT cLines,LPVOID lpvBits,LPBITMAPINFO lpbmi,UINT usage)
{
	LPBITMAPINFOHEADER pbmih = &lpbmi->bmiHeader;
	struct _SDLGDIOBJ sDib;
	INT x,y;

	if (lpvBits == NULL)					// query bitmap information
	{
		pbmih->biWidth = hbm->nWidth;
		pbmih->biHeight = hbm->nHeight;
		pbmih->biPlanes = 1;
		pbmih->biBitCount = 32;
		pbmih->biCompression = BI_RGB;
		pbmih->biSizeImage = WIDTHBYTES(hbm->nWidth * 32) * hbm->nHeight;
		return hbm->nHeight;
	}

	// describe the destination DIB
	SetBitmap(&sDib,hbm->nWidth,hbm->nHeight,pbmih->biBitCount,TRUE,(LPBYTE) lpvBits);

	for (y = (INT) start; y < (INT) (start + cLines) && y < hbm->nHeight; ++y)
		for (x = 0; x < hbm->nWidth; ++x)
			SetPixel32(&sDib,x,hbm->nHeight - 1 - y,GetPixel32(hbm,x,hbm->nHeight - 1 - y));
	return (INT) cLines;
	UNREFERENCED_PARAMETER(hdc);
	UNREFERENCED_PARAMETER(usage);
}

UINT SetDIBColorTable(HDC hdc,UINT iStart,UINT cEntries,const RGBQUAD *prgbq)
{
	HBITMAP hBmp = hdc->hBitmap;

	if (hBmp == NULL || iStart >= 256) return 0;
	if (iStart + cEntries > 256) cEntries = 256 - iStart;
	CopyMemory(&hBmp->sColors[iStart],prgbq,cEntries * sizeof(RGBQUAD));
	return cEntries;
}

HPALETTE CreatePalette(const LOGPALETTE *plpal)
{
	HPALETTE hPal = (HPALETTE) calloc(1,sizeof(*hPal));
	if (hPal == NULL) return NULL;
	hPal->eType = G_PALETTE;
	hPal->nEntries = plpal->palNumEntries;
	return hPal;
}

UINT GetPaletteEntries(HPALETTE hpal,UINT iStart,UINT cEntries,LPPALETTEENTRY pPalEntries)
{
	ZeroMemory(pPalEntries,cEntries * sizeof(*pPalEntries));
	return cEntries;
	UNREFERENCED_PARAMETER(hpal);
	UNREFERENCED_PARAMETER(iStart);
}

HRGN ExtCreateRegion(const XFORM *lpx,DWORD nCount,const RGNDATA *lpData)
{
	HRGN hRgn = (HRGN) calloc(1,sizeof(*hRgn));
	if (hRgn == NULL) return NULL;
	hRgn->eType = G_REGION;
	hRgn->nRects = lpData->rdh.nCount;
	hRgn->pRects = (RECT *) malloc(hRgn->nRects * sizeof(RECT));
	if (hRgn->pRects) CopyMemory(hRgn->pRects,lpData->Buffer,hRgn->nRects * sizeof(RECT));
	return hRgn;
	UNREFERENCED_PARAMETER(lpx);
	UNREFERENCED_PARAMETER(nCount);
}

INT OffsetRgn(HRGN hrgn,INT x,INT y)
{
	UINT i;

	for (i = 0; i < hrgn->nRects; ++i)
	{
		hrgn->pRects[i].left += x;
		hrgn->pRects[i].right += x;
		hrgn->pRects[i].top += y;
		hrgn->pRects[i].bottom += y;
	}
	return 2;								// SIMPLEREGION
}

// get the rectangles of a region
UINT GetRegionRects(HRGN hRgn,const RECT **ppRects)
{
	*ppRects = hRgn->pRects;
	return hRgn->nRects;
}

HGDIOBJ GetStockObject(INT i)
{
	_ASSERT(i >= 0 && i < (INT) ARRAYSIZEOF(sStock));
	sStock[i].eType = G_STOCK;
	sStock[i].nStock = i;
	return &sStock[i];
}

BOOL DeleteObject(HGDIOBJ ho)
{
	if (ho == NULL || ho->eType == G_STOCK) return FALSE;
	if (ho->bOwnBits) free(ho->pbyBits);
	free(ho->pRects);
	free(ho);
	return TRUE;
}

INT GetObject(HGDIOBJ h,INT c,LPVOID pv)
{
	if (h == NULL) return 0;				// no palette on true color displays
	if (h->eType == G_BITMAP)
	{
		BITMAP *pBm = (BITMAP *) pv;
		ZeroMemory(pBm,sizeof(*pBm));
		pBm->bmWidth = h->nWidth;
		pBm->bmHeight = h->nHeight;
		pBm->bmWidthBytes = h->nStride;
		pBm->bmPlanes = 1;
		pBm->bmBitsPixel = (WORD) h->nBitCount;
		pBm->bmBits = h->pbyBits;
		return sizeof(*pBm);
	}
	if (h->eType == G_PALETTE)
	{
		*(WORD *) pv = (WORD) h->nEntries;	// no. of palette entries
		return sizeof(WORD);
	}
	return 0;
	UNREFERENCED_PARAMETER(c);
}

//################
//#
//#    Device contexts
//#
//################

HDC CreateCompatibleDC(HDC hdc)
{
	HDC hNewDC = (HDC) calloc(1,sizeof(struct _SDLDC));
	hNewDC->crBk = RGB(255,255,255);
	return hNewDC;
	UNREFERENCED_PARAMETER(hdc);
}

// DC of a window drawn with Nuklear for text output and bitmaps
HDC GetDC(HWND hWnd)
{
	HDC hDC = CreateCompatibleDC(NULL);
	hDC->hWnd = hWnd;
	return hDC;
}

BOOL DeleteDC(HDC hdc)
{
	free(hdc);
	return TRUE;
}

HGDIOBJ SelectObject(HDC hdc,HGDIOBJ h)
{
	HGDIOBJ hOld = NULL;

	if (h == NULL)							// select default bitmap of a new DC
	{
		hOld = hdc->hBitmap;
		hdc->hBitmap = NULL;
		return hOld;
	}

	if (h->eType == G_FONT || (h->eType == G_STOCK && (h->nStock == DEFAULT_GUI_FONT || h->nStock == SYSTEM_FONT)))
	{
		hOld = hdc->hFont ? (HGDIOBJ) hdc->hFont : GetStockObject(SYSTEM_FONT);
		hdc->hFont = (h->eType == G_STOCK && h->nStock == SYSTEM_FONT) ? NULL : (HFONT) h;
		return hOld;
	}

	switch (h->eType)
	{
	case G_BITMAP:
		hOld = hdc->hBitmap;
		hdc->hBitmap = h;
		break;
	case G_STOCK:							// pens
		hdc->dwPenColor = (h->nStock == WHITE_PEN) ? 0x00FFFFFF : 0x00000000;
		break;
	default:
		break;
	}
	return hOld;
}

HGDIOBJ GetCurrentObject(HDC hdc,UINT type)
{
	return (type == OBJ_BITMAP) ? hdc->hBitmap : NULL;
}

BOOL SetWindowOrgEx(HDC hdc,INT x,INT y,LPPOINT lppt)
{
	if (lppt) *lppt = hdc->ptOrg;
	hdc->ptOrg.x = x;
	hdc->ptOrg.y = y;
	return TRUE;
}

COLORREF SetTextColor(HDC hdc,COLORREF color)
{
	COLORREF crOld = hdc->crText;
	hdc->crText = color;
	return crOld;
}

COLORREF SetBkColor(HDC hdc,COLORREF color)
{
	COLORREF crOld = hdc->crBk;
	hdc->crBk = color;
	return crOld;
}

COLORREF GetTextColor(HDC hdc)
{
	return hdc->crText;
}

COLORREF GetBkColor(HDC hdc)
{
	return hdc->crBk;
}

DWORD GetSysColor(INT nIndex)
{
	switch (nIndex)
	{
	case COLOR_HIGHLIGHT:     return RGB(0,120,215);
	case COLOR_HIGHLIGHTTEXT: return RGB(255,255,255);
	}
	return RGB(240,240,240);				// COLOR_BTNFACE
}

// only the window font in normal or bold and fixed pitch fonts are available
HFONT CreateFont(INT cHeight,INT cWidth,INT cEscapement,INT cOrientation,INT cWeight,DWORD bItalic,DWORD bUnderline,DWORD bStrikeOut,DWORD iCharSet,DWORD iOutPrecision,DWORD iClipPrecision,DWORD iQuality,DWORD iPitchAndFamily,LPCTSTR pszFaceName)
{
	HGDIOBJ hFont = (HGDIOBJ) calloc(1,sizeof(*hFont));

	hFont->eType = G_FONT;
	hFont->nFont = (lstrcmpi(pszFaceName,_T("Courier New")) == 0) ? FONT_FIXED : FONT_GUI;
	if (cWeight >= FW_BOLD) hFont->nFont |= FONT_BOLD;
	return (HFONT) hFont;
	UNREFERENCED_PARAMETER(cHeight);
	UNREFERENCED_PARAMETER(cWidth);
	UNREFERENCED_PARAMETER(cEscapement);
	UNREFERENCED_PARAMETER(cOrientation);
	UNREFERENCED_PARAMETER(bItalic);
	UNREFERENCED_PARAMETER(bUnderline);
	UNREFERENCED_PARAMETER(bStrikeOut);
	UNREFERENCED_PARAMETER(iCharSet);
	UNREFERENCED_PARAMETER(iOutPrecision);
	UNREFERENCED_PARAMETER(iClipPrecision);
	UNREFERENCED_PARAMETER(iQuality);
	UNREFERENCED_PARAMETER(iPitchAndFamily);
}

// font of a font handle, -1 = window font
INT GetFontType(HFONT hFont)
{
	HGDIOBJ h = (HGDIOBJ) hFont;

	if (h == NULL) return -1;
	return (h->eType == G_FONT) ? h->nFont : FONT_GUI; // DEFAULT_GUI_FONT
}

BOOL GetTextMetrics(HDC hdc,TEXTMETRIC *lptm)
{
	return NkTextMetrics(hdc->hWnd,GetFontType(hdc->hFont),lptm);
}

BOOL ExtTextOut(HDC hdc,INT x,INT y,UINT options,const RECT *lprect,LPCTSTR lpString,UINT c,const INT *lpDx)
{
	return NkTextOut(hdc->hWnd,GetFontType(hdc->hFont),hdc->crText,(options & ETO_OPAQUE) ? hdc->crBk : (COLORREF) -1,
					 x,y,lprect,lpString,c);
	UNREFERENCED_PARAMETER(lpDx);
}

BOOL DrawFocusRect(HDC hdc,const RECT *lprc)
{
	return NkFocusRect(hdc->hWnd,lprc);
}

//################
//#
//#    Drawing
//#
//################

BOOL StretchBlt(HDC hdcDest,INT xDest,INT yDest,INT wDest,INT hDest,HDC hdcSrc,INT xSrc,INT ySrc,INT wSrc,INT hSrc,DWORD rop)
{
	HBITMAP hDst = hdcDest->hBitmap;
	HBITMAP hSrcBmp = hdcSrc->hBitmap;
	INT x,y;

	_ASSERT(rop == SRCCOPY);
	if (hDst == NULL || hSrcBmp == NULL || wDest <= 0 || hDest <= 0 || wSrc <= 0 || hSrc <= 0)
		return FALSE;

	xDest -= hdcDest->ptOrg.x; yDest -= hdcDest->ptOrg.y;
	xSrc -= hdcSrc->ptOrg.x;   ySrc -= hdcSrc->ptOrg.y;

	for (y = 0; y < hDest; ++y)
	{
		INT yd = yDest + y;
		INT ys = ySrc + (INT) (((LONGLONG) y * hSrc) / hDest);

		if (yd < 0 || yd >= hDst->nHeight || ys < 0 || ys >= hSrcBmp->nHeight) continue;

		for (x = 0; x < wDest; ++x)
		{
			INT xd = xDest + x;
			INT xs = xSrc + (INT) (((LONGLONG) x * wSrc) / wDest);

			if (xd < 0 || xd >= hDst->nWidth || xs < 0 || xs >= hSrcBmp->nWidth) continue;
			SetPixel32(hDst,xd,yd,GetPixel32(hSrcBmp,xs,ys));
		}
	}
	return TRUE;
	UNREFERENCED_PARAMETER(rop);
}

BOOL BitBlt(HDC hdc,INT x,INT y,INT cx,INT cy,HDC hdcSrc,INT x1,INT y1,DWORD rop)
{
	if (hdc->hWnd) return NkBitBlt(hdc->hWnd,x,y,cx,cy,hdcSrc->hBitmap,x1,y1);
	return StretchBlt(hdc,x,y,cx,cy,hdcSrc,x1,y1,cx,cy,rop);
}

BOOL PatBlt(HDC hdc,INT x,INT y,INT w,INT h,DWORD rop)
{
	HBITMAP hBmp = hdc->hBitmap;
	INT i,j;

	if (hBmp == NULL) return FALSE;
	x -= hdc->ptOrg.x; y -= hdc->ptOrg.y;

	for (j = __max(y,0); j < y + h && j < hBmp->nHeight; ++j)
	{
		for (i = __max(x,0); i < x + w && i < hBmp->nWidth; ++i)
		{
			if (rop == DSTINVERT)
				SetPixel32(hBmp,i,j,~GetPixel32(hBmp,i,j) & 0x00FFFFFF);
			else if (rop == BLACKNESS)
				SetPixel32(hBmp,i,j,0x00000000);
			else if (rop == WHITENESS)
				SetPixel32(hBmp,i,j,0x00FFFFFF);
		}
	}
	return TRUE;
}

BOOL MoveToEx(HDC hdc,INT x,INT y,LPPOINT lppt)
{
	if (lppt) *lppt = hdc->ptPen;
	hdc->ptPen.x = x;
	hdc->ptPen.y = y;
	return TRUE;
}

// draw horizontal or vertical line without end point
BOOL LineTo(HDC hdc,INT x,INT y)
{
	HBITMAP hBmp = hdc->hBitmap;
	INT x0 = hdc->ptPen.x, y0 = hdc->ptPen.y;
	INT dx = (x > x0) - (x < x0), dy = (y > y0) - (y < y0);

	_ASSERT(dx == 0 || dy == 0);			// only horizontal and vertical lines
	for (; x0 != x || y0 != y; x0 += dx, y0 += dy)
	{
		INT xd = x0 - hdc->ptOrg.x, yd = y0 - hdc->ptOrg.y;
		if (hBmp && xd >= 0 && xd < hBmp->nWidth && yd >= 0 && yd < hBmp->nHeight)
			SetPixel32(hBmp,xd,yd,hdc->dwPenColor);
		if (dx == 0 && dy == 0) break;
	}
	hdc->ptPen.x = x;
	hdc->ptPen.y = y;
	return TRUE;
}

// get the pixel buffer of a 32 bit top down bitmap
LPBYTE GetBitmapBits(HBITMAP hBmp,INT *pnWidth,INT *pnHeight,INT *pnStride)
{
	_ASSERT(hBmp->nBitCount == 32 && !hBmp->bBottomUp);
	*pnWidth = hBmp->nWidth;
	*pnHeight = hBmp->nHeight;
	*pnStride = hBmp->nStride;
	return hBmp->pbyBits;
}
