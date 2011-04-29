// GanmaChanger.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "Gauss.h"
#include <Windows.h>
#include <ShellAPI.h>
#include <math.h>
#include "resource.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <shlobj.h>
#pragma comment(lib, "shell32.lib")

#include <CommCtrl.h>
#pragma comment(lib, "ComCtl32.lib")

#pragma comment(lib, "KeyHook.lib")
__declspec(dllimport) BOOL SetWindowHandle(HWND);
__declspec(dllimport) BOOL StartKeyHook(int prevKey, int nextKey, int resetKey, int optKey);
__declspec(dllimport) BOOL RestartHook(void);
__declspec(dllimport) BOOL StopHook(void);
__declspec(dllimport) BOOL isHook(void);

#define MUTEX_NAME L"Gauss"

#define MAX_LOADSTRING 100
#define WM_USER_MESSAGE (WM_USER+0x1000)

#define GAMMA_INCREMENT_VALUE 0.1
#define GAMMA_DECREMENT_VALUE 0.1
#define GAMMA_DEFAULT_VALUE 1.0
#define GAMMA_MIN_VALUE 0.0
#define GAMMA_MAX_VALUE 5.0

typedef struct {
	double r;
	double g;
	double b;

	double level;

	HDC hDC;
	UINT monitorID;

	LPWSTR monitorName;
	LPWSTR deviceName;
} MonitorInfo;

HINSTANCE hInst;								// 現在のインターフェイス
TCHAR szTitle[MAX_LOADSTRING];					// タイトル バーのテキスト
TCHAR szWindowClass[MAX_LOADSTRING];			// メイン ウィンドウ クラス名
HANDLE hMutex;
NOTIFYICONDATA nid = { 0 };
int g_Gamma = 128;
double g_DoubleGamma = 1.0;
bool bStopedFlag = false;

double g_gammaR = 1.0;
double g_gammaG = 1.0;
double g_gammaB = 1.0;

int g_hMonitorTargetIndex;
MonitorInfo monitorInfoList[256];

HWND g_hDlg;
HWND g_hGammaDlg;
HWND g_hMonitorGammaDlg;
HWND g_hKeyConfigDlg;
int g_brightnessLevel = 128;

HDC g_deviceContexts[256];
LPWSTR g_deviceNames[256];
int g_deviceContextCounter = 0;

int g_lightUpKey = VK_PRIOR;
int g_lightDownKey = VK_NEXT;
int g_lightResetKey = VK_HOME;
int g_lightOptKey = VK_CONTROL;

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void ShowLastError(void){
	LPVOID lpMessageBuffer;
  
	FormatMessage(
	FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	GetLastError(),
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // デフォルト ユーザー言語 
	(LPTSTR) &lpMessageBuffer,
	0,
	NULL );

	MessageBox(NULL, (LPCWSTR)lpMessageBuffer, TEXT("Error"), MB_OK);
  
	//... 文字列が表示されます。
	// システムによって確保されたバッファを開放します。
	LocalFree( lpMessageBuffer );
}

// ショートカット作成
BOOL CreateShortcut ( LPCTSTR pszTargetPath /* ターゲットパス */,
    LPCTSTR pszArguments /* 引数 */,
    LPCTSTR pszWorkPath /* 作業ディレクトリ */,
    int nCmdShow /* ShowWindowの引数 */,
    LPCSTR pszShortcutPath /* ショートカットファイル(*.lnk)のパス */ )
{
    IShellLink *psl = NULL;
    IPersistFile *ppf = NULL;
    enum
    {
        MY_MAX_PATH = 65536
    };
    TCHAR wcLink[ MY_MAX_PATH ]=_T("");

    // IShellLinkインターフェースの作成
    HRESULT result = CoCreateInstance( CLSID_ShellLink, NULL,CLSCTX_INPROC_SERVER, IID_IShellLink, ( void ** ) &psl);
	if(FAILED(result))
    {
		return result;
	}

    // 設定
    psl->SetPath ( pszTargetPath );
    psl->SetArguments ( pszArguments );
    psl->SetWorkingDirectory ( pszWorkPath );
    psl->SetShowCmd ( nCmdShow );

    // IPersistFileインターフェースの作成
    if ( FAILED ( psl->QueryInterface ( IID_IPersistFile, ( void ** ) &ppf ) ) )
    {
        psl->Release ();
        return FALSE;
    }
    
    // lpszLinkをWCHAR型に変換
    MultiByteToWideChar ( CP_ACP, 0, pszShortcutPath, -1, wcLink, MY_MAX_PATH );
    if ( FAILED ( ppf->Save ( wcLink, TRUE ) ) )
    {
        ppf->Release ();
        return FALSE;
    }

	result = ppf->Save((LPCOLESTR)pszShortcutPath,TRUE);
	
    // 解放
    ppf->Release ();
    psl->Release ();

    return TRUE;
}

void OutputDebugStringFormat(LPCTSTR format, LPVOID any)
{
	LPVOID lpMessageBuffer;
  
	FormatMessage(
	FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_STRING,
	format,
	GetLastError(),
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // デフォルト ユーザー言語 
	(LPTSTR) &lpMessageBuffer,
	0,
	NULL );

	::OutputDebugString((LPCWSTR)lpMessageBuffer);

	//... 文字列が表示されます。
	// システムによって確保されたバッファを開放します。
	LocalFree( lpMessageBuffer );
}

void CheckError(BOOL b)
{
	if(!b) ShowLastError();
}

BOOL SetGammaCorrectMonitorRGB(HDC hdc, double gammaR, double gammaG, double gammaB)
{
	gammaR = 1.0 / gammaR;
	gammaG = 1.0 / gammaG;
	gammaB = 1.0 / gammaB;

	WORD ramp[256*3];
	for(int i=0; i<256; i++){
		double valueR = pow((i + 1) / 256.0, gammaR) * 65536;
		double valueG = pow((i + 1) / 256.0, gammaG) * 65536;
		double valueB = pow((i + 1) / 256.0, gammaB) * 65536;
		
		if(valueR < 0) valueR = 0; if(valueR > 65535) valueR = 65535;
		if(valueG < 0) valueG = 0; if(valueG > 65535) valueG = 65535;
		if(valueB < 0) valueB = 0; if(valueB > 65535) valueB = 65535;
		
		ramp[0+i] = (WORD)valueR;
		ramp[256+i] = (WORD)valueG;
		ramp[512+i] = (WORD)valueB;
	}
	return !::SetDeviceGammaRamp(hdc, &ramp);
}

BOOL SetGammaCorrectRGB(double gammaR, double gammaG, double gammaB)
{
	return ::SetGammaCorrectMonitorRGB(::GetDC(NULL), gammaR, gammaG, gammaB);
}

BOOL SetGammaCorrect(double gamma)
{
	return SetGammaCorrectRGB(gamma, gamma, gamma);
}

BOOL SetGammaCorrectRGBCareMonitor(double gammaR, double gammaG, double gammaB)
{
	double r = gammaR - 1.0;
	double g = gammaG - 1.0;
	double b = gammaB - 1.0;

	for(int i=0; i<g_deviceContextCounter; i++){
		SetGammaCorrectMonitorRGB(
			monitorInfoList[i].hDC,
			monitorInfoList[i].r + r,
			monitorInfoList[i].g + g,
			monitorInfoList[i].b + b);
	}
	return TRUE;
}

void CreateDesktopShortcutForGamma(double gamma)
{
	LPITEMIDLIST lpidlist;
	TCHAR desktopPath[MAX_PATH];
	SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOPDIRECTORY, &lpidlist );
	SHGetPathFromIDList( lpidlist, desktopPath );

	TCHAR szPath[_MAX_PATH]=_T("");
	DWORD dwRet;

	//初期化
	dwRet = 0;
	
	//実行中のプロセスのフルパス名を取得する
	dwRet = GetModuleFileName(NULL, (LPWSTR)szPath, sizeof(szPath));
	if(dwRet == 0) {
	//エラー処理など(省略)
		;
	}
	
	// desktopPath + linkname
	TCHAR linkPath[_MAX_PATH]=L"";
	TCHAR gammaOption[256]=L"";
	::_stprintf_s(linkPath, _MAX_PATH, L"%s\\ガンマ%.2f.lnk", (LPCTSTR)desktopPath, gamma);
	::_stprintf_s(gammaOption, 256, L"-gamma %.1f", gamma);

	::CoInitialize(NULL);
	CreateShortcut((LPCTSTR)szPath, gammaOption, desktopPath, 0, (LPCSTR)linkPath);
	::CoUninitialize();
}

BOOL CALLBACK MonitorEnumProc(
  HMONITOR hMonitor,  // ディスプレイモニタのハンドル
  HDC hdcMonitor,     // モニタに適したデバイスコンテキストのハンドル
  LPRECT lprcMonitor, // モニタ上の交差部分を表す長方形領域へのポインタ
  LPARAM dwData       // EnumDisplayMonitors から渡されたデータ
){
	// モニタの情報を取得する
    MONITORINFOEX stMonInfoEx;
    stMonInfoEx.cbSize = sizeof(stMonInfoEx);
    ::GetMonitorInfo(hMonitor, &stMonInfoEx);

	LPTSTR deviceName = (LPTSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, 128);
	HDC hDC = ::CreateDC(L"DISPLAY", stMonInfoEx.szDevice, NULL, NULL);
	::wsprintf(deviceName, L"%s", stMonInfoEx.szDevice);

	// モニタ名称を設定
	LPTSTR monitorName = (LPTSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, 64);
	::wsprintf(monitorName, L"モニタ%d", g_deviceContextCounter + 1);

	MonitorInfo *monitor = &monitorInfoList[g_deviceContextCounter];
	monitor->hDC = hDC;
	monitor->deviceName = deviceName; // HDCに使えるデバイス名
	monitor->monitorName = monitorName; // 人間向けデバイス名
	monitor->r = monitor->g = monitor->b = monitor->level = GAMMA_DEFAULT_VALUE;
	g_deviceContextCounter++;

	return TRUE;
}

// モニタの数と名前を再認識する
void RecognizeMonitors(void)
{
	// すでに確保されてるメモリを解放する
	for(int i=0; i<g_deviceContextCounter; i++){
		::DeleteDC(monitorInfoList[i].hDC);
		::GlobalFree(monitorInfoList[i].deviceName);
		::GlobalFree(monitorInfoList[i].monitorName);
	}
	g_deviceContextCounter = 0;

	EnumDisplayMonitors(::GetDC(NULL), NULL, MonitorEnumProc, 0);
}

double GetPrivateProfileDouble(LPCTSTR section, LPCTSTR key, double def, LPCTSTR path)
{
	TCHAR buf[256];
	::GetPrivateProfileString(section, key, L"NOTFOUND", buf, sizeof(buf), path);

	if(::wcscmp(buf, L"NOTFOUND") == 0 || ::wcscmp(buf, L"") == 0)
		return def;
	return ::_wtof(buf);
}

BOOL WritePrivateProfileDouble(LPCTSTR section, LPCTSTR key, double val, LPCTSTR path)
{
	TCHAR buf[256];
	::_stprintf_s(buf, L"%.2f", val);
	return ::WritePrivateProfileString(section, key, buf, path);
}

void LoadConfig(void)
{
	LPTSTR lpCurrentDirectory = (LPTSTR)::GlobalAlloc(GMEM_FIXED, 1024);
	::GetCurrentDirectory(1024, lpCurrentDirectory);
		
	LPTSTR lpConfigPath = (LPTSTR)::GlobalAlloc(GMEM_FIXED, 1024);
	::wsprintf(lpConfigPath, L"%s\\%s", lpCurrentDirectory, L"config.ini");

	::g_lightUpKey = ::GetPrivateProfileInt(L"KeyBind", L"lightUpKey", VK_PRIOR, lpConfigPath);
	::g_lightDownKey = ::GetPrivateProfileInt(L"KeyBind", L"lightDownKey", VK_NEXT, lpConfigPath);
	::g_lightResetKey = ::GetPrivateProfileInt(L"KeyBind", L"lightResetKey", VK_HOME, lpConfigPath);
	::g_lightOptKey = ::GetPrivateProfileInt(L"KeyBind", L"lightOptKey", VK_CONTROL, lpConfigPath);

	::g_DoubleGamma = ::GetPrivateProfileDouble(L"Gamma", L"gamma", 1.0, lpConfigPath);
	::g_gammaR = ::GetPrivateProfileDouble(L"Gamma", L"gammaR", 1.0, lpConfigPath);
	::g_gammaG = ::GetPrivateProfileDouble(L"Gamma", L"gammaG", 1.0, lpConfigPath);
	::g_gammaB = ::GetPrivateProfileDouble(L"Gamma", L"gammaB", 1.0, lpConfigPath);
}

void SaveConfig(void)
{
	LPTSTR lpCurrentDirectory = (LPTSTR)::GlobalAlloc(GMEM_FIXED, 1024);
	::GetCurrentDirectory(1024, lpCurrentDirectory);
		
	LPTSTR lpConfigPath = (LPTSTR)::GlobalAlloc(GMEM_FIXED, 1024);
	::wsprintf(lpConfigPath, L"%s\\%s", lpCurrentDirectory, L"config.ini");

	TCHAR s_lightUpKey[256], s_lightDownKey[256], s_lightResetKey[256], s_lightOptKey[256];
	::wsprintf(s_lightUpKey, L"%d", ::g_lightUpKey);
	::wsprintf(s_lightDownKey, L"%d", ::g_lightDownKey);
	::wsprintf(s_lightResetKey, L"%d", ::g_lightResetKey);
	::wsprintf(s_lightOptKey, L"%d", ::g_lightOptKey);
	::WritePrivateProfileString(L"KeyBind", L"lightUpKey", s_lightUpKey, lpConfigPath);
	::WritePrivateProfileString(L"KeyBind", L"lightDownKey", s_lightDownKey, lpConfigPath);
	::WritePrivateProfileString(L"KeyBind", L"lightResetKey", s_lightResetKey, lpConfigPath);
	::WritePrivateProfileString(L"KeyBind", L"lightOptKey", s_lightOptKey, lpConfigPath);

	::WritePrivateProfileDouble(L"Gamma", L"gamma", ::g_DoubleGamma, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaR", ::g_gammaR, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaG", ::g_gammaG, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaB", ::g_gammaB, lpConfigPath);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// 多重起動防止の前に引数に明るさ情報が指定されていたら
	// それを利用してすぐに明るさを変更できるようにする
	// これは多重起動してもよいものとする
	int    i;
	int    nArgs;
	LPTSTR *lplpszArgs;

	lplpszArgs = CommandLineToArgvW(GetCommandLine(), &nArgs);

	if( nArgs == 1 ){
		;
	}else{
		// なんか引数あり、引数と値のペアの数がちゃんとしてるかチェック
		if( (nArgs - 1) % 2 == 0 ){
			// パラメーターの数があってるので中身が合ってるかとか意味があってるかのチェック
			for(i=1; i<nArgs; i+=2){
				LPTSTR lpOpt = lplpszArgs[i];
				LPTSTR lpStr = lplpszArgs[i+1];
				LPTSTR lpEnd = NULL;

				// gamma指定があったらそのガンマに設定する
				if(wcscmp(lpOpt, L"-gamma") == 0){
					// resetでリセットzzzz
					if(wcscmp(lpStr, L"reset") == 0 || wcscmp(lpStr, L"default") == 0){
						g_DoubleGamma = 1.0;
						SetGammaCorrect(g_DoubleGamma);
					}else{
						g_DoubleGamma = ::wcstod(lpStr, &lpEnd);
						if(g_DoubleGamma == 0 && lpStr == lpEnd){
							MessageBox(NULL, L"format error", L"test", MB_OK);
							exit(-1);
						}
						SetGammaCorrect(g_DoubleGamma);
					}
				}
			}
		}else{
			exit(-1);
		}
	}

	LocalFree(lplpszArgs);

	// 多重起動防止
	hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
	if(GetLastError() == ERROR_ALREADY_EXISTS){
		/*
		MessageBox(NULL, TEXT("すでに起動しています"), TEXT("ERROR"), MB_OK);
		*/
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
		return FALSE;
	}

	// config.iniを読み込んで設定を反映します
	LoadConfig();

	MSG msg;
	HACCEL hAccelTable;

	// グローバル文字列を初期化しています。
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_GANMACHANGER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// アプリケーションの初期化を実行します:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GANMACHANGER));

	// メイン メッセージ ループ:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			if(!::IsDialogMessage(g_hDlg,&msg)){
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	ReleaseMutex(hMutex);
	CloseHandle(hMutex);

	return (int) msg.wParam;
}



//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
//  コメント:
//
//    この関数および使い方は、'RegisterClassEx' 関数が追加された
//    Windows 95 より前の Win32 システムと互換させる場合にのみ必要です。
//    アプリケーションが、関連付けられた
//    正しい形式の小さいアイコンを取得できるようにするには、
//    この関数を呼び出してください。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GANMACHANGER));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_GANMACHANGER);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します。
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // グローバル変数にインスタンス処理を格納します。

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   // 複数のアイコンを識別するためのID定数
	#define ID_TRAYICON  (1)
	// タスクトレイのマウスメッセージの定数
	#define WM_TASKTRAY  (WM_APP + 1)
	// 構造体の宣言(初期化)

	// 構造体メンバの設定
	nid.cbSize           = sizeof( NOTIFYICONDATA );
	nid.uFlags           = (NIF_ICON|NIF_MESSAGE|NIF_TIP);
	nid.hWnd             = hWnd;           // ウインドウ・ハンドル
	nid.hIcon            = LoadIcon(hInst, MAKEINTRESOURCE(IDI_GANMACHANGER));          // アイコン・ハンドル
	nid.uID              = ID_TRAYICON;    // アイコン識別子の定数
	nid.uCallbackMessage = WM_TASKTRAY;    // 通知メッセージの定数
	lstrcpy( nid.szTip, TEXT("ガンマ値変更ツール") );  // チップヘルプの文字列
	// アイコンの変更
	CheckError(Shell_NotifyIcon( NIM_ADD, (PNOTIFYICONDATAW)&nid ) == TRUE);

	//ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

BOOL nextGamma(void)
{
	g_DoubleGamma += GAMMA_INCREMENT_VALUE;
	if(g_DoubleGamma > GAMMA_MAX_VALUE)
		g_DoubleGamma = GAMMA_MAX_VALUE;
	return ::SetGammaCorrectRGBCareMonitor(g_DoubleGamma, g_DoubleGamma, g_DoubleGamma);
}

BOOL prevGamma(void)
{
	g_DoubleGamma -= GAMMA_DECREMENT_VALUE;
	if(g_DoubleGamma < GAMMA_MIN_VALUE) g_DoubleGamma = GAMMA_MIN_VALUE;
	return ::SetGammaCorrectRGBCareMonitor(g_DoubleGamma, g_DoubleGamma, g_DoubleGamma);
}

BOOL resetGamma(void)
{
	g_DoubleGamma = GAMMA_DEFAULT_VALUE;
	return ::SetGammaCorrectRGBCareMonitor(g_DoubleGamma, g_DoubleGamma, g_DoubleGamma);
}

#define SETDOUBLE_BUFFER_SIZE 256
BOOL SetDlgItemDouble(HWND hWnd, UINT id, double value)
{
	TCHAR buf[SETDOUBLE_BUFFER_SIZE]=_T("");
	::_stprintf_s(buf, SETDOUBLE_BUFFER_SIZE, _T("%.2f"), value);
	return ::SetDlgItemText(hWnd, id, (LPCTSTR)buf);
}

double GetDlgItemDouble(HWND hWnd, UINT id)
{
	TCHAR buf[SETDOUBLE_BUFFER_SIZE]=TEXT("");
	::GetDlgItemText(hWnd, id, (LPTSTR)&buf, sizeof(buf));
	return ::_wtof(buf);
}

// ダイアログプロシージャ
BOOL CALLBACK DlgGammaProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	BOOL result = false;
	double level = 0;
	int delta = 0;
	
	switch( msg ){
	case WM_HSCROLL:
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_DoubleGamma = pos * 5.00 / 100;
			g_gammaR = g_gammaG = g_gammaB = g_DoubleGamma;
			
			// 各モニタのガンマを意識したまま全体のガンマの上げ下げをする
			::SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);
			
			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_DoubleGamma);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (5.00 / 100)));
		}
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_RGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gammaR = pos * 5.00 / 100;
			SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
		}
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gammaG = pos * 5.00 / 100;
			SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
		}
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_BGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gammaB = pos * 5.00 / 100;
			SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);
		}
		break;
	case WM_MOUSEWHEEL:
		break;
	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_DoubleGamma);
		::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
		::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
		::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);

		::SendDlgItemMessageA(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(g_DoubleGamma / (5.00 / 100)));
		::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (5.00 / 100)));
		::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (5.00 / 100)));
		::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (5.00 / 100)));

		// 常に最前面に表示
		::SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
		return TRUE;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK:       // 「OK」ボタンが選択された
			SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);
			break;
		case IDCANCEL:   // 「キャンセル」ボタンが選択された
			// ダイアログボックスを消す
			EndDialog(g_hGammaDlg, LOWORD(wp));
			g_hGammaDlg = NULL;
			break;
		case IDDEFAULT:
			g_DoubleGamma = g_gammaR = g_gammaG = g_gammaB = 1.0;
			SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_DoubleGamma);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(g_DoubleGamma / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (5.00 / 100)));

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_DoubleGamma);
			break;
		case IDSHORTCUT: // ショートカット作成
			CreateDesktopShortcutForGamma(g_DoubleGamma);
			break;
		}
		return TRUE;

	case WM_CLOSE:		// ダイアログボックスが閉じられるとき
		// ダイアログボックスを消す
		EndDialog(g_hGammaDlg, LOWORD(wp));
		g_hGammaDlg = NULL;
		return TRUE;
	}

	return FALSE;  // DefWindowProc()ではなく、FALSEを返すこと！
}



// ダイアログプロシージャ
#define MAX_GAMMA 5.0
#define MIN_GAMMA 0.0
#define DEFAULT_GAMMA 1.0
#define SLIDER_SIZE 100
BOOL CALLBACK DlgMonitorGammaProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	BOOL result = false;
	double level = 0;
	int delta = 0;
	MonitorInfo *monitor = &monitorInfoList[::g_hMonitorTargetIndex];
	int pos;
	double gamma_value;

	switch( msg ){
	case WM_HSCROLL:
		pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
		gamma_value = (pos * MAX_GAMMA / SLIDER_SIZE);

		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GAMMA) ){
			monitor->level = monitor->r = monitor->g = monitor->b = gamma_value;
			
			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));
		}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_RGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			monitor->r = gamma_value;
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
		}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			monitor->g = gamma_value;
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
		}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_BGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			monitor->b = gamma_value;
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
		}
		::SetGammaCorrectMonitorRGB(monitor->hDC, monitor->r, monitor->g, monitor->b);
		break;
	case WM_MOUSEWHEEL:
		break;
	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		{
			MonitorInfo *monitor = &monitorInfoList[g_hMonitorTargetIndex];

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
			
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(monitor->level / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));

			// タイトルをモニタとわかるようなものに
			TCHAR buf[256];
			::wsprintf((LPTSTR)buf, (LPCTSTR)_T("%sのガンマ調節"), monitor->monitorName);
			::SetWindowText(hDlg, buf);

			// 常に最前面に表示
			::SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
		}
		return TRUE;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK:       // 適用ボタンが選択された
			level = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_BRIGHTNESS_LEVEL);

			if(level != monitor->level){
				monitor->level = monitor->r = monitor->g = monitor->b = level;
				::SetGammaCorrectMonitorRGB(monitor->hDC, monitor->r, monitor->g, monitor->b);
			}else{
				monitor->r = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_EDIT_RGAMMA);
				monitor->g = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_EDIT_GGAMMA);
				monitor->b = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_EDIT_BGAMMA);
				::SetGammaCorrectMonitorRGB(monitor->hDC, monitor->r, monitor->g, monitor->b);
			}

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
			
			// gamma -> slider
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(monitor->level / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));
			break;
		case IDCANCEL:   // 「キャンセル」ボタンが選択された
			// ダイアログボックスを消す
			EndDialog(g_hMonitorGammaDlg, LOWORD(wp));
			g_hMonitorGammaDlg = NULL;
			break;
		case IDDEFAULT:
			monitor->level = monitor->r = monitor->g = monitor->b = DEFAULT_GAMMA;
			SetGammaCorrectMonitorRGB(monitor->hDC, monitor->r, monitor->g, monitor->b);
			
			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(monitor->level / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));
			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			break;
		case IDSHORTCUT: // ショートカット作成
			CreateDesktopShortcutForGamma(monitor->level);
			break;
		}
		return TRUE;

	case WM_CLOSE:		// ダイアログボックスが閉じられるとき
		// ダイアログボックスを消す
		EndDialog(g_hMonitorGammaDlg, LOWORD(wp));
		g_hMonitorGammaDlg = NULL;
		return TRUE;
	}

	return FALSE;  // DefWindowProc()ではなく、FALSEを返すこと！
}

BOOL stopOrResume(HWND hWnd)
{
	bStopedFlag = !bStopedFlag;
	if(bStopedFlag){
		StopHook();

		HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);
		nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GANMACHANGER_STOP));
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}else{
		RestartHook();
		HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);
		nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GANMACHANGER));
	
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
	return bStopedFlag;
}

double colors[][3] = {{2.0, 1.0, 1.0}, {1.0, 2.0, 1.0}, {1.0, 1.0, 2.0}};
int colorIndex = 0;

VOID CALLBACK TimerProc(
  HWND hwnd,         // ウィンドウのハンドル
  UINT uMsg,         // WM_TIMER メッセージ
  UINT_PTR idEvent,  // タイマの識別子
  DWORD dwTime       // 現在のシステム時刻
){
	if(colorIndex >= 3){
		colorIndex = 0;
	}

	::SetGammaCorrectMonitorRGB(::GetDC(NULL), colors[colorIndex][0], colors[colorIndex][1], colors[colorIndex][2]);
	colorIndex++;
	return;
}

HHOOK g_hKeyConfigHook = NULL;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wp, LPARAM lp)
{
	//nCodeが0未満のときは、CallNextHookExが返した値を返す
	if (nCode < 0)  return CallNextHookEx(g_hKeyConfigHook,nCode,wp,lp);

	if (nCode==HC_ACTION) {
		//キーの遷移状態のビットをチェックして
		//WM_KEYDOWNとWM_KEYUPをDialogに送信する
		if ((lp & 0x80000000)==0) {
			PostMessage(g_hKeyConfigDlg,WM_KEYDOWN,wp,lp);
			return TRUE;
		}else{
			PostMessage(g_hKeyConfigDlg,WM_KEYUP,wp,lp);
			return TRUE;
		}
    }
    return CallNextHookEx(g_hKeyConfigHook,nCode,wp,lp);
}

LPTSTR GetKeyNameTextEx(UINT vk)
{
	UINT uScanCode = ::MapVirtualKey(vk, 0);
	LPARAM lParam = (uScanCode << 16);

	switch (vk) {
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_END:
	case VK_HOME:
	case VK_INSERT:
	case VK_DELETE:
	case VK_DIVIDE:
	case VK_NUMLOCK:
		lParam = (uScanCode << 16) | (1 << 24);
		break;
	}

	LPTSTR buffer = (LPTSTR)::GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, 256);
	::GetKeyNameText(lParam, buffer, 256);
	return buffer;
}

BOOL CALLBACK DlgKeyConfigProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	LPTSTR lpstr, lpBuffer = NULL;
	static UINT targetID = -1;
	BYTE keyTbl[256];
	static int prevKey, nextKey, resetKey, optKey = 0;
	LPTSTR s_nextKey, s_prevKey, s_resetKey, s_optKey, s_buffer = NULL;

	switch( msg ){
	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		// 常に最前面に表示
		::SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
		s_buffer = (LPTSTR)::GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, 256);
	
		s_nextKey = ::GetKeyNameTextEx(g_lightUpKey);
		s_prevKey = ::GetKeyNameTextEx(g_lightDownKey);
		s_resetKey = ::GetKeyNameTextEx(g_lightResetKey);
		s_optKey = ::GetKeyNameTextEx(g_lightOptKey);

		if(g_lightOptKey != NULL){
			::wsprintf(s_buffer, L"%s + %s", s_optKey, s_nextKey);
			::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTUP, s_buffer);

			::wsprintf(s_buffer, L"%s + %s", s_optKey, s_prevKey);
			::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTDOWN, s_buffer);

			::wsprintf(s_buffer, L"%s + %s", s_optKey, s_resetKey);
			::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTRESET, s_buffer);
		}else{
			::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTUP, s_nextKey);
			::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTDOWN, s_prevKey);
			::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTRESET, s_resetKey);
		}

		::GlobalFree(s_buffer);
		::GlobalFree(s_nextKey);
		::GlobalFree(s_prevKey);
		::GlobalFree(s_resetKey);
		::GlobalFree(s_optKey);

		nextKey = g_lightUpKey;
		prevKey = g_lightDownKey;
		resetKey = g_lightResetKey;
		optKey = g_lightOptKey;
		return TRUE;

	case WM_KEYDOWN:
		if( !::GetKeyboardState((PBYTE)&keyTbl) ){
			ShowLastError();
			exit(-1);
		}

		optKey = NULL;

		lpstr = (LPTSTR)::GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, 256);
		lpBuffer = (LPTSTR)::GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, 256);

		if( keyTbl[VK_CONTROL] & 0x80 ){
			::GetKeyNameText(lp, lpstr, 256);
			
			if(wp != VK_CONTROL){
				::wsprintf(lpBuffer, L"%s + %s", L"Ctrl", lpstr);
				optKey = VK_CONTROL;
			}else{
				::wsprintf(lpBuffer, lpstr);
			}

		}else if( keyTbl[VK_SHIFT] & 0x80 ){
			::GetKeyNameText(lp, lpstr, 256);

			if(wp != VK_SHIFT){
				::wsprintf(lpBuffer, L"%s + %s", L"Shift", lpstr);
				optKey = VK_SHIFT;
			}else
				::wsprintf(lpBuffer, lpstr);
		}else if( keyTbl[VK_MENU] & 0x80 ){
			::GetKeyNameText(lp, lpstr, 256);

			if(wp != VK_MENU){
				::wsprintf(lpBuffer, L"%s + %s", L"Alt", lpstr);
				optKey = VK_MENU;
			}else
				::wsprintf(lpBuffer, lpstr);
		}else{
			::GetKeyNameText(lp, lpstr, 256);
			::wsprintf(lpBuffer, L"%s", lpstr);
		}

		::SetDlgItemText(hDlg, targetID, lpBuffer);

		if(targetID == IDC_EDIT_KEYBIND_LIGHTUP){
			nextKey = wp;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTDOWN){
			prevKey = wp;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTRESET){
			resetKey = wp;
		}

		::GlobalFree(lpstr);
		::GlobalFree(lpBuffer);
		return TRUE;

	case WM_KEYUP:
		if(targetID == IDC_EDIT_KEYBIND_LIGHTUP){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTUP, L"設定");
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTDOWN){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTDOWN, L"設定");
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTRESET){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTRESET, L"設定");
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}
		break;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK:       // 適用ボタンが選択された
			::StopHook();
			::StartKeyHook(prevKey, nextKey, resetKey, optKey);

			g_lightUpKey = nextKey;
			g_lightDownKey = prevKey;
			g_lightResetKey = resetKey;
			g_lightOptKey = optKey;

			EndDialog(g_hKeyConfigDlg, LOWORD(wp));
			g_hKeyConfigDlg = NULL;
			break;
		case IDCANCEL:   // 「キャンセル」ボタンが選択された
			// ダイアログボックスを消す
			EndDialog(g_hKeyConfigDlg, LOWORD(wp));
			g_hKeyConfigDlg = NULL;
			break;
		case ID_KEYBIND_LIGHTUP:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTUP;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTUP, L"入力");
			break;
		case ID_KEYBIND_LIGHTDOWN:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTDOWN;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTDOWN, L"入力");
			break;
		case ID_KEYBIND_LIGHTRESET:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTRESET;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTRESET, L"入力");
			break;
		case IDDEFAULT:
			nextKey = g_lightUpKey = VK_PRIOR;
			prevKey = g_lightDownKey = VK_NEXT;
			resetKey = g_lightResetKey = VK_HOME;
			optKey = g_lightOptKey = VK_CONTROL;

			s_buffer = (LPTSTR)::GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, 256);
	
			s_nextKey = ::GetKeyNameTextEx(g_lightUpKey);
			s_prevKey = ::GetKeyNameTextEx(g_lightDownKey);
			s_resetKey = ::GetKeyNameTextEx(g_lightResetKey);
			s_optKey = ::GetKeyNameTextEx(g_lightOptKey);

			if(g_lightOptKey != NULL){
				::wsprintf(s_buffer, L"%s + %s", s_optKey, s_nextKey);
				::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTUP, s_buffer);

				::wsprintf(s_buffer, L"%s + %s", s_optKey, s_prevKey);
				::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTDOWN, s_buffer);

				::wsprintf(s_buffer, L"%s + %s", s_optKey, s_resetKey);
				::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTRESET, s_buffer);
			}else{
				::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTUP, s_nextKey);
				::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTDOWN, s_prevKey);
				::SetDlgItemText(hDlg, IDC_EDIT_KEYBIND_LIGHTRESET, s_resetKey);
			}

			::StopHook();
			::StartKeyHook(prevKey, nextKey, resetKey, optKey);
			break;
		}
		return TRUE;

	case WM_CLOSE:		// ダイアログボックスが閉じられるとき
		// ダイアログボックスを消す
		// フックされてたらそれを消す
		if(::g_hKeyConfigHook){
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}

		EndDialog(g_hKeyConfigDlg, LOWORD(wp));
		g_hKeyConfigDlg = NULL;
		return TRUE;
	}

	return FALSE;  // DefWindowProc()ではなく、FALSEを返すこと！
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的:  メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND	- アプリケーション メニューの処理
//  WM_PAINT	- メイン ウィンドウの描画
//  WM_DESTROY	- 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	HMENU hMenu = NULL;
	HMENU hSubMenu = NULL;
	static UINT wm_taskbarCreated;
	HMENU hView;
	MENUITEMINFO mii;
	static UINT_PTR timer = NULL;

	switch (message)
	{
	case WM_MOUSEWHEEL:
		::OutputDebugString(TEXT("wheeeeel"));
		break;
	case WM_CREATE:
		// モニタの種類を再認識する
		RecognizeMonitors();

		SetWindowHandle(hWnd);
		StartKeyHook(g_lightUpKey, g_lightDownKey, g_lightResetKey, g_lightOptKey);

		// taskbar event
		wm_taskbarCreated = RegisterWindowMessage(TEXT("TaskbarCreated"));
		break;
	case WM_TASKTRAY:
		switch(lParam){
		case WM_LBUTTONDOWN:
			// シングルクリックで中断 <-> 再開
			stopOrResume(hWnd);
			break;
		case WM_LBUTTONDBLCLK:
			break;
		case WM_RBUTTONDOWN:
			SetForegroundWindow(hWnd);

			// カーソルの現在位置を取得
			POINT point;
			::GetCursorPos(&point);

			hMenu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU));
			hSubMenu = GetSubMenu(hMenu, 0);
	
			// 現在の選択状態をタスクバーに表示
			if(bStopedFlag){
				CheckMenuItem(hSubMenu, IDM_STOP, MF_BYCOMMAND | MF_CHECKED);
			}else{
				CheckMenuItem(hSubMenu, IDM_STOP, MF_BYCOMMAND | MF_UNCHECKED);
			}

			hView = CreatePopupMenu();
			mii.wID = IDM_MONITOR;
			mii.cbSize = sizeof(MENUITEMINFO);
			mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU;
			mii.fType = MFT_STRING;
			mii.hSubMenu = hView;
			mii.dwTypeData = _T("モニタ別調節");
			InsertMenuItem(hSubMenu, 0, TRUE, &mii);
			
			for(int i=0; i<g_deviceContextCounter; i++){
				mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID;
				mii.dwTypeData = monitorInfoList[i].monitorName;
				mii.wID = IDM_MONITOR + i; // 2500 - 2600 reserved for monitors
				InsertMenuItem(hView, i, TRUE, &mii);
			}

			TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL);
			PostMessage(hWnd, WM_NULL, 0, 0);
			break;
		case WM_MOUSEWHEEL:
			::OutputDebugString(TEXT("wheel"));
			break;
		}
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// 選択されたメニューの解析:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;

		case IDM_GAMMA_CONTROL:
			if(!g_hGammaDlg)
				g_hGammaDlg = CreateDialog(hInst, TEXT("IDD_GAMMA_DIALOG"), hWnd, &DlgGammaProc);
			break;

		case IDM_SHORTCUT:
			CreateDesktopShortcutForGamma(g_DoubleGamma);
			break;

		case IDM_RESET:
			g_gammaR = g_gammaG = g_gammaB = 1.0;
			g_DoubleGamma = 1.0;
			for(int i=0; i<g_deviceContextCounter; i++){
				monitorInfoList[i].r = 1.0;
				monitorInfoList[i].g = 1.0;
				monitorInfoList[i].b = 1.0;
				monitorInfoList[i].level = 1.0;
				::SetGammaCorrectMonitorRGB(monitorInfoList[i].hDC, 1.0, 1.0, 1.0);
			}
			break;

		case IDM_POCKEMON:
			if(timer != NULL){
				KillTimer(NULL, timer);
				timer = NULL;
				::resetGamma();
			}else{
				timer = SetTimer(NULL, 0, 500, TimerProc);
			}
			break;

		case IDM_STOP:
			stopOrResume(hWnd);
			break;
		case IDM_KEYBOARD:
			if(!g_hKeyConfigDlg)
				g_hKeyConfigDlg = CreateDialog(hInst, TEXT("IDD_KEYCONFIG_DIALOG"), hWnd, &DlgKeyConfigProc);
			break;
		default:
			if(IDM_MONITOR <= wmId && wmId <= IDM_MONITOR + 100){
				int monitorId = wmId - IDM_MONITOR;
				g_hMonitorTargetIndex = monitorId;

				// モニタのIDがわかったのでそいつを対象としたダイアログ表示
				if(!g_hMonitorGammaDlg)
					g_hMonitorGammaDlg = CreateDialog(hInst, TEXT("IDD_GAMMA_DIALOG"), hWnd, &DlgMonitorGammaProc);
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_USER_MESSAGE:
		// VK_PRIOR, VK_NEXT = PgUp, PgDOWN
		if(g_lightOptKey == NULL){
			if(wParam == g_lightUpKey){
				nextGamma();
			}else if(wParam == g_lightDownKey){
				prevGamma();
			}else if(wParam == g_lightResetKey){
				resetGamma();
			}
		}else{
			if(GetAsyncKeyState(g_lightOptKey)){
				if(wParam == g_lightUpKey){
					nextGamma();
				}else if(wParam == g_lightDownKey){
					prevGamma();
				}else if(wParam == g_lightResetKey){
					resetGamma();
				}
			}
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: 描画コードをここに追加してください...
		EndPaint(hWnd, &ps);
		break;
	case WM_CLOSE:
		SetMenu(hWnd, NULL);
		DestroyMenu(hMenu);
		hMenu = NULL;
	case WM_DESTROY:
		SaveConfig();

		StopHook();
		NOTIFYICONDATA tnid; 
		tnid.cbSize = sizeof(NOTIFYICONDATA); 
		tnid.hWnd = hWnd;				// メインウィンドウハンドル
		tnid.uID = ID_TRAYICON;			// コントロールID
		::Shell_NotifyIcon(NIM_DELETE, &tnid); 

		SetMenu(hWnd, NULL);
		DestroyMenu(hMenu);
		hMenu = NULL;
		PostQuitMessage(0);
		break;
	default:
		if(message == wm_taskbarCreated){
			Shell_NotifyIcon(NIM_ADD, &nid);
		}else{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
