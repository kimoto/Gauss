#pragma once

#include <Windows.h>
#include "Util.h"

#define GAMMA_INCREMENT_VALUE 0.1
#define GAMMA_DECREMENT_VALUE 0.1
#define GAMMA_DEFAULT_VALUE 1.0
#define GAMMA_MIN_VALUE 0.0
#define GAMMA_MAX_VALUE 5.0

#define MAX_GAMMA 5.0
#define MIN_GAMMA 0.0

#define DEFAULT_GAMMA 1.0

#define MAX_MONITOR_NUMBER 32

typedef struct {
	double r;		// ��
	double g;		// ��
	double b;		// ��
	double level;	// �S��

	HDC hDC;		// ���j�^�̃f�o�C�X�R���e�L�X�g
	UINT monitorID;	// ���j�^�̓����Ǘ�ID(window���b�Z�[�W�Ƃ̊֘A�t���Ɏg�p)

	LPWSTR monitorName;	// ���j�^�̖��O
	LPWSTR deviceName;	// ���j�^�̃f�o�C�X�p�X
} MonitorInfo;

class GammaController
{
private:
	double m_gamma;
	int m_monitorIndex;
	MonitorInfo m_monitors[MAX_MONITOR_NUMBER];

public:
	// constructer & destructer
	GammaController();
	~GammaController();

	// ========= raw level API
	bool setGamma(double r, double g, double b);
	bool setGamma(double r);
	bool setMonitorGamma(HDC hdc, double r, double g, double b);
	bool setMonitorGamma(HDC hdc, double gamma);

	// ========= monitor�Ԃ̃K���}�����ێ������܂ܑ���ł���API
	bool reset();
	bool increment();
	bool decrement();
	bool resetMonitorDifference();
	bool setMonitorGammaDifference(double r, double g, double b);
	bool setMonitorGammaDifference(double gamma);

	// ========= monitor�֌W��API
	bool monitorAdd(MonitorInfo *monitor);
	bool monitorAdd(HDC, LPTSTR, LPTSTR);
	MonitorInfo *monitorGet(int index);
	bool monitorReset();
	int monitorGetCount();
	bool hasMultiMonitor(); // ���݂̊��͕����̃��j�^�������Ă���?

	// ========== monitor�̃C���f�b�N�X�w�肵�đ��삷��nAPI(����)
	bool resetMonitor(int index);
	bool setMonitorGammaIndex(int index, double r, double g, double b, double level);
	bool setMonitorGammaIndex(int index, double gamma);
};