#ifndef PAL_FONT_H
#define PAL_FONT_H

#include "win.h"

#ifdef __cplusplus
extern "C" {
#endif

HFONT PAL_CreateFont(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight,
                     DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet,
                     DWORD fdwOutputPrecision, DWORD fdwClipPrecision, DWORD fdwQuality,
                     DWORD fdwPitchAndFamily, LPCSTR lpszFace);

HDC PAL_CreateCompatibleDC(HDC hdc);

HBITMAP PAL_CreateDIBSection(HDC hdc, const BITMAPINFO *pbmi, UINT usage, void **ppvBits,
                            HANDLE hSection, DWORD offset);

HGDIOBJ PAL_SelectObject(HDC hdc, HGDIOBJ hgdiobj);

BOOL PAL_DeleteObject(HGDIOBJ hObject);

BOOL PAL_DeleteDC(HDC hdc);

BOOL PAL_GetTextMetrics(HDC hdc, TEXTMETRICA *tm);

BOOL PAL_GetTextExtentPoint32W(HDC hdc, const wchar_t *lpString, int c, SIZE *psz);

BOOL PAL_ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT *lprc,
                     const wchar_t *lpString, UINT c, const int *lpDx);

#ifdef __cplusplus
}
#endif

#endif // PAL_FONT_H
