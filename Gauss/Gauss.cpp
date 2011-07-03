#include "stdafx.h"
#include "Gauss.h"
#include <Windows.h>
#include <Windowsx.h>
#include <ShellAPI.h>
#include <math.h>
#include "resource.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex>
#include <list>
using namespace std;

#include <shlobj.h>
#pragma comment(lib, "shell32.lib")

#include <CommCtrl.h>
#pragma comment(lib, "ComCtl32.lib")

#include "Util.h"
#include "GammaController.h"

#include "KeyHook.h"
#ifdef _WIN64
#pragma comment(lib, "KeyHook64.lib")
#else
#pragma comment(lib, "KeyHook.lib")
#endif

// for x64 environment
#ifdef _WIN64
#undef GWL_HINSTANCE
#define GWL_HINSTANCE GWLP_HINSTANCE
#undef GetWindowLong
#define GetWindowLong GetWindowLongPtr
#endif

#define MUTEX_NAME L"Gauss"

#define MAX_LOADSTRING 100
#define WM_USER_MESSAGE (WM_USER+0x1000)
#define SLIDER_SIZE ((int)((MAX_GAMMA - MIN_GAMMA) * 100)) // probablly 417

// 複数のアイコンを識別するためのID定数
#define ID_TRAYICON  (1)

// タスクトレイのマウスメッセージの定数
#define WM_TASKTRAY  (WM_APP + 1)

#define WM_GAMMA_UP (WM_USER_MESSAGE + 1)
#define WM_GAMMA_DOWN (WM_USER_MESSAGE + 2)
#define WM_GAMMA_RESET (WM_USER_MESSAGE + 3)

#define GAMMA_TO_SLIDER_POS(gamma) ((int)((gamma - MIN_GAMMA) * 100))
#define SLIDER_POS_TO_GAMMA(pos) (pos / 100.0 + MIN_GAMMA)

#define SLIDER_SETPOS(hwnd, id, value) (::SendDlgItemMessage(hwnd, id, TBM_SETPOS, TRUE, value))
#define SLIDER_SETRANGE(hwnd, id, a, b) (::SendDlgItemMessage(hwnd, id, TBM_SETRANGE, TRUE, MAKELONG(a, b)))

#define IF_KEY_PRESS(lp) ((lp & (1 << 31)) == 0)

HINSTANCE hInst;								// 現在のインターフェイス
TCHAR szTitle[MAX_LOADSTRING];					// タイトル バーのテキスト
TCHAR szWindowClass[MAX_LOADSTRING];			// メイン ウィンドウ クラス名
HANDLE hMutex;
NOTIFYICONDATA nid = { 0 };
bool bStopedFlag = false;

// GUIの値保存用変数
double g_gamma = 1.0;
double g_gammaR = 1.0;
double g_gammaG = 1.0;
double g_gammaB = 1.0;
int g_hMonitorTargetIndex;

HWND g_hDlg;
HWND g_hGammaDlg;
HWND g_hMonitorGammaDlg;
HWND g_hKeyConfigDlg;

KEYINFO g_lightUpKeyInfo = {0};
KEYINFO g_lightDownKeyInfo = {0};
KEYINFO g_lightResetKeyInfo = {0};

HHOOK g_hKeyConfigHook = NULL;
GammaController gammaController; // gamma制御用の関数まとめたクラス

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

BOOL CreateDesktopShortcutForMonitorGamma(int index, MonitorInfo *monitor)
{
	TCHAR desktopPath[MAX_PATH];
	if( !::GetDesktopPath(desktopPath, MAX_PATH) )
		return FALSE;

	//実行中のプロセスのフルパス名を取得する
	TCHAR szPath[MAX_PATH];
	if( GetModuleFileName(NULL, (LPWSTR)szPath, sizeof(szPath)) == 0 ){
		return FALSE;
	}

  TCHAR linkFormat[MAX_PATH];
  ::LoadString(::GetModuleHandle(NULL), IDS_SHORTCUT_LINK_FORMAT, linkFormat, sizeof(linkFormat));

	TCHAR linkPath[MAX_PATH];
	TCHAR gammaOption[MAX_PATH];
	::_stprintf_s(linkPath, linkFormat, desktopPath, monitor->monitorName, monitor->r, monitor->g, monitor->b);
	::_stprintf_s(gammaOption, L"-monitor-gammaRGB Monitor%d=%.1f:%.1f:%.1f", index + 1, monitor->r, monitor->g, monitor->b);
  
	::CoInitialize(NULL);
	BOOL result = CreateShortcut(szPath, gammaOption, desktopPath, 0, (LPCSTR)linkPath);
	::CoUninitialize();
  
  ::MessageBeep(MB_ICONASTERISK);

	return result;
}

BOOL CreateDesktopShortcutForGammaRGB(double gammaR, double gammaG, double gammaB)
{
	TCHAR desktopPath[MAX_PATH];
	if( !::GetDesktopPath(desktopPath, MAX_PATH) )
		return FALSE;

	//実行中のプロセスのフルパス名を取得する
	TCHAR szPath[MAX_PATH];
	if( GetModuleFileName(NULL, (LPWSTR)szPath, sizeof(szPath)) == 0 ){
		return FALSE;
	}

  TCHAR linkFormat[MAX_PATH];
  ::LoadString(::GetModuleHandle(NULL), IDS_SHORTCUT_LINK_FORMAT_RGB, linkFormat, sizeof(linkFormat));

	TCHAR linkPath[MAX_PATH];
	TCHAR gammaOption[MAX_PATH];
	::_stprintf_s(linkPath, linkFormat, desktopPath, gammaR, gammaG, gammaB);
	::_stprintf_s(gammaOption, L"-gammaRGB %.1f:%.1f:%.1f", gammaR, gammaG, gammaB);

	::CoInitialize(NULL);
	BOOL result = CreateShortcut(szPath, gammaOption, desktopPath, 0, (LPCSTR)linkPath);
	::CoUninitialize();

  ::MessageBeep(MB_ICONASTERISK);

	return result;
}

BOOL CreateDesktopShortcutForGamma(double gamma)
{
  return CreateDesktopShortcutForGammaRGB(gamma, gamma, gamma);
}

BOOL CALLBACK MonitorEnumProc(
	HMONITOR hMonitor,  // ディスプレイモニタのハンドル
	HDC hdcMonitor,     // モニタに適したデバイスコンテキストのハンドル
	LPRECT lprcMonitor, // モニタ上の交差部分を表す長方形領域へのポインタ
	LPARAM dwData       // EnumDisplayMonitors から渡されたデータ
	){
#define MONITOR_WORDS MAX_PATH

		// モニタの情報を取得する
		MONITORINFOEX stMonInfoEx;
		stMonInfoEx.cbSize = sizeof(stMonInfoEx);
		::GetMonitorInfo(hMonitor, &stMonInfoEx);

		HDC hDC = ::CreateDC(L"DISPLAY", stMonInfoEx.szDevice, NULL, NULL);
		LPTSTR deviceName = sprintf_alloc(L"%s", stMonInfoEx.szDevice);
    
    TCHAR format[256];
    ::LoadString(::GetModuleHandle(NULL), IDS_MONITOR_CONTROL_NAME_FORMAT, format, sizeof(format));
		LPTSTR monitorName = sprintf_alloc(format, gammaController.monitorGetCount() + 1);

		// 取得した情報を設定します
    gammaController.monitorAdd(hDC, deviceName, monitorName, stMonInfoEx.rcMonitor);

		return TRUE;
}

// モニタの数と名前を再認識する
void RecognizeMonitors(void)
{
	gammaController.monitorReset();
	EnumDisplayMonitors(::GetDC(NULL), NULL, MonitorEnumProc, 0);
}

void LoadConfig(void)
{
	// モニタを認識します
	RecognizeMonitors();

	LPTSTR lpConfigPath = ::GetConfigPath(L"config.ini");

	// setup default key config
	::QuickSetKeyInfo(&::g_lightUpKeyInfo, VK_CONTROL, VK_PRIOR);
	::QuickSetKeyInfo(&::g_lightDownKeyInfo, VK_CONTROL, VK_NEXT);
	::QuickSetKeyInfo(&::g_lightResetKeyInfo, VK_CONTROL, VK_HOME);

	// light up/down/reset keyconfig
	::GetPrivateProfileKeyInfo(L"KeyBind", L"lightUp", &::g_lightUpKeyInfo, lpConfigPath);
	::GetPrivateProfileKeyInfo(L"KeyBind", L"lightDown", &::g_lightDownKeyInfo, lpConfigPath);
	::GetPrivateProfileKeyInfo(L"KeyBind", L"lightReset", &::g_lightResetKeyInfo, lpConfigPath);

	::g_gamma = ::GetPrivateProfileDouble(L"Gamma", L"gamma", DEFAULT_GAMMA, lpConfigPath);
	::g_gammaR = ::GetPrivateProfileDouble(L"Gamma", L"gammaR", DEFAULT_GAMMA, lpConfigPath);
	::g_gammaG = ::GetPrivateProfileDouble(L"Gamma", L"gammaG", DEFAULT_GAMMA, lpConfigPath);
	::g_gammaB = ::GetPrivateProfileDouble(L"Gamma", L"gammaB", DEFAULT_GAMMA, lpConfigPath);

	// 起動時はガンマリセットする
	// 前回終了時のガンマ設定に強制変更
	// gammaController.setGamma(g_gammaR, g_gammaG, g_gammaB);

	// モニタごとにガンマを設定します
	int count = gammaController.monitorGetCount();
	for(int i=0; i<count; i++){
		LPTSTR key = ::sprintf_alloc(L"Monitor%d", i + 1);
		double level = ::GetPrivateProfileDouble(key, L"gamma", DEFAULT_GAMMA, lpConfigPath);
		double r = ::GetPrivateProfileDouble(key, L"gammaR", DEFAULT_GAMMA, lpConfigPath);
		double g = ::GetPrivateProfileDouble(key, L"gammaG", DEFAULT_GAMMA, lpConfigPath);
		double b = ::GetPrivateProfileDouble(key, L"gammaB", DEFAULT_GAMMA, lpConfigPath);
		gammaController.setMonitorGammaIndex(i, r, g, b, level);
		::GlobalFree(key);
	}

	::GlobalFree(lpConfigPath);
}

void SaveConfig(void)
{
	// プログラム(実行ファイル)のあるフォルダに設定を保存します
	LPTSTR lpConfigPath = ::GetConfigPath(L"config.ini");

	// light up/down/reset keyconfigs
	::WritePrivateProfileKeyInfo(L"KeyBind", L"lightUp", &::g_lightUpKeyInfo, lpConfigPath);
	::WritePrivateProfileKeyInfo(L"KeyBind", L"lightDown", &::g_lightDownKeyInfo, lpConfigPath);
	::WritePrivateProfileKeyInfo(L"KeyBind", L"lightReset", &::g_lightResetKeyInfo, lpConfigPath);

	::WritePrivateProfileDouble(L"Gamma", L"gamma", ::g_gamma, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaR", ::g_gammaR, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaG", ::g_gammaG, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaB", ::g_gammaB, lpConfigPath);

	// モニタごとのガンマ値を保存する
	// [Monitor1] = index 0
	// [Monitor2] = index 1
	// [Monitor3] = index 2 みたいな感じで対応付け
	int count = gammaController.monitorGetCount();
	for(int i=0; i<count; i++){
		// モニタのガンマ値を読み込んで保存
		LPTSTR key = ::sprintf_alloc(L"Monitor%d", i + 1);
		::WritePrivateProfileDouble(key, L"gamma",	gammaController.monitorGet(i)->level, lpConfigPath);
		::WritePrivateProfileDouble(key, L"gammaR",	gammaController.monitorGet(i)->r, lpConfigPath);
		::WritePrivateProfileDouble(key, L"gammaG",	gammaController.monitorGet(i)->g, lpConfigPath);
		::WritePrivateProfileDouble(key, L"gammaB",	gammaController.monitorGet(i)->b, lpConfigPath);
		::GlobalFree(key);
	}

	::GlobalFree(lpConfigPath);
}

/**
* split関数
* @param string str 分割したい文字列
* @param string delim デリミタ
* @return list<string> 分割された文字列
* copy from goodjob.boy.jp/chirashinoura/id/100.html
*/
list<wstring> split(wstring str, wstring delim)
{
	list<wstring> result;
	int cutAt;
	while( (cutAt = str.find_first_of(delim)) != str.npos )
	{
		if(cutAt > 0)
		{
			result.push_back(str.substr(0, cutAt));
		}
		str = str.substr(cutAt + 1);
	}
	if(str.length() > 0)
	{
		result.push_back(str);
	}
	return result;
}

double wstring2double(wstring str)
{
	LPTSTR start = (LPTSTR)str.c_str();
	LPTSTR end = NULL;

	double value = ::wcstod(start, &end);
	if(value == 0 && start == end){
		throw L"illegal argument";
	}
	return value;
}

long wstring2int(wstring str, int radix = 10)
{
	LPTSTR start = (LPTSTR)str.c_str();
	LPTSTR end = NULL;

	long value = ::wcstol(start, &end, radix);
	if(value == 0 && start == end){
		throw L"illegal argument";
	}
	return value;
}

// R:G:B の形式の文字列を読み込んでガンマ設定に反映させます
void CommandLine_SetGammaRGB(LPTSTR input)
{
	// コロンで区切る
	wstring ws = wstring(input);
	list<wstring> l = split(ws, L":");

	// 要素数があってることを確認
	if(l.size() != 3){
		::LocaleErrorMsgBox(IDS_INVALID_ARGUMENT_FORMAT, input);
		exit(-1);
	}

	list<wstring>::iterator it = l.begin();
	try{
		double r = wstring2double(*it++);
		double g = wstring2double(*it++);
		double b = wstring2double(*it++);
		gammaController.setGamma(r, g, b);
	}catch(LPTSTR str){
		::LocaleErrorMsgBox(IDS_INVALID_ARGUMENT_FORMAT, str);
	}
}

void CommandLine_SetGamma(LPTSTR input)
{
	double gamma = wstring2double(input);
	gammaController.setGamma(gamma);
}

void CommandLine_Reset()
{
	g_gamma = DEFAULT_GAMMA;
	gammaController.setGamma(DEFAULT_GAMMA);
}


// -monitor-gamma Monitor1=1.0,Monitor2=2.0
// こういうのを変更すればいい
void CommandLine_SetMonitorGamma(LPTSTR input)
{
	// カンマで区切る
	list<wstring> csl = split(wstring(input), L",");

	// 要素にたいして、さらに=で分割
	list<wstring>::iterator it = csl.begin();
	while(it != csl.end()){
		wstring s = wstring((*it).c_str());

		list<wstring> eql = split(s, L"=");
		list<wstring>::iterator l = eql.begin();

		if(eql.size() != 2){
			::LocaleErrorMsgBox(IDS_INVALID_ARGUMENT_FORMAT, input);
			exit(-1);
		}

		wstring monitorName = wstring((*l++).c_str());
		wstring gamma_value = wstring((*l++).c_str());
		double gamma = wstring2double(gamma_value);

		std::tr1::wregex exp(L".*([0-9]+)");
		std::tr1::wsmatch match;
		if( std::tr1::regex_match(monitorName, match, exp) ){
			int index = wstring2int(match.str(1)) - 1;
			gammaController.setMonitorGammaIndex(index, gamma);
		}
		it++;
	}
}

// -monitor-gamma Monitor1=1.0,Monitor2=2.0
// こういうのを変更すればいい
void CommandLine_SetMonitorGammaRGB(LPTSTR input)
{
	// カンマで区切る
	list<wstring> csl = split(wstring(input), L",");

	// 要素にたいして、さらに=で分割
	list<wstring>::iterator it = csl.begin();
	while(it != csl.end()){
		wstring s = wstring((*it).c_str());

		list<wstring> eql = split(s, L"=");
		list<wstring>::iterator l = eql.begin();

		if(eql.size() != 2){
			::LocaleErrorMsgBox(IDS_INVALID_ARGUMENT_FORMAT, input);
			exit(-1);
		}

		wstring monitorName = wstring((*l++).c_str());

		wstring gamma_value = wstring((*l++).c_str());
		//double gamma = wstring2double(gamma_value);

		std::tr1::wregex exp(L".*([0-9]+)");
		std::tr1::wsmatch match;
		if( std::tr1::regex_match(monitorName, match, exp) ){
			int index = wstring2int(match.str(1)) - 1;

			// ガンマ値を取得する
			std::tr1::wregex exp2(L"([0-9.]+):([0-9.]+):([0-9.]+)");
			std::tr1::wsmatch match2;
			if(std::tr1::regex_match(gamma_value, match2, exp2)){
				double r = wstring2double(match2.str(1));
				double g = wstring2double(match2.str(2));
				double b = wstring2double(match2.str(3));

				gammaController.setMonitorGammaIndex(index, r, g, b, (r + g + b) / 3);
			}
		}
		it++;
	}
}

void CommandLine_Parse()
{
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
		::RecognizeMonitors();
		gammaController.reset(); // bug対策

		// なんか引数あり、引数と値のペアの数がちゃんとしてるかチェック
		if( (nArgs - 1) % 2 == 0 ){
			// パラメーターの数があってるので中身が合ってるかとか意味があってるかのチェック
			for(i=1; i<nArgs; i+=2){
				LPTSTR lpOpt = lplpszArgs[i];
				LPTSTR lpStr = lplpszArgs[i+1];
				LPTSTR lpEnd = NULL;

				// 全体のガンマ設定機能
				if(wcscmp(lpOpt, L"-gamma") == 0){
					// resetでリセット
					if(wcscmp(lpStr, L"reset") == 0 || wcscmp(lpStr, L"default") == 0){
						CommandLine_Reset();
					}else{
						CommandLine_SetGamma(lpStr);
					}
					// 個別の色ごとガンマ設定機能
				}else if(wcscmp(lpOpt, L"-gammaRGB") == 0){
					CommandLine_SetGammaRGB(lpStr);
					// 特定のモニタのガンマ設定
				}else if(wcscmp(lpOpt, L"-monitor-gamma") == 0){
					CommandLine_SetMonitorGamma(lpStr);
				}else if(wcscmp(lpOpt, L"-monitor-gammaRGB") == 0){
					CommandLine_SetMonitorGammaRGB(lpStr);
				}
			}
		}else{
      ::LocaleErrorMsgBox(IDS_INVALID_ARGUMENT_NUMBER_FORMAT, nArgs);
			exit(-1);
		}

		// コマンドライン,ショートカットからの実行時は常駐モードへ移行しない
		//::SaveConfig();
		exit(0);
	}
	
	LocalFree(lplpszArgs);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	// メモリリークの検出機能を有効化します
	// 対象となるのはmallocなどの基本的な関数だけで、外部のglobalallocなどは対象ではないことに留意
	//_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
  
  // ロケールの設定
  // 日本だったら日本語、それ以外はすべて英語
  UINT localeId = GetUserDefaultLCID();
  if(0x411 != localeId){
    localeId = 0x409; // 日本語以外は英語UIとして表示する
  }
  ::SetThreadUILanguage(localeId);

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// config.iniを読み込んで設定を反映します
	//LoadConfig();

	CommandLine_Parse();

	// 多重起動防止
	hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
	if(GetLastError() == ERROR_ALREADY_EXISTS){
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
		return FALSE;
	}

	::LoadConfig();

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

	// 構造体メンバの設定
	nid.cbSize           = sizeof( NOTIFYICONDATA );
	nid.uFlags           = (NIF_ICON|NIF_MESSAGE|NIF_TIP);
	nid.hWnd             = hWnd;           // ウインドウ・ハンドル
	nid.hIcon            = LoadIcon(hInst, MAKEINTRESOURCE(IDI_GANMACHANGER));          // アイコン・ハンドル
	nid.uID              = ID_TRAYICON;    // アイコン識別子の定数
	nid.uCallbackMessage = WM_TASKTRAY;    // 通知メッセージの定数
  ::LoadString(::GetModuleHandle(NULL), IDS_TASKTRAY_TOOLTIP_TEXT, nid.szTip, sizeof(nid.szTip));

	// アイコンの変更
	if( !Shell_NotifyIcon( NIM_ADD, (PNOTIFYICONDATAW)&nid ) )
		::ShowLastError();

	ShowWindow(hWnd, SW_HIDE); // ウインドウは非表示
	UpdateWindow(hWnd);

	return TRUE;
}

BOOL SetCursorPosToWindowPos(HWND hWnd)
{
	POINT point;
	::GetCursorPos(&point);

	RECT rect;
	::GetWindowRect(hWnd, &rect);
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	return ::SetWindowPos(hWnd, NULL, point.x - width, point.y - height, 0, 0, SWP_NOSIZE); // マウスの位置に表示する
}

void SetCursorPosToWindowTopMost(HWND hWnd)
{
	::SetWindowTopMost(hWnd);			// 常に最前面に表示
	::SetCursorPosToWindowPos(hWnd);	// カーソルの位置にマウスを移動する
}

// モニタ関係なく、すべてのデスクトップに対するガンマ変更プロシージャ
INT_PTR CALLBACK DlgGammaProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch( msg ){
	case WM_HSCROLL:
		{
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GAMMA) ){
				g_gamma = SLIDER_POS_TO_GAMMA(pos);
				g_gammaR = g_gammaG = g_gammaB = g_gamma;

				// 各モニタのガンマを意識したまま全体のガンマ設定をする
				gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);

				::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
				::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
				::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
				::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);

				// 連動してR,G,Bそれぞれのスライダーも同じように
				SLIDER_SETPOS(hDlg, IDC_SLIDER_RGAMMA, GAMMA_TO_SLIDER_POS(g_gammaR));
				SLIDER_SETPOS(hDlg, IDC_SLIDER_GGAMMA, GAMMA_TO_SLIDER_POS(g_gammaG));
				SLIDER_SETPOS(hDlg, IDC_SLIDER_BGAMMA, GAMMA_TO_SLIDER_POS(g_gammaB));
			}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_RGAMMA) ){
				g_gammaR = SLIDER_POS_TO_GAMMA(pos);
				gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
				::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
			}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GGAMMA) ){
				g_gammaG = SLIDER_POS_TO_GAMMA(pos);
				gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
				::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
			}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_BGAMMA) ){
				g_gammaB = SLIDER_POS_TO_GAMMA(pos);
				gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
				::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);
			}
		}
		break;
	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
		::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
		::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
		::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);

		// スライダーの範囲(0 - 417)
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_GAMMA, 0, SLIDER_SIZE);
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_RGAMMA, 0, SLIDER_SIZE);
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_GGAMMA, 0, SLIDER_SIZE);
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_BGAMMA, 0, SLIDER_SIZE);

		// 現在の設定におけるカーソルの位置を設定する
		SLIDER_SETPOS(hDlg, IDC_SLIDER_GAMMA, GAMMA_TO_SLIDER_POS(g_gamma));
		SLIDER_SETPOS(hDlg, IDC_SLIDER_RGAMMA, GAMMA_TO_SLIDER_POS(g_gammaR));
		SLIDER_SETPOS(hDlg, IDC_SLIDER_GGAMMA, GAMMA_TO_SLIDER_POS(g_gammaG));
		SLIDER_SETPOS(hDlg, IDC_SLIDER_BGAMMA, GAMMA_TO_SLIDER_POS(g_gammaB));

		::SetCursorPosToWindowTopMost(hDlg);
		return TRUE;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK: // 「OK」ボタンが選択された
			gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
			EndDialog(g_hGammaDlg, LOWORD(wp));
			g_hGammaDlg = NULL;
			break;
		case IDCANCEL: // 「キャンセル」ボタンが選択された, ダイアログボックスを消す
			EndDialog(g_hGammaDlg, LOWORD(wp));
			g_hGammaDlg = NULL;
			break;
		case IDDEFAULT:
			g_gamma = g_gammaR = g_gammaG = g_gammaB = DEFAULT_GAMMA;
			gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);

			SLIDER_SETPOS(hDlg, IDC_SLIDER_GAMMA, GAMMA_TO_SLIDER_POS(g_gamma));
			SLIDER_SETPOS(hDlg, IDC_SLIDER_RGAMMA, GAMMA_TO_SLIDER_POS(g_gammaR));
			SLIDER_SETPOS(hDlg, IDC_SLIDER_GGAMMA, GAMMA_TO_SLIDER_POS(g_gammaG));
			SLIDER_SETPOS(hDlg, IDC_SLIDER_BGAMMA, GAMMA_TO_SLIDER_POS(g_gammaB));
			break;
		case IDSHORTCUT: // ショートカット作成
			CreateDesktopShortcutForGammaRGB(g_gammaR, g_gammaG, g_gammaB);
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

// モニタごとのガンマ変更プロシージャ
INT_PTR CALLBACK DlgMonitorGammaProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	static MonitorInfo *monitor = NULL;

	switch( msg ){
	case WM_HSCROLL:
		{
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			double gamma_value = SLIDER_POS_TO_GAMMA(pos);

			if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GAMMA) ){
				// 全体のガンマ調整スライダーいじったとき
				monitor->r = monitor->g = monitor->b = monitor->level = gamma_value;

				::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
				::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
				::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
				::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
				SLIDER_SETPOS(hDlg, IDC_SLIDER_RGAMMA, GAMMA_TO_SLIDER_POS(monitor->r));
				SLIDER_SETPOS(hDlg, IDC_SLIDER_GGAMMA, GAMMA_TO_SLIDER_POS(monitor->g));
				SLIDER_SETPOS(hDlg, IDC_SLIDER_BGAMMA, GAMMA_TO_SLIDER_POS(monitor->b));
			}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_RGAMMA) ){
				monitor->r = gamma_value;
				::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
			}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GGAMMA) ){
				monitor->g = gamma_value;
				::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
			}else if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_BGAMMA) ){
				monitor->b = gamma_value;
				::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
			}
			gammaController.setMonitorGammaIndex(::g_hMonitorTargetIndex, monitor->r, monitor->g, monitor->b, monitor->level);
		}
		return TRUE;

	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		// このGUIにて編集しようとしているモニタの設定を取得し格納する
		monitor = gammaController.monitorGet(::g_hMonitorTargetIndex);

		::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
		::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
		::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
		::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);

		// スライダーの範囲(0 - 417)
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_GAMMA, 0, SLIDER_SIZE);
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_RGAMMA, 0, SLIDER_SIZE);
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_GGAMMA, 0, SLIDER_SIZE);
		SLIDER_SETRANGE(hDlg, IDC_SLIDER_BGAMMA, 0, SLIDER_SIZE);

		// 現在の値のセット
		SLIDER_SETPOS(hDlg, IDC_SLIDER_GAMMA, GAMMA_TO_SLIDER_POS(monitor->level));
		SLIDER_SETPOS(hDlg, IDC_SLIDER_RGAMMA, GAMMA_TO_SLIDER_POS(monitor->r));
		SLIDER_SETPOS(hDlg, IDC_SLIDER_GGAMMA, GAMMA_TO_SLIDER_POS(monitor->g));
		SLIDER_SETPOS(hDlg, IDC_SLIDER_BGAMMA, GAMMA_TO_SLIDER_POS(monitor->b));

		// ウインドウのタイトルをどのモニタかわかるようなものに
    TCHAR windowTitleFormat[256];
    ::LoadString(::GetModuleHandle(NULL), IDS_MONITOR_GAMMA_WINDOW_TITLE_FORMAT, windowTitleFormat, sizeof(windowTitleFormat));
		::SetWindowTextFormat(hDlg, windowTitleFormat, monitor->monitorName);

		// 常に最前面に表示
		::SetCursorPosToWindowTopMost(hDlg);
		return TRUE;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK:       // 適用ボタンが選択された
			{
				double level = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_BRIGHTNESS_LEVEL);

				if(level != monitor->level){
					gammaController.setMonitorGammaIndex(::g_hMonitorTargetIndex, level);
				}else{
					monitor->r = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_EDIT_RGAMMA);
					monitor->g = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_EDIT_GGAMMA);
					monitor->b = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_EDIT_BGAMMA);
					gammaController.setMonitorGammaIndex(g_hMonitorTargetIndex, monitor->r, monitor->g, monitor->b, monitor->level);
				}

				::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
				::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
				::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
				::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);

				// gamma -> slider
				SLIDER_SETPOS(hDlg, IDC_SLIDER_GAMMA, GAMMA_TO_SLIDER_POS(monitor->level));
				SLIDER_SETPOS(hDlg, IDC_SLIDER_RGAMMA, GAMMA_TO_SLIDER_POS(monitor->r));
				SLIDER_SETPOS(hDlg, IDC_SLIDER_GGAMMA, GAMMA_TO_SLIDER_POS(monitor->g));
				SLIDER_SETPOS(hDlg, IDC_SLIDER_BGAMMA, GAMMA_TO_SLIDER_POS(monitor->b));

				EndDialog(g_hMonitorGammaDlg, LOWORD(wp));
				g_hMonitorGammaDlg = NULL;
			}
			break;
		case IDCANCEL:   // 「キャンセル」ボタンが選択された
			// ダイアログボックスを消す
			EndDialog(g_hMonitorGammaDlg, LOWORD(wp));
			g_hMonitorGammaDlg = NULL;
			break;
		case IDDEFAULT:
			// 指定されたインデックスのモニタをデフォルトのガンマ設定に戻す
			gammaController.resetMonitor(::g_hMonitorTargetIndex);

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);

			SLIDER_SETPOS(hDlg, IDC_SLIDER_GAMMA, GAMMA_TO_SLIDER_POS(monitor->level));
			SLIDER_SETPOS(hDlg, IDC_SLIDER_RGAMMA, GAMMA_TO_SLIDER_POS(monitor->r));
			SLIDER_SETPOS(hDlg, IDC_SLIDER_GGAMMA, GAMMA_TO_SLIDER_POS(monitor->g));
			SLIDER_SETPOS(hDlg, IDC_SLIDER_BGAMMA, GAMMA_TO_SLIDER_POS(monitor->b));

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			break;
		case IDSHORTCUT: // ショートカット作成
			CreateDesktopShortcutForMonitorGamma(g_hMonitorTargetIndex, monitor);
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

// キーフックのトグル
BOOL stopOrResume(HWND hWnd)
{
	bStopedFlag = !bStopedFlag;
	if(bStopedFlag){
		StopHook();

		HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);
		nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GANMACHANGER_STOP));
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}else{
		RegistKey(::g_lightUpKeyInfo, WM_GAMMA_UP);
		RegistKey(::g_lightDownKeyInfo, WM_GAMMA_DOWN);
		RegistKey(::g_lightResetKeyInfo, WM_GAMMA_RESET);
		::StartHook();

		HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);
		nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GANMACHANGER));

		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
	return bStopedFlag;
}

// キー設定用、キーフックプロシージャ(not グローバル / グローバルはDLLを利用しなければ行えない)
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wp, LPARAM lp)
{
	//nCodeが0未満のときは、CallNextHookExが返した値を返す
	if (nCode < 0)  return CallNextHookEx(g_hKeyConfigHook,nCode,wp,lp);

	if (nCode==HC_ACTION) {
		//キーの遷移状態のビットをチェックして
		//WM_KEYDOWNとWM_KEYUPをDialogに送信する
		if ( IF_KEY_PRESS(lp) ) {
			PostMessage(g_hKeyConfigDlg,WM_KEYDOWN,wp,lp);
			return TRUE;
		}else{
			PostMessage(g_hKeyConfigDlg,WM_KEYUP,wp,lp);
			return TRUE;
		}
	}
	return CallNextHookEx(g_hKeyConfigHook,nCode,wp,lp);
}

void SetCurrentKeyConfigToGUI(HWND hWnd)
{
	LPTSTR up		= ::GetKeyInfoString(&g_lightUpKeyInfo);
	LPTSTR down		= ::GetKeyInfoString(&g_lightDownKeyInfo);
	LPTSTR reset	= ::GetKeyInfoString(&g_lightResetKeyInfo);

	::SetDlgItemText(hWnd, IDC_EDIT_KEYBIND_LIGHTUP, up);
	::SetDlgItemText(hWnd, IDC_EDIT_KEYBIND_LIGHTDOWN, down);
	::SetDlgItemText(hWnd, IDC_EDIT_KEYBIND_LIGHTRESET, reset);

	::GlobalFree(up);
	::GlobalFree(down);
	::GlobalFree(reset);
}

void SetCurrentKeyConfigToGUI(HWND hWnd, KEYINFO *kup, KEYINFO *kdown, KEYINFO *kreset)
{
	LPTSTR up		= ::GetKeyInfoString(kup);
	LPTSTR down		= ::GetKeyInfoString(kdown);
	LPTSTR reset	= ::GetKeyInfoString(kreset);

	::SetDlgItemText(hWnd, IDC_EDIT_KEYBIND_LIGHTUP, up);
	::SetDlgItemText(hWnd, IDC_EDIT_KEYBIND_LIGHTDOWN, down);
	::SetDlgItemText(hWnd, IDC_EDIT_KEYBIND_LIGHTRESET, reset);

	::GlobalFree(up);
	::GlobalFree(down);
	::GlobalFree(reset);
}

INT_PTR CALLBACK DlgKeyConfigProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT targetID = -1;
	BYTE keyTbl[256];
	int optKey = 0;

	// 一時格納用バッファ
	static KEYINFO prevKeyInfo = {0};
	static KEYINFO nextKeyInfo = {0};
	static KEYINFO resetKeyInfo = {0};

	switch( msg ){
	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		::SetCursorPosToWindowTopMost(hDlg);(hDlg); // ウインドウを最前面にします
		::SetCurrentKeyConfigToGUI(hDlg); // 現在のキー設定をGUI上に反映させます

		// 一時格納用バッファを初期化します
		nextKeyInfo = g_lightUpKeyInfo;
		prevKeyInfo = g_lightDownKeyInfo;
		resetKeyInfo = g_lightResetKeyInfo;

		// ウインドウのタイトルを規定のものに設定します
    TCHAR windowTitle[256];
    ::LoadString(::GetModuleHandle(NULL), IDS_KEYCONFIG_DEFAULT_WINDOW_TITLE, windowTitle, sizeof(windowTitle));
		::SetWindowText(hDlg, windowTitle);
		return TRUE;

	case WM_KEYDOWN:
		if( !::GetKeyboardState((PBYTE)&keyTbl) ){
			ShowLastError();
			exit(-1);
		}

		// 入力された補助キーを判断して代入
		KEYINFO tmp;
		::ClearKeyInfo(&tmp);
		if( keyTbl[VK_CONTROL] & 0x80 ){
			// wpと一緒だったらwp使えばいいので入力しません
			if(wp != VK_CONTROL)
				tmp.ctrlKey = VK_CONTROL;
		}
		if( keyTbl[VK_SHIFT] & 0x80 ){
			if(wp != VK_SHIFT)
				tmp.shiftKey = VK_SHIFT;
		}
		if( keyTbl[VK_MENU] & 0x80 ){
			if(wp != VK_MENU)
				tmp.altKey = VK_MENU;
		}
		tmp.key = wp;

		// 入力されたキーをUI上に反映させます
		{
			LPTSTR lpKeyConfigBuffer = ::GetKeyInfoString(&tmp);
			::SetDlgItemText(hDlg, targetID, lpKeyConfigBuffer);
			::GlobalFree(lpKeyConfigBuffer);
		}

		if(targetID == IDC_EDIT_KEYBIND_LIGHTUP){
			nextKeyInfo = tmp;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTDOWN){
			prevKeyInfo = tmp;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTRESET){
			resetKeyInfo = tmp;
		}
		return TRUE;

	case WM_KEYUP:
    if(targetID == IDC_EDIT_KEYBIND_LIGHTUP ||
      targetID == IDC_EDIT_KEYBIND_LIGHTDOWN ||
      targetID == IDC_EDIT_KEYBIND_LIGHTRESET){
        UINT objectID = 0;
        if(targetID == IDC_EDIT_KEYBIND_LIGHTUP){
          objectID = ID_KEYBIND_LIGHTUP;
        }else if(targetID == IDC_EDIT_KEYBIND_LIGHTDOWN){
          objectID = ID_KEYBIND_LIGHTDOWN;
        }else if(targetID == IDC_EDIT_KEYBIND_LIGHTRESET){
          objectID = ID_KEYBIND_LIGHTRESET;
        }

        TCHAR buttonName[256];
        ::LoadString(::GetModuleHandle(NULL), IDS_KEYCONFIG_BUTTON_NAME_SET, buttonName, sizeof(buttonName));
        ::SetDlgItemText(hDlg, objectID, buttonName);
			  ::UnhookWindowsHookEx(g_hKeyConfigHook);

        TCHAR windowTitle[256];
        ::LoadString(::GetModuleHandle(NULL), IDS_KEYCONFIG_DEFAULT_WINDOW_TITLE, windowTitle, sizeof(windowTitle));
    	  ::SetWindowText(hDlg, windowTitle);
			  g_hKeyConfigHook = NULL;
    }
		break;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK:       // 適用ボタンが選択された
			::StopHook();

			::g_lightUpKeyInfo = nextKeyInfo;
			::g_lightDownKeyInfo = prevKeyInfo;
			::g_lightResetKeyInfo = resetKeyInfo;

			RegistKey(g_lightUpKeyInfo, WM_GAMMA_UP);
			RegistKey(g_lightDownKeyInfo, WM_GAMMA_DOWN);
			RegistKey(g_lightResetKeyInfo, WM_GAMMA_RESET);
			if(!::StartHook())
				::ShowLastError();

			EndDialog(g_hKeyConfigDlg, LOWORD(wp));
			g_hKeyConfigDlg = NULL;
			break;
		case IDCANCEL:   // 「キャンセル」ボタンが選択された
			// ダイアログボックスを消す
			EndDialog(g_hKeyConfigDlg, LOWORD(wp));
			g_hKeyConfigDlg = NULL;
			break;
		case IDDEFAULT: // デフォルトボタンが押されたとき
			// setup default key config
			::QuickSetKeyInfo(&nextKeyInfo, VK_CONTROL, VK_PRIOR);
			::QuickSetKeyInfo(&prevKeyInfo, VK_CONTROL, VK_NEXT);
			::QuickSetKeyInfo(&resetKeyInfo, VK_CONTROL, VK_HOME);

			// 現在のキー設定をGUIに反映します
			SetCurrentKeyConfigToGUI(hDlg, &nextKeyInfo, &prevKeyInfo, &resetKeyInfo);
			break;
    default:
      // ボタン個別の処理
      UINT objectID = 0;
      if(LOWORD(wp) == ID_KEYBIND_LIGHTUP){
			  targetID = IDC_EDIT_KEYBIND_LIGHTUP;
        objectID = ID_KEYBIND_LIGHTUP;
      }else if(LOWORD(wp) == ID_KEYBIND_LIGHTDOWN){
 			  targetID = IDC_EDIT_KEYBIND_LIGHTDOWN;
        objectID = ID_KEYBIND_LIGHTDOWN;
      }else if(LOWORD(wp) == ID_KEYBIND_LIGHTRESET){
 			  targetID = IDC_EDIT_KEYBIND_LIGHTRESET;
        objectID = ID_KEYBIND_LIGHTRESET;
      }

      // 各ボタン共通の処理
      if(objectID != 0){
        if(::g_hKeyConfigHook == NULL)
				  g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));

        TCHAR windowTitle[256];
        ::LoadString(::GetModuleHandle(NULL), IDS_KEYCONFIG_INPUT_WINDOW_TITLE, windowTitle, sizeof(windowTitle));
			  ::SetWindowText(hDlg, windowTitle);

        TCHAR buttonName[256];
        ::LoadString(::GetModuleHandle(NULL), IDS_KEYCONFIG_BUTTON_NAME_INPUT, buttonName, sizeof(buttonName));
        ::SetDlgItemText(hDlg, objectID, buttonName);
      }
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	HMENU hMenu = NULL;
	HMENU hSubMenu = NULL;
	static UINT wm_taskbarCreated;
	MENUITEMINFO mii;
	static UINT_PTR timer = NULL;
	static HWND hTool;
	static TOOLINFO ti;

	switch (message)
	{
	case WM_CREATE:
		// モニタの種類、数を認識する
		SetWindowHandle(hWnd);

		// 一番最初のキーフック
		// デフォルト値で起動する
		::RegistKey(::g_lightUpKeyInfo, WM_GAMMA_UP);
		::RegistKey(::g_lightDownKeyInfo, WM_GAMMA_DOWN);
		::RegistKey(::g_lightResetKeyInfo, WM_GAMMA_RESET);
		if(!::StartHook())
			::ShowLastError();
		::trace(L"starthook\n");

		// taskbar event
		wm_taskbarCreated = RegisterWindowMessage(TEXT("TaskbarCreated"));
		break;
	case WM_TASKTRAY:
		switch(lParam){
		case WM_LBUTTONDOWN:
			// シングルクリックで中断 <-> 再開
      if(!g_hGammaDlg)
        g_hGammaDlg = CreateDialog(hInst, TEXT("IDD_GAMMA_DIALOG"), hWnd, &DlgGammaProc);
			break;
		case WM_LBUTTONDBLCLK:
			break;
		case WM_RBUTTONDOWN:
			SetForegroundWindow(hWnd);

			hMenu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU));
			hSubMenu = GetSubMenu(hMenu, 0);

			// 現在の選択状態をタスクバーに表示
			if(bStopedFlag){
				CheckMenuItem(hSubMenu, IDM_STOP, MF_BYCOMMAND | MF_CHECKED);
			}else{
				CheckMenuItem(hSubMenu, IDM_STOP, MF_BYCOMMAND | MF_UNCHECKED);
			}

			// モニタが複数あったら、モニタごとの調整メニューを表示するように
			if( gammaController.hasMultiMonitor() ){
				for(int i=0; i<gammaController.monitorGetCount(); i++){
				  mii.cbSize = sizeof(MENUITEMINFO);
          mii.fType = MFT_STRING;
          mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID;
          //mii.dwTypeData = ::sprintf_alloc(L"%s調節", gammaController.monitorGet(i)->monitorName);
          
          TCHAR format[256];
          ::LoadString(::GetModuleHandle(NULL), IDS_MONITOR_CONTROL_MENU_FORMAT, format, sizeof(format));

          mii.dwTypeData = ::sprintf_alloc(format, gammaController.monitorGet(i)->monitorName);
				  mii.wID = IDM_MONITOR + i;
				  InsertMenuItem(hSubMenu, i, TRUE, &mii);
        }
			}

			// カーソルの現在位置を取得
			POINT point;
			::GetCursorPos(&point);

			TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL);
			PostMessage(hWnd, WM_NULL, 0, 0);
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
			CreateDesktopShortcutForGamma(g_gamma);
			break;

		case IDM_RESET:
			g_gammaR = g_gammaG = g_gammaB = g_gamma = DEFAULT_GAMMA;
			gammaController.reset();
			break;

		case IDM_STOP:
			stopOrResume(hWnd);
			break;

		case IDM_KEYBOARD:
			if(!g_hKeyConfigDlg)
				g_hKeyConfigDlg = CreateDialog(hInst, TEXT("IDD_KEYCONFIG_DIALOG"), hWnd, &DlgKeyConfigProc);
			break;

		default:
			if(IDM_MONITOR <= wmId && wmId <= IDM_MONITOR + MAX_MONITOR_NUMBER){
				int monitorId = wmId - IDM_MONITOR;
				g_hMonitorTargetIndex = monitorId;

				// モニタのIDがわかったのでそいつを対象としたダイアログ表示
				if(!g_hMonitorGammaDlg)
					g_hMonitorGammaDlg = CreateDialog(hInst, TEXT("IDD_GAMMA_DIALOG"), hWnd, DlgMonitorGammaProc);
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_GAMMA_UP:
		if( IF_KEY_PRESS(lParam) ){
			gammaController.incrementAtCursorPos();
		}
		break;

	case WM_GAMMA_DOWN:
		if( IF_KEY_PRESS(lParam) ){
		  gammaController.decrementAtCursorPos();
		}
		break;

	case WM_GAMMA_RESET:
		if( IF_KEY_PRESS(lParam) ){
      gammaController.resetMonitorAtCursorPos();
    }
		break;

	case WM_CLOSE:
		SetMenu(hWnd, NULL);
		DestroyMenu(hMenu);
		hMenu = NULL;
		break;

  case WM_NCDESTROY:
		//::TasktrayDeleteIcon(hWnd, ID_TRAYICON);
		if(!::Shell_NotifyIcon(NIM_DELETE, &nid)){
      ::ShowLastError();
    }

		SetMenu(hWnd, NULL);
		DestroyMenu(hMenu);
		hMenu = NULL;
    break;

	case WM_DESTROY:
		SaveConfig();
		StopHook();

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
