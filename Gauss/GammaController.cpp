#include "stdafx.h"
#include "GammaController.h"

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
	return (::SetGamma(gamma) == TRUE);
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

bool GammaController::monitorAdd(HDC hDC, LPTSTR deviceName, LPTSTR monitorName)
{
	MonitorInfo monitor;
	monitor.hDC = hDC;
	monitor.deviceName = deviceName; // HDCに使えるデバイス名
	monitor.monitorName = monitorName; // 人間向けデバイス名
	monitor.r = monitor.g = monitor.b = monitor.level = GAMMA_DEFAULT_VALUE;
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

int GammaController::monitorGetCount()
{
	return this->m_monitorIndex;
}

bool GammaController::hasMultiMonitor()
{
	//return true;
	return (this->monitorGetCount() >= 2);
}
