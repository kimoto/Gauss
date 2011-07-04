#pragma once

#include <Windows.h>
#include "Util.h"

#define GAMMA_INCREMENT_VALUE 0.1
#define GAMMA_DECREMENT_VALUE 0.1

#define GAMMA_DEFAULT_VALUE 1.0
//#define GAMMA_MIN_VALUE 0.0
#define GAMMA_MIN_VALUE 0.23
//#define GAMMA_MAX_VALUE 5.0
#define GAMMA_MAX_VALUE 4.40

//#define MAX_GAMMA 5.0
//#define MIN_GAMMA 0.0
#define MAX_GAMMA GAMMA_MAX_VALUE
#define MIN_GAMMA GAMMA_MIN_VALUE

#define DEFAULT_GAMMA 1.0

#define MAX_MONITOR_NUMBER 32

typedef struct {
	double r;		// 赤
	double g;		// 緑
	double b;		// 青
	double level;	// 全体

	HDC hDC;		// モニタのデバイスコンテキスト
	UINT monitorID;	// モニタの内部管理ID(windowメッセージとの関連付けに使用)

	LPWSTR monitorName;	// モニタの名前
	LPWSTR deviceName;	// モニタのデバイスパス

  RECT rectangle; // モニタの領域
} MonitorInfo;

class GammaController
{
private:
	double m_gamma;
	int m_monitorIndex;
	MonitorInfo m_monitors[MAX_MONITOR_NUMBER];

public:
  bool m_darkCorrect;
  
  // constructer & destructer
	GammaController();
	~GammaController();

	// ========= raw level API
	bool setGamma(double r, double g, double b);
	bool setGamma(double r);
	bool setMonitorGamma(HDC hdc, double r, double g, double b);
	bool setMonitorGamma(HDC hdc, double gamma);
  double correctGamma(double gamma);
  void GammaController::redraw();

	// ========= monitor間のガンマ差を維持したまま操作できるAPI
	bool reset();
	bool increment();
	bool decrement();
	bool resetMonitorDifference();
	bool setMonitorGammaDifference(double r, double g, double b);
	bool setMonitorGammaDifference(double gamma);

  // ========= 指定された座標のモニタを対象にするAPI
  int findMonitorAt(POINT *pt);
  MonitorInfo *monitorGetAt(POINT *pt);
	bool incrementAt(POINT *pt);
  bool decrementAt(POINT *pt);
  bool resetMonitorAt(POINT *pt);

  bool incrementAtCursorPos();
  bool decrementAtCursorPos();
  bool resetMonitorAtCursorPos();

	// ========= monitor関係のAPI
	bool monitorAdd(MonitorInfo *monitor);
	bool monitorAdd(HDC, LPTSTR, LPTSTR, RECT);
	MonitorInfo *monitorGet(int index);
  bool monitorReset();
	int monitorGetCount();
	bool hasMultiMonitor(); // 現在の環境は複数のモニタを持っている?
  
	// ========== monitorのインデックス指定して操作する系API(推奨)
	bool resetMonitor(int index);
	bool setMonitorGammaIndex(int index, double r, double g, double b, double level);
	bool setMonitorGammaIndex(int index, double gamma);
};
