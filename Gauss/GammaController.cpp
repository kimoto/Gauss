#include "stdafx.h"
#include "GammaController.h"

// モニタ個別にガンマを設定
//#define MIN_GAMMA 0.23
//#define MAX_GAMMA 4.40
BOOL SetMonitorGamma(HDC hdc, double gammaR, double gammaG, double gammaB)
{
	// 補正
	/*
	if(gammaR < MIN_GAMMA) gammaR = MIN_GAMMA;
	if(gammaG < MIN_GAMMA) gammaG = MIN_GAMMA;
	if(gammaB < MIN_GAMMA) gammaB = MIN_GAMMA;

	if(gammaR > MAX_GAMMA) gammaR = MAX_GAMMA;
	if(gammaG > MAX_GAMMA) gammaG = MAX_GAMMA;
	if(gammaB > MAX_GAMMA) gammaB = MAX_GAMMA;
	*/
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

// すべてのモニタ共通にガンマを設定
BOOL SetGamma(double gammaR, double gammaG, double gammaB)
{
	return SetMonitorGamma(::GetDC(NULL), gammaR, gammaG, gammaB);
}

BOOL SetGamma(double gamma)
{
	return SetGamma(gamma, gamma, gamma);
}

GammaController::GammaController()
{
	m_gamma = DEFAULT_GAMMA;
	m_monitorIndex = 0;
}

GammaController::~GammaController()
{
	this->monitorReset();
}

bool GammaController::setGamma(double r, double g, double b)
{
	return (::SetGamma(r, g, b) == TRUE);
}

bool GammaController::setGamma(double gamma)
{
	m_gamma = gamma;
	return this->setGamma(gamma, gamma, gamma);
}

bool GammaController::setMonitorGamma(HDC hdc, double r, double g, double b)
{
	return (::SetMonitorGamma(hdc, r, g, b) == TRUE);
}

bool GammaController::setMonitorGamma(HDC hdc, double gamma)
{
	return (this->setMonitorGamma(hdc, gamma, gamma, gamma) == TRUE);
}

bool GammaController::reset()
{
	this->m_gamma = DEFAULT_GAMMA;

	for(int i=0; i<this->monitorGetCount(); i++){
		this->resetMonitor(i);
	}
	return true;
}

bool GammaController::resetMonitor(int index)
{
	return this->setMonitorGammaIndex(index, DEFAULT_GAMMA);
}

bool GammaController::setMonitorGammaIndex(int index, double r, double g, double b, double level)
{
	MonitorInfo *p = this->monitorGet(index);
	p->r = r;
	p->g = g;
	p->b = b;
	p->level = level;
	return this->setMonitorGamma(p->hDC, p->r, p->g, p->b);
}

bool GammaController::setMonitorGammaIndex(int index, double gamma)
{
	return this->setMonitorGammaIndex(index, gamma, gamma, gamma, gamma);
}

bool GammaController::resetMonitorDifference()
{
	m_gamma = DEFAULT_GAMMA;
	return this->setMonitorGammaDifference(m_gamma, m_gamma, m_gamma);
}

bool GammaController::setMonitorGammaDifference(double gammaR, double gammaG, double gammaB)
{
	double r = gammaR - 1.0;
	double g = gammaG - 1.0;
	double b = gammaB - 1.0;

	for(int i=0; i<m_monitorIndex; i++){
		this->setMonitorGamma(
			m_monitors[i].hDC,
			m_monitors[i].r + r,
			m_monitors[i].g + g,
			m_monitors[i].b + b);
	}
	return TRUE;
}

bool GammaController::setMonitorGammaDifference(double gamma)
{
	this->m_gamma = gamma;
	return this->setMonitorGammaDifference(gamma, gamma, gamma);
}

bool GammaController::increment()
{
	m_gamma += GAMMA_INCREMENT_VALUE;
	if(m_gamma > GAMMA_MAX_VALUE)
		m_gamma = GAMMA_MAX_VALUE;
	return this->setMonitorGammaDifference(m_gamma, m_gamma, m_gamma);
}

bool GammaController::decrement()
{
	m_gamma -= GAMMA_DECREMENT_VALUE;
	if(m_gamma < GAMMA_MIN_VALUE) m_gamma = GAMMA_MIN_VALUE;
	return this->setMonitorGammaDifference(m_gamma, m_gamma, m_gamma);
}

// モニタの数と名前を再認識する
bool GammaController::monitorAdd(MonitorInfo *monitor)
{
	this->m_monitors[this->m_monitorIndex++] = *monitor; // 構造体ノコピー
	return true;
}

bool GammaController::monitorAdd(HDC hDC, LPTSTR deviceName, LPTSTR monitorName, RECT rectangle)
{
	MonitorInfo monitor;
	monitor.hDC = hDC;
	monitor.deviceName = deviceName; // HDCに使えるデバイス名
	monitor.monitorName = monitorName; // 人間向けデバイス名
	monitor.r = monitor.g = monitor.b = monitor.level = GAMMA_DEFAULT_VALUE;
  monitor.rectangle = rectangle;
	return this->monitorAdd(&monitor);
}

// 保存されてるモニタの情報をクリアします
bool GammaController::monitorReset()
{
	// すでに確保されてるメモリを解放する
	for(int i=0; i<this->m_monitorIndex; i++){
		::DeleteDC(this->m_monitors[i].hDC);
		::GlobalFree(this->m_monitors[i].deviceName);
		::GlobalFree(this->m_monitors[i].monitorName);
	}
	this->m_monitorIndex = 0;
	return true;
}

MonitorInfo *GammaController::monitorGet(int index)
{
	return &this->m_monitors[index];
}

MonitorInfo *GammaController::monitorGetAt(POINT *pt)
{
  int n = this->findMonitorAt(pt);
  if(n < 0){
    return NULL;
  }
  return this->monitorGet(n);
}

int GammaController::monitorGetCount()
{
	return this->m_monitorIndex;
}

bool GammaController::hasMultiMonitor()
{
	//return true;
	return (this->monitorGetCount() >= 2);
}

// return index
int GammaController::findMonitorAt(POINT *pt)
{
  for(int i=0; i<this->monitorGetCount(); i++){
    MonitorInfo *m = this->monitorGet(i);
    if(m->rectangle.left <= pt->x && pt->x <= m->rectangle.right &&
      m->rectangle.top <= pt->y && pt->y <= m->rectangle.bottom){
        return i;
    }
  }
  return -1;
}

bool GammaController::incrementAt(POINT *pt)
{
  MonitorInfo *m = this->monitorGetAt(pt);
  m->level = correctGamma(m->level + GAMMA_INCREMENT_VALUE);
  return this->setMonitorGamma(m->hDC, m->level);
}

bool GammaController::decrementAt(POINT *pt)
{  
  MonitorInfo *m = this->monitorGetAt(pt);
  m->level = correctGamma(m->level - GAMMA_DECREMENT_VALUE);
  return this->setMonitorGamma(m->hDC, m->level);
}

bool GammaController::resetMonitorAt(POINT *pt)
{
  MonitorInfo *m = this->monitorGetAt(pt);
  m->level = correctGamma(DEFAULT_GAMMA);
  return this->setMonitorGamma(m->hDC, m->level);
}

double GammaController::correctGamma(double gamma)
{
  if(gamma < GAMMA_MIN_VALUE)
    gamma = GAMMA_MIN_VALUE;
  if(GAMMA_MAX_VALUE < gamma)
    gamma = GAMMA_MAX_VALUE;
  return gamma;
}

bool GammaController::incrementAtCursorPos()
{
  POINT pt;
  return ::GetCursorPos(&pt) ? this->incrementAt(&pt) : false;
}

bool GammaController::decrementAtCursorPos()
{
  POINT pt;
  return ::GetCursorPos(&pt) ? this->decrementAt(&pt) : false;
}

bool GammaController::resetMonitorAtCursorPos()
{
  POINT pt;
  return ::GetCursorPos(&pt) ? this->resetMonitorAt(&pt) : false;
}
