#include "stdafx.h"
#include "GammaController.h"

//==================================
//  Utility Functions
//==================================
// モニタ個別にガンマを設定
//#define MIN_GAMMA 0.23
//#define MAX_GAMMA 4.40
BOOL SetMonitorGamma(HDC hdc, double gammaR, double gammaG, double gammaB, BOOL darkCorrect = FALSE)
{
	gammaR = 1.0 / gammaR;
	gammaG = 1.0 / gammaG;
	gammaB = 1.0 / gammaB;

  // WORD = char x 2 = 2byte = 16bit = 2の16乗 = 65536(0 - 65536)
	WORD ramp[256*3];
	for(int i=0; i<256; i++){
		double valueR = pow((i + 1) / 256.0, gammaR) * 65536;
		double valueG = pow((i + 1) / 256.0, gammaG) * 65536;
		double valueB = pow((i + 1) / 256.0, gammaB) * 65536;

    if(darkCorrect){
      if(i <= 0){ // 完全な黒 = 最小の階調(i = 0) = のときはオリジナルのまま(= gamma 1.0)で描画する
        double original_gamma = 1.0;
        valueR = valueG = valueB = pow((i + 1) / 256.0, original_gamma) * 65536;
      }
    }

    if(valueR < 0) valueR = 0; if(valueR > 65535) valueR = 65535;
		if(valueG < 0) valueG = 0; if(valueG > 65535) valueG = 65535;
		if(valueB < 0) valueB = 0; if(valueB > 65535) valueB = 65535;
		
		ramp[0+i] = (WORD)valueR;
		ramp[256+i] = (WORD)valueG;
		ramp[512+i] = (WORD)valueB;
	}
	return !::SetDeviceGammaRamp(hdc, &ramp);
}

BOOL SetMonitorGamma(HDC hdc, double gamma, BOOL darkCorrect = FALSE)
{
  return SetMonitorGamma(hdc, gamma, gamma, gamma, darkCorrect);
}

BOOL SetMonitorGamma(double gammaR, double gammaG, double gammaB, BOOL darkCorrect = FALSE)
{
  return SetMonitorGamma(::GetDC(NULL), gammaR, gammaG, gammaB, darkCorrect);
}

BOOL SetMonitorGamma(double gamma, BOOL darkCorrect = FALSE)
{
  return SetMonitorGamma(::GetDC(NULL), gamma, gamma, gamma, darkCorrect);
}

//===========================================
//  Implement GammaController Class
//===========================================
GammaController::GammaController()
{
	m_gamma = DEFAULT_GAMMA;
	m_monitorIndex = 0;
  m_darkCorrect = false;
}

GammaController::~GammaController()
{
	this->monitorReset();
}

// ========= raw level interfaces
bool GammaController::setGamma(double r, double g, double b)
{
  return (::SetMonitorGamma(r, g, b, this->m_darkCorrect) == TRUE);
}

bool GammaController::setMonitorGamma(HDC hdc, double r, double g, double b)
{
	return (::SetMonitorGamma(hdc, r, g, b, this->m_darkCorrect) == TRUE);
}

bool GammaController::setGamma(double gamma)
{
	m_gamma = gamma;
	return this->setGamma(gamma, gamma, gamma);
}

bool GammaController::setMonitorGamma(HDC hdc, double gamma)
{
	return (this->setMonitorGamma(hdc, gamma, gamma, gamma) == TRUE);
}
// =========== raw level interfaces end.

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

void GammaController::redraw()
{
  this->setMonitorGammaDifference(this->m_gamma, this->m_gamma, this->m_gamma);
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
  m->level = m->r = m->g = m->b = correctGamma(m->r + GAMMA_INCREMENT_VALUE); // R based
  return this->setMonitorGamma(m->hDC, m->r, m->g, m->b);
}

bool GammaController::decrementAt(POINT *pt)
{  
  MonitorInfo *m = this->monitorGetAt(pt);
  m->level = m->r = m->g = m->b = correctGamma(m->r - GAMMA_DECREMENT_VALUE);
  return this->setMonitorGamma(m->hDC, m->r, m->g, m->b);
}

bool GammaController::resetMonitorAt(POINT *pt)
{
  MonitorInfo *m = this->monitorGetAt(pt);
  m->level = m->r = m->g = m->b = correctGamma(DEFAULT_GAMMA);
  return this->setMonitorGamma(m->hDC, m->r, m->g, m->b);
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
