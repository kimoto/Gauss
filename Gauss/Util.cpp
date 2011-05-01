#include "stdafx.h"
#include "Util.h"

#define TRACE_BUFFER_SIZE 1024
#define FORMAT_BUFFER_SIZE 1024
#define MCI_LASTERROR_BUFSIZE 1024

void trace(LPCTSTR format, ...)
{
	va_list arg;
	va_start(arg, format);
	
	TCHAR buffer[TRACE_BUFFER_SIZE];
	::_vsnwprintf_s(buffer, TRACE_BUFFER_SIZE, TRACE_BUFFER_SIZE, format, arg);
	::OutputDebugString(buffer);	
	va_end(arg);
}

void DrawFormatText(HDC hdc, LPRECT rect, UINT type, LPCTSTR format, ...)
{
	va_list arg;
	va_start(arg, format);
	
	TCHAR buffer[TRACE_BUFFER_SIZE];
	::_vsnwprintf_s(buffer, TRACE_BUFFER_SIZE, TRACE_BUFFER_SIZE, format, arg);
	::DrawText(hdc, buffer, lstrlen(buffer), rect, type);	
	va_end(arg);
}

void TextFormatOut(HDC hdc, int x, int y, LPCTSTR format, ...)
{
	va_list arg;
	va_start(arg, format);
	
	TCHAR buffer[FORMAT_BUFFER_SIZE];
	::_vsnwprintf_s(buffer, FORMAT_BUFFER_SIZE, FORMAT_BUFFER_SIZE, format, arg);
	::TextOut(hdc, x, y, buffer, lstrlen(buffer));
	va_end(arg);
}

void BorderedRect(HDC hdc, int x, int y, int width, int height, COLORREF color)
{
	FillRectBrush(hdc, x, y, width, height, color);
	drawRect(hdc, x, y, width, height);
}

void FillRectBrush(HDC hdc, int x, int y, int width, int height, COLORREF color)
{
	RECT rect;
	rect.top = y;
	rect.left = x;
	rect.right = rect.left + width;
	rect.bottom = rect.top + height;

	HBRUSH brush = ::CreateSolidBrush(color);
	::FillRect(hdc, &rect, brush);
	::DeleteObject(brush);
}

void drawRect(HDC hdc, int x, int y, int width, int height)
{
	::MoveToEx(hdc, x, y, NULL);
	::LineTo(hdc, x + width - 1, y);
	::LineTo(hdc, x + width - 1, y + height);
	::LineTo(hdc, x, y + height);
	::LineTo(hdc, x, y);
}

void drawRectColor(HDC hdc, int x, int y, int width, int height, COLORREF color, int bold_width)
{
	HPEN pen = ::CreatePen(PS_SOLID, bold_width, color);
	HBRUSH oldPen = (HBRUSH)::SelectObject(hdc, pen);

	drawRect(hdc, x, y, width, height);

	::SelectObject(hdc, oldPen);
	::DeleteObject(pen);
}

void mciShowLastError(MMRESULT result)
{
	LPTSTR lpstr = (LPWSTR)::GlobalAlloc(GMEM_FIXED, MCI_LASTERROR_BUFSIZE);
	mciGetErrorString(result, lpstr, MCI_LASTERROR_BUFSIZE);
	::MessageBox(NULL, lpstr, L"ERROR", MB_OK);
}

void mciAssert(MMRESULT result)
{
	// �G���[�������Ƃ��͂��̓��e��\�����ċ����I�����܂�
	if(result != MMSYSERR_NOERROR){
		::mciShowLastError(result);
		exit(1);
	}
}

BOOL ReadWaveFile(LPTSTR lpszFileName, LPWAVEFORMATEX lpwf, LPBYTE *lplpData, LPDWORD lpdwDataSize)
{
	HMMIO    hmmio;
	MMCKINFO mmckRiff;
	MMCKINFO mmckFmt;
	MMCKINFO mmckData;
	LPBYTE   lpData;

	hmmio = mmioOpen(lpszFileName, NULL, MMIO_READ);
	if (hmmio == NULL) {
		MessageBox(NULL, TEXT("�t�@�C���̃I�[�v���Ɏ��s���܂����B"), NULL, MB_ICONWARNING);
		return FALSE;
	}
	
	mmckRiff.fccType = mmioStringToFOURCC(TEXT("WAVE"), 0);
	if (mmioDescend(hmmio, &mmckRiff, NULL, MMIO_FINDRIFF) != MMSYSERR_NOERROR) {
		MessageBox(NULL, TEXT("WAVE�t�@�C���ł͂���܂���B"), NULL, MB_ICONWARNING);
		mmioClose(hmmio, 0);
		return FALSE;
	}

	mmckFmt.ckid = mmioStringToFOURCC(TEXT("fmt "), 0);
	if (mmioDescend(hmmio, &mmckFmt, NULL, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
		mmioClose(hmmio, 0);
		return FALSE;
	}
	mmioRead(hmmio, (HPSTR)lpwf, mmckFmt.cksize);
	mmioAscend(hmmio, &mmckFmt, 0);
	if (lpwf->wFormatTag != WAVE_FORMAT_PCM) {
		MessageBox(NULL, TEXT("PCM�f�[�^�ł͂���܂���B"), NULL, MB_ICONWARNING);
		mmioClose(hmmio, 0);
		return FALSE;
	}

	mmckData.ckid = mmioStringToFOURCC(TEXT("data"), 0);
	if (mmioDescend(hmmio, &mmckData, NULL, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
		mmioClose(hmmio, 0);
		return FALSE;
	}
	lpData = (LPBYTE)HeapAlloc(GetProcessHeap(), 0, mmckData.cksize);
	mmioRead(hmmio, (HPSTR)lpData, mmckData.cksize);
	mmioAscend(hmmio, &mmckData, 0);

	mmioAscend(hmmio, &mmckRiff, 0);
	mmioClose(hmmio, 0);

	*lplpData = lpData;
	*lpdwDataSize = mmckData.cksize;

	return TRUE;
}

BOOL LoadBitmapFromBMPFile( LPTSTR szFileName, HBITMAP *phBitmap, HPALETTE *phPalette )
{
	BITMAP  bm;

	*phBitmap = NULL;
	*phPalette = NULL;

	// Use LoadImage() to get the image loaded into a DIBSection
	*phBitmap = (HBITMAP)LoadImage( NULL, szFileName, IMAGE_BITMAP, 0, 0,
				LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE );
	if( *phBitmap == NULL )
		return FALSE;

	// Get the color depth of the DIBSection
	GetObject(*phBitmap, sizeof(BITMAP), &bm );
	// If the DIBSection is 256 color or less, it has a color table
	if( ( bm.bmBitsPixel * bm.bmPlanes ) <= 8 )
	{
		HDC           hMemDC;
		HBITMAP       hOldBitmap;
		RGBQUAD       rgb[256];
		LPLOGPALETTE  pLogPal;
		WORD          i;

		// Create a memory DC and select the DIBSection into it
		hMemDC = CreateCompatibleDC( NULL );
		hOldBitmap = (HBITMAP)SelectObject( hMemDC, *phBitmap );
		// Get the DIBSection's color table
		GetDIBColorTable( hMemDC, 0, 256, rgb );
		// Create a palette from the color tabl
		pLogPal = (LOGPALETTE *)malloc( sizeof(LOGPALETTE) + (256*sizeof(PALETTEENTRY)) );
		pLogPal->palVersion = 0x300;
		pLogPal->palNumEntries = 256;
		for(i=0;i<256;i++)
		{
			pLogPal->palPalEntry[i].peRed = rgb[i].rgbRed;
			pLogPal->palPalEntry[i].peGreen = rgb[i].rgbGreen;
			pLogPal->palPalEntry[i].peBlue = rgb[i].rgbBlue;
			pLogPal->palPalEntry[i].peFlags = 0;
		}
		*phPalette = CreatePalette( pLogPal );
		// Clean up
		free( pLogPal );
		SelectObject( hMemDC, hOldBitmap );
		DeleteDC( hMemDC );
	}
	else   // It has no color table, so use a halftone palette
	{
		HDC    hRefDC;

		hRefDC = GetDC( NULL );
		*phPalette = CreateHalftonePalette( hRefDC );
		ReleaseDC( NULL, hRefDC );
	}
	return TRUE;
}

BOOL LoadBitmapToDC(LPTSTR szFileName, int x, int y, HDC hdc)
{
	HBITMAP hBitmap2, hOldBitmap2;
	HPALETTE hPalette2, hOldPalette2;
	HDC hMemDC;
	BITMAP bm;

	if( LoadBitmapFromBMPFile(szFileName, &hBitmap2, &hPalette2) )
	{
		::GetObject(hBitmap2, sizeof(BITMAP), &bm);
		hMemDC = ::CreateCompatibleDC(hdc);
		hOldBitmap2 = (HBITMAP)::SelectObject(hMemDC, hBitmap2);
		hOldPalette2 = ::SelectPalette(hdc, hPalette2, FALSE);
		::RealizePalette(hdc);
			
		::BitBlt(hdc, x, y, bm.bmWidth, bm.bmHeight,
			hMemDC, 0, 0, SRCAND);

		::SelectObject(hMemDC, hOldBitmap2);
		::DeleteObject(hBitmap2);

		::SelectPalette(hdc, hOldPalette2, FALSE);
		::DeleteObject(hPalette2);
		return TRUE;
	}else{
		::OutputDebugString(L"error loading bitmap\n");
		return FALSE;
	}
}

// .wav information
WAVEFORMATEX wfe;
WAVEHDR whdr[2]; // double buffering
HWAVEOUT hWaveOut;
LPBYTE lpWaveData;
DWORD dwDataSize;
int sound_ptr = 0;

void CALLBACK musicCallback(
	HWAVEOUT hwo , UINT uMsg,         
	DWORD dwInstance,  
	DWORD dwParam1, DWORD dwParam2     
){
	if(uMsg == MM_WOM_OPEN)
		trace(L"open\n");
	if(uMsg == MM_WOM_CLOSE)
		trace(L"close\n");
	if(uMsg == MM_WOM_DONE){
		trace(L"done\n");

		if(sound_ptr >= 2){ // 2��ڂ̃o�b�t�@���I�������A�ŏ��Ƀo�b�t�@���Q�Ƃ���悤��(double buffering)
			sound_ptr = 0;
		}
		::mciAssert( ::waveOutWrite(hWaveOut, &whdr[sound_ptr++], sizeof(WAVEHDR)) );
	}
}

void mciPlayBGM(LPTSTR szFileName, double volume_scale)
{
	if(!ReadWaveFile(szFileName, &wfe, &lpWaveData, &dwDataSize)){
		MessageBox(NULL, TEXT("WAVE�f�o�C�X�̓ǂݍ��݂Ɏ��s���܂����B"), NULL, MB_ICONWARNING);
		return;
	}

	if(::waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfe,(DWORD_PTR)&musicCallback, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR){
		MessageBox(NULL, TEXT("WAVE�f�o�C�X�̃I�[�v���Ɏ��s���܂����B"), NULL, MB_ICONWARNING);
		return;
	}

	for(int i=0; i<2; i++){
		whdr[i].lpData = (LPSTR)lpWaveData;
		whdr[i].dwBufferLength = dwDataSize;
		whdr[i].dwFlags = 0;

		::waveOutPrepareHeader(hWaveOut, &whdr[i], sizeof(WAVEHDR));

		// �ŏ��͗����̃o�b�t�@���Đ�����
		::mciAssert( ::waveOutWrite(hWaveOut, &whdr[i], sizeof(WAVEHDR)) );
	}

	// ���ʂ̐ݒ�
	DWORD left = (DWORD)(0xFFFF * volume_scale); // 0xFFFF = max volume
	DWORD right = left;
	DWORD dwVolume = MAKELONG(left, right);
	::mciAssert(::waveOutSetVolume(hWaveOut, dwVolume));
}

void ShowLastError(void){
	LPVOID lpMessageBuffer;
  
	FormatMessage(
	FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	GetLastError(),
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // �f�t�H���g ���[�U�[���� 
	(LPTSTR) &lpMessageBuffer,
	0,
	NULL );

	MessageBox(NULL, (LPCWSTR)lpMessageBuffer, TEXT("Error"), MB_OK);
  
	//... �����񂪕\������܂��B
	// �V�X�e���ɂ���Ċm�ۂ��ꂽ�o�b�t�@���J�����܂��B
	LocalFree( lpMessageBuffer );
}


// �V���[�g�J�b�g�쐬
BOOL CreateShortcut ( LPCTSTR pszTargetPath /* �^�[�Q�b�g�p�X */,
    LPCTSTR pszArguments /* ���� */,
    LPCTSTR pszWorkPath /* ��ƃf�B���N�g�� */,
    int nCmdShow /* ShowWindow�̈��� */,
    LPCSTR pszShortcutPath /* �V���[�g�J�b�g�t�@�C��(*.lnk)�̃p�X */ )
{
    IShellLink *psl = NULL;
    IPersistFile *ppf = NULL;
    enum
    {
        MY_MAX_PATH = 65536
    };
    TCHAR wcLink[ MY_MAX_PATH ]=_T("");

    // IShellLink�C���^�[�t�F�[�X�̍쐬
    HRESULT result = CoCreateInstance( CLSID_ShellLink, NULL,CLSCTX_INPROC_SERVER, IID_IShellLink, ( void ** ) &psl);
	if(FAILED(result))
    {
		return result;
	}

    // �ݒ�
    psl->SetPath ( pszTargetPath );
    psl->SetArguments ( pszArguments );
    psl->SetWorkingDirectory ( pszWorkPath );
    psl->SetShowCmd ( nCmdShow );

    // IPersistFile�C���^�[�t�F�[�X�̍쐬
    if ( FAILED ( psl->QueryInterface ( IID_IPersistFile, ( void ** ) &ppf ) ) )
    {
        psl->Release ();
        return FALSE;
    }
    
    // lpszLink��WCHAR�^�ɕϊ�
    MultiByteToWideChar ( CP_ACP, 0, pszShortcutPath, -1, wcLink, MY_MAX_PATH );
    if ( FAILED ( ppf->Save ( wcLink, TRUE ) ) )
    {
        ppf->Release ();
        return FALSE;
    }

	result = ppf->Save((LPCOLESTR)pszShortcutPath,TRUE);
	
    // ���
    ppf->Release ();
    psl->Release ();

    return TRUE;
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

void ErrorMessageBox(LPTSTR message)
{
	::MessageBox(NULL, message, L"Error", MB_OK);
}

BOOL GetExecuteDirectory(LPTSTR buffer, DWORD size_in_words)
{
	TCHAR modulePath[MAX_PATH]; 
	TCHAR drive[MAX_PATH];
	TCHAR dir[MAX_PATH];

	DWORD result = ::GetModuleFileName(NULL, modulePath, MAX_PATH);
	if(result == 0){
		return FALSE;
	}

	if( ::_wsplitpath_s(modulePath, drive, MAX_PATH, dir, MAX_PATH, NULL, 0, NULL, 0) != 0 )
		return FALSE;

	if( ::_snwprintf_s(buffer, size_in_words, _TRUNCATE, L"%s%s", drive, dir) < 0 )
		return FALSE;

	return TRUE;
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

BOOL GetDesktopPath(LPTSTR buffer, DWORD size_in_words)
{
	/* SHGetPathFromIDList���o�b�t�@�̃T�C�Y�����؂��Ă���Ȃ��̂�
	����Ɂu�Œ�ł�MAX_PATH���������̃o�b�t�@���m�ۂ���Ă��邱�Ɓv���m�F���܂� */
	if(size_in_words < MAX_PATH){
		return FALSE;
	}

	LPITEMIDLIST lpidlist;
	SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOPDIRECTORY, &lpidlist );
	SHGetPathFromIDList( lpidlist, buffer );
	return TRUE;
}

// ���j�^�ʂɃK���}��ݒ�
BOOL SetMonitorGamma(HDC hdc, double gammaR, double gammaG, double gammaB)
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

BOOL SetMonitorGamma(HDC hdc, double gamma)
{
	return SetMonitorGamma(hdc, gamma, gamma, gamma);
}

// ���ׂẴ��j�^���ʂɃK���}��ݒ�
BOOL SetGamma(double gammaR, double gammaG, double gammaB)
{
	return SetMonitorGamma(::GetDC(NULL), gammaR, gammaG, gammaB);
}

BOOL SetGamma(double gamma)
{
	return SetGamma(gamma, gamma, gamma);
}
