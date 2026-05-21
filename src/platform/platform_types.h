#ifndef WORLD_SIM_PLATFORM_TYPES_H
#define WORLD_SIM_PLATFORM_TYPES_H

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>

typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint32_t COLORREF;
typedef unsigned int UINT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef int BOOL;
typedef wchar_t WCHAR;
typedef void *HWND;
typedef struct SdlGdiContext *HDC;
typedef struct SdlGdiObject *HGDIOBJ;
typedef struct SdlGdiObject *HBRUSH;
typedef struct SdlGdiObject *HPEN;
typedef struct SdlGdiObject *HFONT;
typedef struct SdlGdiObject *HBITMAP;
typedef struct SdlGdiObject *HRGN;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} RECT;

typedef struct {
    int x;
    int y;
} POINT;

typedef struct {
    int cx;
    int cy;
} SIZE;

typedef struct {
    DWORD BlendOp;
    DWORD BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
} BLENDFUNCTION;

typedef struct {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
    BITMAPINFOHEADER bmiHeader;
} BITMAPINFO;

static inline void SetRectEmpty(RECT *rect) {
    if (!rect) return;
    rect->left = rect->top = rect->right = rect->bottom = 0;
}

static inline int IsRectEmpty(const RECT *rect) {
    return !rect || rect->right <= rect->left || rect->bottom <= rect->top;
}

#define RGB(r, g, b) ((COLORREF)(((uint8_t)(r)) | ((uint32_t)((uint8_t)(g)) << 8) | ((uint32_t)((uint8_t)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)((rgb) & 0xff))
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xff))
#define GetBValue(rgb) ((BYTE)(((rgb) >> 16) & 0xff))
#define TRUE 1
#define FALSE 0
#define CP_UTF8 65001
#define MAX_PATH 260
#define TRANSPARENT 1
#define OPAQUE 2
#define SRCCOPY 0x00cc0020u
#define BI_RGB 0
#define DIB_RGB_COLORS 0
#define AC_SRC_OVER 0
#define HALFTONE 4
#define COLORONCOLOR 3
#define PS_SOLID 0
#define FW_NORMAL 400
#define FW_BOLD 700
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define DEFAULT_PITCH 0
#define FF_SWISS 32
#define NULL_BRUSH 5
#define GRAY_BRUSH 2
#define MB_OK 0
#define MB_ICONINFORMATION 0x40
#define MB_ICONERROR 0x10
#define VK_LBUTTON 0x01
#define DT_LEFT 0x0000
#define DT_CENTER 0x0001
#define DT_RIGHT 0x0002
#define DT_VCENTER 0x0004
#define DT_SINGLELINE 0x0020
#define DT_WORDBREAK 0x0010
#define DT_CALCRECT 0x0400
#define DT_END_ELLIPSIS 0x8000
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

static inline DWORD GetTickCount(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static inline void OutputDebugStringA(const char *text) {
    if (text) fputs(text, stderr);
}

int InflateRect(RECT *rect, int dx, int dy);
HBRUSH CreateSolidBrush(COLORREF color);
HPEN CreatePen(int style, int width, COLORREF color);
HFONT CreateFontW(int height, int width, int escapement, int orientation, int weight,
                  BOOL italic, BOOL underline, BOOL strikeout, DWORD charset,
                  DWORD out_precision, DWORD clip_precision, DWORD quality,
                  DWORD pitch_and_family, const WCHAR *face_name);
HDC CreateCompatibleDC(HDC hdc);
HBITMAP CreateCompatibleBitmap(HDC hdc, int width, int height);
HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO *info, UINT usage,
                         void **bits, void *section, DWORD offset);
HRGN CreateRectRgn(int left, int top, int right, int bottom);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ object);
HGDIOBJ GetStockObject(int object_id);
int DeleteObject(HGDIOBJ object);
int DeleteDC(HDC hdc);
int FillRect(HDC hdc, const RECT *rect, HBRUSH brush);
int FrameRect(HDC hdc, const RECT *rect, HBRUSH brush);
int SetBkMode(HDC hdc, int mode);
COLORREF SetTextColor(HDC hdc, COLORREF color);
int TextOutW(HDC hdc, int x, int y, const WCHAR *text, int len);
int DrawTextW(HDC hdc, const WCHAR *text, int len, RECT *rect, UINT format);
int DrawTextA(HDC hdc, const char *text, int len, RECT *rect, UINT format);
int GetTextExtentPoint32W(HDC hdc, const WCHAR *text, int len, SIZE *size);
int MultiByteToWideChar(UINT code_page, DWORD flags, const char *src, int src_len,
                        WCHAR *dst, int dst_len);
int BitBlt(HDC dst, int x, int y, int width, int height, HDC src, int sx, int sy, DWORD rop);
int TransparentBlt(HDC dst, int x, int y, int width, int height, HDC src,
                   int sx, int sy, int sw, int sh, COLORREF transparent);
int AlphaBlend(HDC dst, int x, int y, int width, int height, HDC src,
               int sx, int sy, int sw, int sh, BLENDFUNCTION blend);
int SaveDC(HDC hdc);
int RestoreDC(HDC hdc, int saved);
int IntersectClipRect(HDC hdc, int left, int top, int right, int bottom);
int SelectClipRgn(HDC hdc, HRGN region);
int SetStretchBltMode(HDC hdc, int mode);
int SetBrushOrgEx(HDC hdc, int x, int y, void *point);
int MoveToEx(HDC hdc, int x, int y, POINT *old_point);
int LineTo(HDC hdc, int x, int y);
int Polyline(HDC hdc, const POINT *points, int count);
int Rectangle(HDC hdc, int left, int top, int right, int bottom);
int Ellipse(HDC hdc, int left, int top, int right, int bottom);
BOOL InvalidateRect(HWND hwnd, const RECT *rect, BOOL erase);
HWND SetCapture(HWND hwnd);
BOOL ReleaseCapture(void);
int MessageBoxW(HWND hwnd, const WCHAR *text, const WCHAR *title, UINT flags);
short GetKeyState(int key);

#endif

#endif
