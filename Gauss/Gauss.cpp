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

/*
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
*/

// for x64 environment
#ifdef _WIN64
#undef GWL_HINSTANCE
#define GWL_HINSTANCE GWLP_HINSTANCE
#undef GetWindowLong
#define GetWindowLong GetWindowLongPtr
#endif

#define MUTEX_NAME L"Gauss"
#define TASKTRAY_TOOLTIP_TEXT L"ガンマ値変更ツール"

#define MAX_LOADSTRING 100
#define WM_USER_MESSAGE (WM_USER+0x1000)
#define SLIDER_SIZE 100

#define DLG_KEYCONFIG_PROC_WINDOW_TITLE L"キー設定"
#define DLG_KEYCONFIG_ASK L"キーを入力してください"
#define DLG_KEYCONFIG_ASK_BUTTON_TITLE L"入力"
#define DLG_KEYCONFIG_DEFAULT_BUTTON_TITLE L"設定"
#define DLG_MONITOR_GAMMA_WINDOW_TITLE_FORMAT L"%sのガンマ調節"

// 複数のアイコンを識別するためのID定数
#define ID_TRAYICON  (1)

// タスクトレイのマウスメッセージの定数
#define WM_TASKTRAY  (WM_APP + 1)

#define WM_GAMMA_UP (WM_USER_MESSAGE + 1)
#define WM_GAMMA_DOWN (WM_USER_MESSAGE + 2)
#define WM_GAMMA_RESET (WM_USER_MESSAGE + 3)

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

HDC g_test = (HDC)INVALID_HANDLE_VALUE;

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// KEYINFO構造体を文字列表現にします
LPTSTR GetKeyInfoString(KEYINFO *keyInfo)
{
	LPTSTR alt, ctrl, shift, key;
	alt = ctrl = shift = key = NULL;

	if(keyInfo->altKey != KEY_NOT_SET)
		alt		= ::GetKeyNameTextEx(keyInfo->altKey);
	if(keyInfo->ctrlKey != KEY_NOT_SET)
		ctrl	= ::GetKeyNameTextEx(keyInfo->ctrlKey);
	if(keyInfo->shiftKey != KEY_NOT_SET)
		shift	= ::GetKeyNameTextEx(keyInfo->shiftKey);
	if(keyInfo->key != KEY_NOT_SET)
		key		= ::GetKeyNameTextEx(keyInfo->key);

	LPTSTR buffer = NULL;
	if(alt == NULL && ctrl == NULL && shift == NULL && key == NULL){
		buffer = ::sprintf_alloc(L"");
	}else if(alt == NULL && ctrl == NULL && shift == NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s", key);
	}else if(alt == NULL && ctrl == NULL && shift != NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s + %s", shift, key);
	}else if(alt == NULL && ctrl != NULL && shift == NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s + %s", ctrl, key);
	}else if(alt != NULL && ctrl == NULL && shift == NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s + %s", alt, key);
	}else if(alt == NULL && ctrl != NULL && shift != NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s + %s + %s", ctrl, shift, key);
	}else if(alt != NULL && ctrl == NULL && shift != NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s + %s + %s", alt, shift, key);
	}else if(alt != NULL && ctrl != NULL && shift == NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s + %s + %s", ctrl, alt, key);
	}else if(alt != NULL && ctrl != NULL && shift != NULL && key != NULL){
		buffer = ::sprintf_alloc(L"%s + %s + %s + %s", ctrl, alt, shift, key);
	}else{
		buffer = ::sprintf_alloc(L"undef!");
		::ErrorMessageBox(L"キー設定に失敗しました");
	}

	::GlobalFree(alt);
	::GlobalFree(ctrl);
	::GlobalFree(shift);
	::GlobalFree(key);
	return buffer;
}

BOOL CreateDesktopShortcutForGamma(double gamma)
{
	TCHAR desktopPath[MAX_PATH];
	if( !::GetDesktopPath(desktopPath, MAX_PATH) )
		return FALSE;

	//実行中のプロセスのフルパス名を取得する
	TCHAR szPath[MAX_PATH];
	if( GetModuleFileName(NULL, (LPWSTR)szPath, sizeof(szPath)) == 0 ){
		return FALSE;
	}

	TCHAR linkPath[MAX_PATH];
	TCHAR gammaOption[MAX_PATH];
	::_stprintf_s(linkPath, L"%s\\ガンマ%.2f.lnk", desktopPath, gamma);
	::_stprintf_s(gammaOption, L"-gamma %.1f", gamma);

	::CoInitialize(NULL);
	BOOL result = CreateShortcut(szPath, gammaOption, desktopPath, 0, (LPCSTR)linkPath);
	::CoUninitialize();

	return result;
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
		LPTSTR monitorName = sprintf_alloc(L"モニタ%d", gammaController.monitorGetCount() + 1);

		// 取得した情報を設定します
		gammaController.monitorAdd(hDC, deviceName, monitorName);

		return TRUE;
}

// モニタの数と名前を再認識する
void RecognizeMonitors(void)
{
	gammaController.monitorReset();
	EnumDisplayMonitors(::GetDC(NULL), NULL, MonitorEnumProc, 0);
}

// 設定ファイルのパスを取得します
// 実行ファイルのディレクトリに、config.ini(デフォルト)を付加した物になります
// 動的にバッファを格納して返却するので、解放必須です
LPTSTR GetConfigPath(LPTSTR fileName=L"config.ini")
{
	LPTSTR lpExecDirectory = (LPTSTR)::GlobalAlloc(GMEM_FIXED, MAX_PATH * sizeof(TCHAR));
	if( ::GetExecuteDirectory(lpExecDirectory, MAX_PATH) ) {
		LPTSTR lpConfigPath = sprintf_alloc(L"%s%s", lpExecDirectory, fileName);
		::GlobalFree(lpExecDirectory);
		return lpConfigPath;
	} else {
		::GlobalFree(lpExecDirectory);
		::ShowLastError();
		return NULL;
	}
}

void LoadConfig(void)
{
	LPTSTR lpConfigPath = ::GetConfigPath();

	// clear keyinfo
	::ClearKeyInfo(&g_lightUpKeyInfo);
	::ClearKeyInfo(&g_lightDownKeyInfo);
	::ClearKeyInfo(&g_lightResetKeyInfo);

	// lightup keyconfig
	::g_lightUpKeyInfo.key		= ::GetPrivateProfileInt(L"KeyBind", L"lightUpKey", ::g_lightUpKeyInfo.key, lpConfigPath);
	::g_lightUpKeyInfo.ctrlKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightUpCtrlKey", ::g_lightUpKeyInfo.ctrlKey, lpConfigPath);
	::g_lightUpKeyInfo.shiftKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightUpShiftKey", ::g_lightUpKeyInfo.shiftKey, lpConfigPath);
	::g_lightUpKeyInfo.altKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightUpAltKey", ::g_lightUpKeyInfo.altKey, lpConfigPath);

	// lightdown keyconfig
	::g_lightDownKeyInfo.key		= ::GetPrivateProfileInt(L"KeyBind", L"lightDownKey", ::g_lightDownKeyInfo.key, lpConfigPath);
	::g_lightDownKeyInfo.ctrlKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightDownCtrlKey", ::g_lightDownKeyInfo.ctrlKey, lpConfigPath);
	::g_lightDownKeyInfo.shiftKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightDownShiftKey", ::g_lightDownKeyInfo.shiftKey, lpConfigPath);
	::g_lightDownKeyInfo.altKey		= ::GetPrivateProfileInt(L"KeyBind", L"lightDownAltKey", ::g_lightDownKeyInfo.altKey, lpConfigPath);

	// lightreset keyconfig
	::g_lightResetKeyInfo.key		= ::GetPrivateProfileInt(L"KeyBind", L"lightResetKey", ::g_lightResetKeyInfo.key, lpConfigPath);
	::g_lightResetKeyInfo.ctrlKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightResetCtrlKey", ::g_lightResetKeyInfo.ctrlKey, lpConfigPath);
	::g_lightResetKeyInfo.shiftKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightResetShiftKey", ::g_lightResetKeyInfo.shiftKey, lpConfigPath);
	::g_lightResetKeyInfo.altKey	= ::GetPrivateProfileInt(L"KeyBind", L"lightResetAltKey", ::g_lightResetKeyInfo.altKey, lpConfigPath);

	::g_gamma = ::GetPrivateProfileDouble(L"Gamma", L"gamma", DEFAULT_GAMMA, lpConfigPath);
	::g_gammaR = ::GetPrivateProfileDouble(L"Gamma", L"gammaR", DEFAULT_GAMMA, lpConfigPath);
	::g_gammaG = ::GetPrivateProfileDouble(L"Gamma", L"gammaG", DEFAULT_GAMMA, lpConfigPath);
	::g_gammaB = ::GetPrivateProfileDouble(L"Gamma", L"gammaB", DEFAULT_GAMMA, lpConfigPath);

	::GlobalFree(lpConfigPath);
}

void SaveConfig(void)
{
	// プログラム(実行ファイル)のあるフォルダに設定を保存します
	LPTSTR lpConfigPath = ::GetConfigPath();

	// lightup keyconfigs
	::WritePrivateProfileInt(L"KeyBind", L"lightUpKey", ::g_lightUpKeyInfo.key, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightUpCtrlKey", ::g_lightUpKeyInfo.ctrlKey, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightUpShiftKey", ::g_lightUpKeyInfo.shiftKey, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightUpAltKey", ::g_lightUpKeyInfo.altKey, lpConfigPath);

	// lightdown keyconfig
	::WritePrivateProfileInt(L"KeyBind", L"lightDownKey", ::g_lightDownKeyInfo.key, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightDownCtrlKey", ::g_lightDownKeyInfo.ctrlKey, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightDownShiftKey", ::g_lightDownKeyInfo.shiftKey, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightDownAltKey", ::g_lightDownKeyInfo.altKey, lpConfigPath);

	// lightreset keyconfig
	::WritePrivateProfileInt(L"KeyBind", L"lightResetKey", ::g_lightResetKeyInfo.key, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightResetCtrlKey", ::g_lightResetKeyInfo.ctrlKey, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightResetShiftKey", ::g_lightResetKeyInfo.shiftKey, lpConfigPath);
	::WritePrivateProfileInt(L"KeyBind", L"lightResetAltKey", ::g_lightResetKeyInfo.altKey, lpConfigPath);

	::WritePrivateProfileDouble(L"Gamma", L"gamma", ::g_gamma, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaR", ::g_gammaR, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaG", ::g_gammaG, lpConfigPath);
	::WritePrivateProfileDouble(L"Gamma", L"gammaB", ::g_gammaB, lpConfigPath);

	::GlobalFree(lpConfigPath);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	// メモリリークの検出機能を有効化します
	// 対象となるのはmallocなどの基本的な関数だけで、外部のglobalallocなどは対象ではないことに留意
	//_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

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
						g_gamma = DEFAULT_GAMMA;
						gammaController.reset();
					}else{
						g_gamma = ::wcstod(lpStr, &lpEnd);
						if(g_gamma == 0 && lpStr == lpEnd){
							ErrorMessageBox(L"Format Error");
							exit(-1);
						}
						gammaController.setGamma(g_gamma);
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

	// ガンマの変更テスト
	g_test = ::GetDC(hWnd);

	// 構造体メンバの設定
	nid.cbSize           = sizeof( NOTIFYICONDATA );
	nid.uFlags           = (NIF_ICON|NIF_MESSAGE|NIF_TIP);
	nid.hWnd             = hWnd;           // ウインドウ・ハンドル
	nid.hIcon            = LoadIcon(hInst, MAKEINTRESOURCE(IDI_GANMACHANGER));          // アイコン・ハンドル
	nid.uID              = ID_TRAYICON;    // アイコン識別子の定数
	nid.uCallbackMessage = WM_TASKTRAY;    // 通知メッセージの定数

	lstrcpy( nid.szTip, TASKTRAY_TOOLTIP_TEXT );  // チップヘルプの文字列

	// アイコンの変更
	if( !Shell_NotifyIcon( NIM_ADD, (PNOTIFYICONDATAW)&nid ) )
		::ShowLastError();

	//ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

// モニタ関係なく、すべてのデスクトップに対するガンマ変更プロシージャ
INT_PTR CALLBACK DlgGammaProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	BOOL result = false;
	double level = 0;
	int delta = 0;

	switch( msg ){
	case WM_HSCROLL:
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gamma = pos * MAX_GAMMA / SLIDER_SIZE;
			g_gammaR = g_gammaG = g_gammaB = g_gamma;

			// 各モニタのガンマを意識したまま全体のガンマ設定をする
			gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);
			::SendDlgItemMessage(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (MAX_GAMMA / SLIDER_SIZE)));
		}
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_RGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gammaR = pos * MAX_GAMMA / SLIDER_SIZE;
			gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
		}
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gammaG = pos * MAX_GAMMA / SLIDER_SIZE;
			gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
		}
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_BGAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gammaB = pos * MAX_GAMMA / SLIDER_SIZE;
			gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);
		}
		break;
	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
		::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
		::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
		::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);

		::SendDlgItemMessage(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(g_gamma / (MAX_GAMMA / SLIDER_SIZE)));
		::SendDlgItemMessage(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (MAX_GAMMA / SLIDER_SIZE)));
		::SendDlgItemMessage(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (MAX_GAMMA / SLIDER_SIZE)));
		::SendDlgItemMessage(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (MAX_GAMMA / SLIDER_SIZE)));

		::SetWindowTopMost(hDlg); // 常に最前面に表示
		return TRUE;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK: // 「OK」ボタンが選択された
			gammaController.setMonitorGammaDifference(g_gammaR, g_gammaG, g_gammaB);
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
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(g_gamma / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (MAX_GAMMA / SLIDER_SIZE)));

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
			break;
		case IDSHORTCUT: // ショートカット作成
			CreateDesktopShortcutForGamma(g_gamma);
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
	BOOL result = false;
	double level = 0;
	int delta = 0;
	int pos;
	double gamma_value;

	switch( msg ){
	case WM_HSCROLL:
		pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
		gamma_value = (pos * MAX_GAMMA / SLIDER_SIZE);

		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GAMMA) ){
			// 全体のガンマ調整スライダーいじったとき
			monitor->r = monitor->g = monitor->b = monitor->level = gamma_value;

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);
			::SendDlgItemMessage(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));
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
		return TRUE;

	case WM_INITDIALOG:  // ダイアログボックスが作成されたとき
		// このGUIにて編集しようとしているモニタの設定を取得し格納する
		monitor = gammaController.monitorGet(::g_hMonitorTargetIndex);

		::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, monitor->level);
		::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, monitor->r);
		::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, monitor->g);
		::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, monitor->b);

		::SendDlgItemMessage(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(monitor->level / (MAX_GAMMA / SLIDER_SIZE)));
		::SendDlgItemMessage(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
		::SendDlgItemMessage(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
		::SendDlgItemMessage(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));

		// ウインドウのタイトルをどのモニタかわかるようなものに
		::SetWindowTextFormat(hDlg, DLG_MONITOR_GAMMA_WINDOW_TITLE_FORMAT, monitor->monitorName);

		// 常に最前面に表示
		::SetWindowTopMost(hDlg);
		return TRUE;

	case WM_COMMAND:     // ダイアログボックス内の何かが選択されたとき
		switch( LOWORD( wp ) ){
		case IDOK:       // 適用ボタンが選択された
			level = GetDlgItemDouble(g_hMonitorGammaDlg, IDC_BRIGHTNESS_LEVEL);

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
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(monitor->level / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));
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
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(monitor->level / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(monitor->r / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(monitor->g / (MAX_GAMMA / SLIDER_SIZE)));
			::SendDlgItemMessage(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(monitor->b / (MAX_GAMMA / SLIDER_SIZE)));
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
		RestartHook();
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
		::SetWindowTopMost(hDlg); // ウインドウを最前面にします
		::SetCurrentKeyConfigToGUI(hDlg); // 現在のキー設定をGUI上に反映させます

		// 一時格納用バッファを初期化します
		nextKeyInfo = g_lightUpKeyInfo;
		prevKeyInfo = g_lightDownKeyInfo;
		resetKeyInfo = g_lightResetKeyInfo;

		// ウインドウのタイトルを規定のものに設定します
		::SetWindowText(hDlg, DLG_KEYCONFIG_PROC_WINDOW_TITLE);
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
		if(targetID == IDC_EDIT_KEYBIND_LIGHTUP){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTUP, DLG_KEYCONFIG_DEFAULT_BUTTON_TITLE);
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			::SetWindowText(hDlg, DLG_KEYCONFIG_PROC_WINDOW_TITLE);
			g_hKeyConfigHook = NULL;

		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTDOWN){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTDOWN, DLG_KEYCONFIG_DEFAULT_BUTTON_TITLE);
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			::SetWindowText(hDlg, DLG_KEYCONFIG_PROC_WINDOW_TITLE);
			g_hKeyConfigHook = NULL;

		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTRESET){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTRESET, DLG_KEYCONFIG_DEFAULT_BUTTON_TITLE);
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			::SetWindowText(hDlg, DLG_KEYCONFIG_PROC_WINDOW_TITLE);
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
			::StartHook();

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
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTUP, DLG_KEYCONFIG_ASK_BUTTON_TITLE);
			::SetWindowText(hDlg, DLG_KEYCONFIG_ASK);
			break;
		case ID_KEYBIND_LIGHTDOWN:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTDOWN;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTDOWN, DLG_KEYCONFIG_ASK_BUTTON_TITLE);
			::SetWindowText(hDlg, DLG_KEYCONFIG_ASK);
			break;
		case ID_KEYBIND_LIGHTRESET:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTRESET;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTRESET, DLG_KEYCONFIG_ASK_BUTTON_TITLE);
			::SetWindowText(hDlg, DLG_KEYCONFIG_ASK);
			break;
		case IDDEFAULT: // デフォルトボタンが押されたとき
			nextKeyInfo.key = VK_PRIOR;
			nextKeyInfo.ctrlKey = VK_CONTROL;
			nextKeyInfo.altKey = nextKeyInfo.shiftKey = KEY_NOT_SET;
			nextKeyInfo.message = WM_GAMMA_UP;

			prevKeyInfo.key = VK_NEXT;
			prevKeyInfo.ctrlKey = VK_CONTROL;
			prevKeyInfo.altKey = prevKeyInfo.shiftKey = KEY_NOT_SET;
			prevKeyInfo.message = WM_GAMMA_DOWN;

			resetKeyInfo.key = VK_HOME;
			resetKeyInfo.ctrlKey = VK_CONTROL;
			resetKeyInfo.altKey = resetKeyInfo.shiftKey = KEY_NOT_SET;
			resetKeyInfo.message = WM_GAMMA_RESET;

			// 現在のキー設定をGUIに反映します
			SetCurrentKeyConfigToGUI(hDlg, &nextKeyInfo, &prevKeyInfo, &resetKeyInfo);
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	HMENU hMenu = NULL;
	HMENU hSubMenu = NULL;
	static UINT wm_taskbarCreated;
	HMENU hView;
	MENUITEMINFO mii;
	static UINT_PTR timer = NULL;
	static HWND hTool;
	static TOOLINFO ti;

	switch (message)
	{
	case WM_CREATE:
		// モニタの種類、数を認識する
		RecognizeMonitors();

		SetWindowHandle(hWnd);

		// 一番最初のキーフック
		// デフォルト値で起動する
		::RegistKey(::g_lightUpKeyInfo, WM_GAMMA_UP);
		::RegistKey(::g_lightDownKeyInfo, WM_GAMMA_DOWN);
		::RegistKey(::g_lightResetKeyInfo, WM_GAMMA_RESET);
		::StartHook();

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

			// モニタが複数あったら、モニタごとの調整メニューを表示するように
			if( gammaController.hasMultiMonitor() ){
				hView = CreatePopupMenu();
				mii.wID = IDM_MONITOR;
				mii.cbSize = sizeof(MENUITEMINFO);
				mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU;
				mii.fType = MFT_STRING;
				mii.hSubMenu = hView;
				mii.dwTypeData = L"モニタ別調節";
				InsertMenuItem(hSubMenu, 0, TRUE, &mii);

				for(int i=0; i<gammaController.monitorGetCount(); i++){
					mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID;
					mii.dwTypeData = gammaController.monitorGet(i)->monitorName;
					mii.wID = IDM_MONITOR + i; // 2500 - 2600 reserved for monitors
					InsertMenuItem(hView, i, TRUE, &mii);
				}
			}

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
#define IF_KEY_PRESS(lp) ((lParam & (1 << 31)) == 0)
	case WM_GAMMA_UP:
		if( IF_KEY_PRESS(lParam) ){
			trace(L"gamma up\n");
			gammaController.increment();
		}
		break;
	case WM_GAMMA_DOWN:
		if( IF_KEY_PRESS(lParam) ){
			trace(L"gamma down\n");
			gammaController.decrement();
		}
		break;
	case WM_GAMMA_RESET:
		if( IF_KEY_PRESS(lParam) ){
			trace(L"gamma reset\n");
			gammaController.reset();
		}
		break;
	case WM_CLOSE:
		SetMenu(hWnd, NULL);
		DestroyMenu(hMenu);
		hMenu = NULL;
		break;
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
