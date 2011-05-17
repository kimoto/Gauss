#include <Windows.h>

#define WM_USER_MESSAGE (WM_USER+0x1000)
#define KEY_NOT_SET -1

typedef struct{
	int ctrlKey;	// CTRL
	int shiftKey;	// SHIFT
	int altKey;		// ALT
	int key;		// a-z‚È‚Ç
	int message;	// message
}KEYINFO;

#ifndef DLLIMPORT
	#define DLLIMPORT __declspec(dllimport)
#endif

#ifndef DLLEXPORT
	#define DLLEXPORT __declspec(dllexport)
#endif

DLLEXPORT BOOL StartHook(void);
DLLEXPORT BOOL SetWindowHandle(HWND hWnd);
DLLEXPORT BOOL StopHook(void);
DLLEXPORT BOOL StartKeyHook(int prevKey, int nextKey, int resetKey, int optKey);
DLLEXPORT BOOL RestartHook(void);
DLLEXPORT BOOL isHook(void);
DLLEXPORT int getHookKeys();
DLLEXPORT BOOL RegistKeyCombination(int ctrlKey, int altKey, int shiftKey, int key, int message);
DLLEXPORT BOOL RegistKey(KEYINFO keyInfo, int message);
DLLEXPORT void ClearKeyInfo(KEYINFO *keyInfo);
