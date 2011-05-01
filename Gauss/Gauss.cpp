// GanmaChanger.cpp : �A�v���P�[�V�����̃G���g�� �|�C���g���`���܂��B
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

#include "Util.h"

#define MUTEX_NAME L"Gauss"
#define TASKTRAY_TOOLTIP_TEXT L"�K���}�l�ύX�c�[��"

#define MAX_LOADSTRING 100
#define WM_USER_MESSAGE (WM_USER+0x1000)

#define GAMMA_INCREMENT_VALUE 0.1
#define GAMMA_DECREMENT_VALUE 0.1
#define GAMMA_DEFAULT_VALUE 1.0
#define GAMMA_MIN_VALUE 0.0
#define GAMMA_MAX_VALUE 5.0

#define MAX_GAMMA 5.0
#define MIN_GAMMA 0.0
#define DEFAULT_GAMMA 1.0
#define SLIDER_SIZE 100

// �����̃A�C�R�������ʂ��邽�߂�ID�萔
#define ID_TRAYICON  (1)

// �^�X�N�g���C�̃}�E�X���b�Z�[�W�̒萔
#define WM_TASKTRAY  (WM_APP + 1)

typedef struct {
	double r;		// ��
	double g;		// ��
	double b;		// ��
	double level;	// ���邳���x��(���݂ł͖��g�p)

	HDC hDC;		// ���j�^�̃f�o�C�X�R���e�L�X�g
	UINT monitorID;	// ���j�^�̓����Ǘ�ID(window���b�Z�[�W�Ƃ̊֘A�t���Ɏg�p)

	LPWSTR monitorName;	// ���j�^�̖��O
	LPWSTR deviceName;	// ���j�^�̃f�o�C�X�p�X
} MonitorInfo;

HINSTANCE hInst;								// ���݂̃C���^�[�t�F�C�X
TCHAR szTitle[MAX_LOADSTRING];					// �^�C�g�� �o�[�̃e�L�X�g
TCHAR szWindowClass[MAX_LOADSTRING];			// ���C�� �E�B���h�E �N���X��
HANDLE hMutex;
NOTIFYICONDATA nid = { 0 };
double g_gamma = 1.0;
bool bStopedFlag = false;

double g_gammaR = 1.0;
double g_gammaG = 1.0;
double g_gammaB = 1.0;

int g_hMonitorTargetIndex;
#define MAX_MONITOR_NUMBER 32
MonitorInfo monitorInfoList[MAX_MONITOR_NUMBER];

HWND g_hDlg;
HWND g_hGammaDlg;
HWND g_hMonitorGammaDlg;
HWND g_hKeyConfigDlg;

HDC g_deviceContexts[256];
LPWSTR g_deviceNames[256];
int g_deviceContextCounter = 0;

int g_lightUpKey = VK_PRIOR;
int g_lightDownKey = VK_NEXT;
int g_lightResetKey = VK_HOME;
int g_lightOptKey = VK_CONTROL;

HHOOK g_hKeyConfigHook = NULL;

// ���̃R�[�h ���W���[���Ɋ܂܂��֐��̐錾��]�����܂�:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// �w��̃f�o�C�X�R���e�L�X�g�̃K���}��ύX����
BOOL SetGammaCorrectMonitorRGB(HDC hdc, double gammaR, double gammaG, double gammaB)
{
	return ::SetMonitorGamma(hdc, gammaR, gammaG, gammaB);
}

BOOL SetGammaCorrectRGB(double gammaR, double gammaG, double gammaB)
{
	return ::SetGamma(gammaR, gammaG, gammaB);
	//return ::SetGammaCorrectMonitorRGB(::GetDC(NULL), gammaR, gammaG, gammaB);
}

BOOL SetGammaCorrect(double gamma)
{
	return ::SetGamma(gamma);
	//return SetGammaCorrectRGB(gamma, gamma, gamma);
}

// �e���j�^�Ԃ̃K���}�����ӎ������܂܁A�S�̂Ƃ��ăK���}�̏グ�������s�����߂̊֐�
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

BOOL CreateDesktopShortcutForGamma(double gamma)
{
	TCHAR desktopPath[MAX_PATH];
	if( !::GetDesktopPath(desktopPath, MAX_PATH) )
		return FALSE;

	//���s���̃v���Z�X�̃t���p�X�����擾����
	TCHAR szPath[MAX_PATH];
	if( GetModuleFileName(NULL, (LPWSTR)szPath, sizeof(szPath)) == 0 ){
		return FALSE;
	}
	
	TCHAR linkPath[MAX_PATH];
	TCHAR gammaOption[MAX_PATH];
	::_stprintf_s(linkPath, L"%s\\�K���}%.2f.lnk", desktopPath, gamma);
	::_stprintf_s(gammaOption, L"-gamma %.1f", gamma);

	::CoInitialize(NULL);
	BOOL result = CreateShortcut(szPath, gammaOption, desktopPath, 0, (LPCSTR)linkPath);
	::CoUninitialize();

	return result;
}

BOOL CALLBACK MonitorEnumProc(
  HMONITOR hMonitor,  // �f�B�X�v���C���j�^�̃n���h��
  HDC hdcMonitor,     // ���j�^�ɓK�����f�o�C�X�R���e�L�X�g�̃n���h��
  LPRECT lprcMonitor, // ���j�^��̌���������\�������`�̈�ւ̃|�C���^
  LPARAM dwData       // EnumDisplayMonitors ����n���ꂽ�f�[�^
){
#define MONITOR_WORDS MAX_PATH

	// ���j�^�̏����擾����
    MONITORINFOEX stMonInfoEx;
    stMonInfoEx.cbSize = sizeof(stMonInfoEx);
    ::GetMonitorInfo(hMonitor, &stMonInfoEx);

	LPTSTR deviceName = (LPTSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, MONITOR_WORDS * sizeof(TCHAR));
	HDC hDC = ::CreateDC(L"DISPLAY", stMonInfoEx.szDevice, NULL, NULL);
	::wsprintf(deviceName, L"%s", stMonInfoEx.szDevice);

	// ���j�^���̂�ݒ�
	LPTSTR monitorName = (LPTSTR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, MONITOR_WORDS * sizeof(TCHAR));
	::wsprintf(monitorName, L"���j�^%d", g_deviceContextCounter + 1);

	MonitorInfo *monitor = &monitorInfoList[g_deviceContextCounter];
	monitor->hDC = hDC;
	monitor->deviceName = deviceName; // HDC�Ɏg����f�o�C�X��
	monitor->monitorName = monitorName; // �l�Ԍ����f�o�C�X��
	monitor->r = monitor->g = monitor->b = monitor->level = GAMMA_DEFAULT_VALUE;
	g_deviceContextCounter++;

	return TRUE;
}

// ���j�^�̐��Ɩ��O���ĔF������
void RecognizeMonitors(void)
{
	// ���łɊm�ۂ���Ă郁�������������
	for(int i=0; i<g_deviceContextCounter; i++){
		::DeleteDC(monitorInfoList[i].hDC);
		::GlobalFree(monitorInfoList[i].deviceName);
		::GlobalFree(monitorInfoList[i].monitorName);
	}
	g_deviceContextCounter = 0;

	EnumDisplayMonitors(::GetDC(NULL), NULL, MonitorEnumProc, 0);
}

void LoadConfig(void)
{
	LPTSTR lpCurrentDirectory = (LPTSTR)::GlobalAlloc(GMEM_FIXED, MAX_PATH * sizeof(TCHAR));

	if( ::GetExecuteDirectory(lpCurrentDirectory, MAX_PATH) ) {
		LPTSTR lpConfigPath = (LPTSTR)::GlobalAlloc(GMEM_FIXED, MAX_PATH * sizeof(TCHAR));
		::wsprintf(lpConfigPath, L"%s%s", lpCurrentDirectory, L"config.ini");
		
		::g_lightUpKey = ::GetPrivateProfileInt(L"KeyBind", L"lightUpKey", VK_PRIOR, lpConfigPath);
		::g_lightDownKey = ::GetPrivateProfileInt(L"KeyBind", L"lightDownKey", VK_NEXT, lpConfigPath);
		::g_lightResetKey = ::GetPrivateProfileInt(L"KeyBind", L"lightResetKey", VK_HOME, lpConfigPath);
		::g_lightOptKey = ::GetPrivateProfileInt(L"KeyBind", L"lightOptKey", VK_CONTROL, lpConfigPath);

		::g_gamma = ::GetPrivateProfileDouble(L"Gamma", L"gamma", 1.0, lpConfigPath);
		::g_gammaR = ::GetPrivateProfileDouble(L"Gamma", L"gammaR", 1.0, lpConfigPath);
		::g_gammaG = ::GetPrivateProfileDouble(L"Gamma", L"gammaG", 1.0, lpConfigPath);
		::g_gammaB = ::GetPrivateProfileDouble(L"Gamma", L"gammaB", 1.0, lpConfigPath);

		::GlobalFree(lpConfigPath);
	}else{
		::ShowLastError();
	}

	::GlobalFree(lpCurrentDirectory);
}

void SaveConfig(void)
{
	LPTSTR lpCurrentDirectory = (LPTSTR)::GlobalAlloc(GMEM_FIXED, MAX_PATH * sizeof(TCHAR));

	if( ::GetExecuteDirectory(lpCurrentDirectory, MAX_PATH) ){
		LPTSTR lpConfigPath = (LPTSTR)::GlobalAlloc(GMEM_FIXED, MAX_PATH * sizeof(TCHAR));
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

		::WritePrivateProfileDouble(L"Gamma", L"gamma", ::g_gamma, lpConfigPath);
		::WritePrivateProfileDouble(L"Gamma", L"gammaR", ::g_gammaR, lpConfigPath);
		::WritePrivateProfileDouble(L"Gamma", L"gammaG", ::g_gammaG, lpConfigPath);
		::WritePrivateProfileDouble(L"Gamma", L"gammaB", ::g_gammaB, lpConfigPath);

		::GlobalFree(lpConfigPath);
	}else{
		::ShowLastError();
	}

	::GlobalFree(lpCurrentDirectory);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// ���d�N���h�~�̑O�Ɉ����ɖ��邳��񂪎w�肳��Ă�����
	// ����𗘗p���Ă����ɖ��邳��ύX�ł���悤�ɂ���
	// ����͑��d�N�����Ă��悢���̂Ƃ���
	int    i;
	int    nArgs;
	LPTSTR *lplpszArgs;

	lplpszArgs = CommandLineToArgvW(GetCommandLine(), &nArgs);

	if( nArgs == 1 ){
		;
	}else{
		// �Ȃ񂩈�������A�����ƒl�̃y�A�̐��������Ƃ��Ă邩�`�F�b�N
		if( (nArgs - 1) % 2 == 0 ){
			// �p�����[�^�[�̐��������Ă�̂Œ��g�������Ă邩�Ƃ��Ӗ��������Ă邩�̃`�F�b�N
			for(i=1; i<nArgs; i+=2){
				LPTSTR lpOpt = lplpszArgs[i];
				LPTSTR lpStr = lplpszArgs[i+1];
				LPTSTR lpEnd = NULL;

				// gamma�w�肪�������炻�̃K���}�ɐݒ肷��
				if(wcscmp(lpOpt, L"-gamma") == 0){
					// reset�Ń��Z�b�gzzzz
					if(wcscmp(lpStr, L"reset") == 0 || wcscmp(lpStr, L"default") == 0){
						g_gamma = 1.0;
						SetGammaCorrect(g_gamma);
					}else{
						g_gamma = ::wcstod(lpStr, &lpEnd);
						if(g_gamma == 0 && lpStr == lpEnd){
							ErrorMessageBox(L"Format Error");
							exit(-1);
						}
						SetGammaCorrect(g_gamma);
					}
				}
			}
		}else{
			exit(-1);
		}
	}

	LocalFree(lplpszArgs);

	// ���d�N���h�~
	hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
	if(GetLastError() == ERROR_ALREADY_EXISTS){
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
		return FALSE;
	}

	// config.ini��ǂݍ���Őݒ�𔽉f���܂�
	LoadConfig();

	MSG msg;
	HACCEL hAccelTable;

	// �O���[�o������������������Ă��܂��B
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_GANMACHANGER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// �A�v���P�[�V�����̏����������s���܂�:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GANMACHANGER));

	// ���C�� ���b�Z�[�W ���[�v:
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
//  �֐�: MyRegisterClass()
//
//  �ړI: �E�B���h�E �N���X��o�^���܂��B
//
//  �R�����g:
//
//    ���̊֐�����юg�����́A'RegisterClassEx' �֐����ǉ����ꂽ
//    Windows 95 ���O�� Win32 �V�X�e���ƌ݊�������ꍇ�ɂ̂ݕK�v�ł��B
//    �A�v���P�[�V�������A�֘A�t����ꂽ
//    �������`���̏������A�C�R�����擾�ł���悤�ɂ���ɂ́A
//    ���̊֐����Ăяo���Ă��������B
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
//   �֐�: InitInstance(HINSTANCE, int)
//
//   �ړI: �C���X�^���X �n���h����ۑ����āA���C�� �E�B���h�E���쐬���܂��B
//
//   �R�����g:
//
//        ���̊֐��ŁA�O���[�o���ϐ��ŃC���X�^���X �n���h����ۑ����A
//        ���C�� �v���O���� �E�B���h�E���쐬����ѕ\�����܂��B
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // �O���[�o���ϐ��ɃC���X�^���X�������i�[���܂��B

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

	// �\���̃����o�̐ݒ�
	nid.cbSize           = sizeof( NOTIFYICONDATA );
	nid.uFlags           = (NIF_ICON|NIF_MESSAGE|NIF_TIP);
	nid.hWnd             = hWnd;           // �E�C���h�E�E�n���h��
	nid.hIcon            = LoadIcon(hInst, MAKEINTRESOURCE(IDI_GANMACHANGER));          // �A�C�R���E�n���h��
	nid.uID              = ID_TRAYICON;    // �A�C�R�����ʎq�̒萔
	nid.uCallbackMessage = WM_TASKTRAY;    // �ʒm���b�Z�[�W�̒萔

	lstrcpy( nid.szTip, TASKTRAY_TOOLTIP_TEXT );  // �`�b�v�w���v�̕�����

	// �A�C�R���̕ύX
	if( !Shell_NotifyIcon( NIM_ADD, (PNOTIFYICONDATAW)&nid ) )
		::ShowLastError();

	//ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

BOOL nextGamma(void)
{
	g_gamma += GAMMA_INCREMENT_VALUE;
	if(g_gamma > GAMMA_MAX_VALUE)
		g_gamma = GAMMA_MAX_VALUE;
	return ::SetGammaCorrectRGBCareMonitor(g_gamma, g_gamma, g_gamma);
}

BOOL prevGamma(void)
{
	g_gamma -= GAMMA_DECREMENT_VALUE;
	if(g_gamma < GAMMA_MIN_VALUE) g_gamma = GAMMA_MIN_VALUE;
	return ::SetGammaCorrectRGBCareMonitor(g_gamma, g_gamma, g_gamma);
}

BOOL resetGamma(void)
{
	g_gamma = GAMMA_DEFAULT_VALUE;
	return ::SetGammaCorrectRGBCareMonitor(g_gamma, g_gamma, g_gamma);
}

// ���j�^�֌W�Ȃ��A���ׂẴf�X�N�g�b�v���ʂ̃K���}�ύX�v���V�[�W��
BOOL CALLBACK DlgGammaProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	BOOL result = false;
	double level = 0;
	int delta = 0;
	
	switch( msg ){
	case WM_HSCROLL:
		if( (HWND)lp == GetDlgItem(hDlg, IDC_SLIDER_GAMMA) ){
			int pos = SendMessage((HWND)lp, TBM_GETPOS, 0, 0);
			g_gamma = pos * 5.00 / 100;
			g_gammaR = g_gammaG = g_gammaB = g_gamma;
			
			// �e���j�^�̃K���}���ӎ������܂ܑS�̂̃K���}�̏グ����������
			::SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);
			
			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
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
	case WM_INITDIALOG:  // �_�C�A���O�{�b�N�X���쐬���ꂽ�Ƃ�
		::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
		::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
		::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
		::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);

		::SendDlgItemMessageA(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(g_gamma / (5.00 / 100)));
		::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (5.00 / 100)));
		::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (5.00 / 100)));
		::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (5.00 / 100)));

		// ��ɍőO�ʂɕ\��
		::SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
		return TRUE;

	case WM_COMMAND:     // �_�C�A���O�{�b�N�X���̉������I�����ꂽ�Ƃ�
		switch( LOWORD( wp ) ){
		case IDOK:       // �uOK�v�{�^�����I�����ꂽ
			SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);
			break;
		case IDCANCEL:   // �u�L�����Z���v�{�^�����I�����ꂽ
			// �_�C�A���O�{�b�N�X������
			EndDialog(g_hGammaDlg, LOWORD(wp));
			g_hGammaDlg = NULL;
			break;
		case IDDEFAULT:
			g_gamma = g_gammaR = g_gammaG = g_gammaB = 1.0;
			SetGammaCorrectRGBCareMonitor(g_gammaR, g_gammaG, g_gammaB);

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
			::SetDlgItemDouble(hDlg, IDC_EDIT_RGAMMA, g_gammaR);
			::SetDlgItemDouble(hDlg, IDC_EDIT_GGAMMA, g_gammaG);
			::SetDlgItemDouble(hDlg, IDC_EDIT_BGAMMA, g_gammaB);
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GAMMA, TBM_SETPOS, TRUE, (int)(g_gamma / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_RGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaR / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_GGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaG / (5.00 / 100)));
			::SendDlgItemMessageA(hDlg, IDC_SLIDER_BGAMMA, TBM_SETPOS, TRUE, (int)(g_gammaB / (5.00 / 100)));

			::SetDlgItemDouble(hDlg, IDC_BRIGHTNESS_LEVEL, g_gamma);
			break;
		case IDSHORTCUT: // �V���[�g�J�b�g�쐬
			CreateDesktopShortcutForGamma(g_gamma);
			break;
		}
		return TRUE;

	case WM_CLOSE:		// �_�C�A���O�{�b�N�X��������Ƃ�
		// �_�C�A���O�{�b�N�X������
		EndDialog(g_hGammaDlg, LOWORD(wp));
		g_hGammaDlg = NULL;
		return TRUE;
	}

	return FALSE;  // DefWindowProc()�ł͂Ȃ��AFALSE��Ԃ����ƁI
}



// ���j�^���Ƃ̃K���}�ύX�v���V�[�W��
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
	case WM_INITDIALOG:  // �_�C�A���O�{�b�N�X���쐬���ꂽ�Ƃ�
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

			// �E�C���h�E�̃^�C�g�����ǂ̃��j�^���킩��悤�Ȃ��̂�
			TCHAR buf[256];
			::wsprintf((LPTSTR)buf, (LPCTSTR)_T("%s�̃K���}����"), monitor->monitorName);
			::SetWindowText(hDlg, buf);

			// ��ɍőO�ʂɕ\��
			::SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
		}
		return TRUE;

	case WM_COMMAND:     // �_�C�A���O�{�b�N�X���̉������I�����ꂽ�Ƃ�
		switch( LOWORD( wp ) ){
		case IDOK:       // �K�p�{�^�����I�����ꂽ
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
		case IDCANCEL:   // �u�L�����Z���v�{�^�����I�����ꂽ
			// �_�C�A���O�{�b�N�X������
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
		case IDSHORTCUT: // �V���[�g�J�b�g�쐬
			CreateDesktopShortcutForGamma(monitor->level);
			break;
		}
		return TRUE;

	case WM_CLOSE:		// �_�C�A���O�{�b�N�X��������Ƃ�
		// �_�C�A���O�{�b�N�X������
		EndDialog(g_hMonitorGammaDlg, LOWORD(wp));
		g_hMonitorGammaDlg = NULL;
		return TRUE;
	}

	return FALSE;  // DefWindowProc()�ł͂Ȃ��AFALSE��Ԃ����ƁI
}

// �L�[�t�b�N�̃g�O��
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

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wp, LPARAM lp)
{
	//nCode��0�����̂Ƃ��́ACallNextHookEx���Ԃ����l��Ԃ�
	if (nCode < 0)  return CallNextHookEx(g_hKeyConfigHook,nCode,wp,lp);

	if (nCode==HC_ACTION) {
		//�L�[�̑J�ڏ�Ԃ̃r�b�g���`�F�b�N����
		//WM_KEYDOWN��WM_KEYUP��Dialog�ɑ��M����
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

BOOL CALLBACK DlgKeyConfigProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	LPTSTR lpstr, lpBuffer = NULL;
	static UINT targetID = -1;
	BYTE keyTbl[256];
	static int prevKey, nextKey, resetKey, optKey = 0;
	LPTSTR s_nextKey, s_prevKey, s_resetKey, s_optKey, s_buffer = NULL;

	switch( msg ){
	case WM_INITDIALOG:  // �_�C�A���O�{�b�N�X���쐬���ꂽ�Ƃ�
		// ��ɍőO�ʂɕ\��
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
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTUP, L"�ݒ�");
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTDOWN){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTDOWN, L"�ݒ�");
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}else if(targetID == IDC_EDIT_KEYBIND_LIGHTRESET){
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTRESET, L"�ݒ�");
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}
		break;

	case WM_COMMAND:     // �_�C�A���O�{�b�N�X���̉������I�����ꂽ�Ƃ�
		switch( LOWORD( wp ) ){
		case IDOK:       // �K�p�{�^�����I�����ꂽ
			::StopHook();
			::StartKeyHook(prevKey, nextKey, resetKey, optKey);

			g_lightUpKey = nextKey;
			g_lightDownKey = prevKey;
			g_lightResetKey = resetKey;
			g_lightOptKey = optKey;

			EndDialog(g_hKeyConfigDlg, LOWORD(wp));
			g_hKeyConfigDlg = NULL;
			break;
		case IDCANCEL:   // �u�L�����Z���v�{�^�����I�����ꂽ
			// �_�C�A���O�{�b�N�X������
			EndDialog(g_hKeyConfigDlg, LOWORD(wp));
			g_hKeyConfigDlg = NULL;
			break;
		case ID_KEYBIND_LIGHTUP:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTUP;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTUP, L"����");
			break;
		case ID_KEYBIND_LIGHTDOWN:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTDOWN;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTDOWN, L"����");
			break;
		case ID_KEYBIND_LIGHTRESET:
			if(::g_hKeyConfigHook == NULL)
				g_hKeyConfigHook = ::SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetWindowThreadProcessId(hDlg, NULL));
			targetID = IDC_EDIT_KEYBIND_LIGHTRESET;
			::SetDlgItemText(hDlg, ID_KEYBIND_LIGHTRESET, L"����");
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

	case WM_CLOSE:		// �_�C�A���O�{�b�N�X��������Ƃ�
		// �_�C�A���O�{�b�N�X������
		// �t�b�N����Ă��炻�������
		if(::g_hKeyConfigHook){
			::UnhookWindowsHookEx(g_hKeyConfigHook);
			g_hKeyConfigHook = NULL;
		}

		EndDialog(g_hKeyConfigDlg, LOWORD(wp));
		g_hKeyConfigDlg = NULL;
		return TRUE;
	}

	return FALSE;  // DefWindowProc()�ł͂Ȃ��AFALSE��Ԃ����ƁI
}

//
//  �֐�: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  �ړI:  ���C�� �E�B���h�E�̃��b�Z�[�W���������܂��B
//
//  WM_COMMAND	- �A�v���P�[�V���� ���j���[�̏���
//  WM_PAINT	- ���C�� �E�B���h�E�̕`��
//  WM_DESTROY	- ���~���b�Z�[�W��\�����Ė߂�
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
		// ���j�^�̎�ނ��ĔF������
		RecognizeMonitors();

		SetWindowHandle(hWnd);
		StartKeyHook(g_lightUpKey, g_lightDownKey, g_lightResetKey, g_lightOptKey);

		// taskbar event
		wm_taskbarCreated = RegisterWindowMessage(TEXT("TaskbarCreated"));
		break;
	case WM_TASKTRAY:
		switch(lParam){
		case WM_LBUTTONDOWN:
			// �V���O���N���b�N�Œ��f <-> �ĊJ
			stopOrResume(hWnd);
			break;
		case WM_LBUTTONDBLCLK:
			break;
		case WM_RBUTTONDOWN:
			SetForegroundWindow(hWnd);

			// �J�[�\���̌��݈ʒu���擾
			POINT point;
			::GetCursorPos(&point);

			hMenu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU));
			hSubMenu = GetSubMenu(hMenu, 0);
	
			// ���݂̑I����Ԃ��^�X�N�o�[�ɕ\��
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
			mii.dwTypeData = _T("���j�^�ʒ���");
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
		// �I�����ꂽ���j���[�̉��:
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
			g_gammaR = g_gammaG = g_gammaB = 1.0;
			g_gamma = 1.0;
			for(int i=0; i<g_deviceContextCounter; i++){
				monitorInfoList[i].r = 1.0;
				monitorInfoList[i].g = 1.0;
				monitorInfoList[i].b = 1.0;
				monitorInfoList[i].level = 1.0;
				::SetGammaCorrectMonitorRGB(monitorInfoList[i].hDC, 1.0, 1.0, 1.0);
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

				// ���j�^��ID���킩�����̂ł�����ΏۂƂ����_�C�A���O�\��
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
		// TODO: �`��R�[�h�������ɒǉ����Ă�������...
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
		tnid.hWnd = hWnd;				// ���C���E�B���h�E�n���h��
		tnid.uID = ID_TRAYICON;			// �R���g���[��ID
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

// �o�[�W�������{�b�N�X�̃��b�Z�[�W �n���h���[�ł��B
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
