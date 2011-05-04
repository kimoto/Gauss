﻿#pragma once
#include <MMSystem.h>
#pragma comment(lib, "winmm")

#include <shlobj.h>
#pragma comment(lib, "shell32.lib")

#include <math.h>

void trace(LPCTSTR format, ...);
void FillRectBrush(HDC hdc, int x, int y, int width, int height, COLORREF color);
void BorderedRect(HDC hdc, int x, int y, int width, int height, COLORREF color);
void drawRect(HDC hdc, int x, int y, int width, int height);
void mciShowLastError(MMRESULT result);
void mciAssert(MMRESULT result);
BOOL ReadWaveFile(LPTSTR lpszFileName, LPWAVEFORMATEX lpwf, LPBYTE *lplpData, LPDWORD lpdwDataSize);
BOOL LoadBitmapFromBMPFile( LPTSTR szFileName, HBITMAP *phBitmap, HPALETTE *phPalette );
BOOL LoadBitmapToDC(LPTSTR szFileName, int x, int y, HDC hdc);
void drawRectColor(HDC hdc, int x, int y, int width, int height, COLORREF color, int bold_width);
void DrawFormatText(HDC hdc, LPRECT rect, UINT type, LPCTSTR format, ...);
void TextFormatOut(HDC hdc, int x, int y, LPCTSTR format, ...);
void mciPlayBGM(LPTSTR szFileName, double volume_scale);
void ShowLastError(void);
BOOL CreateShortcut ( LPCTSTR pszTargetPath /* ターゲットパス */,
    LPCTSTR pszArguments /* 引数 */,
    LPCTSTR pszWorkPath /* 作業ディレクトリ */,
    int nCmdShow /* ShowWindowの引数 */,
    LPCSTR pszShortcutPath /* ショートカットファイル(*.lnk)のパス */ );
double GetPrivateProfileDouble(LPCTSTR section, LPCTSTR key, double def, LPCTSTR path);
BOOL WritePrivateProfileDouble(LPCTSTR section, LPCTSTR key, double val, LPCTSTR path);
BOOL WritePrivateProfileInt(LPCTSTR section, LPCTSTR key, int val, LPCTSTR path);
LPTSTR GetKeyNameTextEx(UINT vk);
void ErrorMessageBox(LPTSTR message);
BOOL GetExecuteDirectory(LPTSTR buffer, DWORD buffer_size);
BOOL SetDlgItemDouble(HWND hWnd, UINT id, double value);
double GetDlgItemDouble(HWND hWnd, UINT id);
BOOL GetDesktopPath(LPTSTR buffer, DWORD size_in_words);
BOOL SetMonitorGamma(HDC hdc, double gammaR, double gammaG, double gammaB);
BOOL SetMonitorGamma(HDC hdc, double gamma);
BOOL SetGamma(double gammaR, double gammaG, double gammaB);
BOOL SetGamma(double gamma);
BOOL SetWindowTopMost(HWND hWnd);
