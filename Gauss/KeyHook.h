#include <Windows.h>

__declspec(dllexport) BOOL StartHook(void);
__declspec(dllexport) BOOL SetWindowHandle(HWND hWnd);
__declspec(dllexport) BOOL StopHook(void);
__declspec(dllexport) BOOL StartKeyHook(int prevKey, int nextKey, int resetKey, int optKey);
__declspec(dllexport) BOOL RestartHook(void);
__declspec(dllexport) BOOL isHook(void);
