// MainFrm.h : interface of the CMainFrame class
//

#pragma once

#include "afxwin.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

enum class PixelFormat
{
	BAYER_RGGB12,
	BAYER_RGGB10,
	YUV420
};

struct Color
{
	byte R = 0;
	byte G = 0;
	byte B = 0;
};

class CMainFrame : public CFrameWnd
{
public:
	CMainFrame() noexcept;

	// Update the display (called from UI thread)
	void UpdateFrame();

	// Worker control
	void StartStreamWorker();
	void StopStreamWorker();

protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnPacketReceived(WPARAM wParam, LPARAM lParam);
	afx_msg void OnPaint();  // Add OnPaint handler declaration
	afx_msg BOOL PreCreateWindow(CREATESTRUCT& cs);
	afx_msg void OnSize(UINT nType, int cx, int cy);  // Add handler for window resizing

	DECLARE_MESSAGE_MAP()

private:
	void loop();
	void UpdateLayout();  // Helper to manage control positions

	inline Color getBayerColor(size_t curCol, unsigned char data);
	inline Color getYUVColor(size_t curCol, unsigned char data);
	inline Color getColor(size_t curCol, unsigned char data);

	void addLogString(CString str);
	void incCurRow(int* minRow, int* maxRow);
private:
	CImage m_image;  // Member variable for the image
	CListBox m_logList;  // Log list box

	int m_width = 0;
	int m_height = 0;
	uint16_t m_curRow = 0;
	//PixelFormat m_pixelFormat = PixelFormat::BAYER_RGGB10;
	PixelFormat m_pixelFormat = PixelFormat::YUV420;

	// networking / worker thread
	std::thread m_thread;
	std::atomic_bool m_streamRunning{ false };

	// queue for raw frame packets (push from worker thread, pop on UI thread)
	std::mutex m_queueMutex;
	std::queue<std::vector<unsigned char>> m_frameQueue;
	std::chrono::time_point<std::chrono::system_clock> m_lastPacketTimestamp{};
	std::chrono::time_point<std::chrono::system_clock> m_startFPSTimestamp{};
	std::chrono::time_point<std::chrono::system_clock> m_startGlobalFPSTimestamp{};

	unsigned int m_frameCount = 0;
	unsigned int m_globalFrameCount = 0;
	unsigned int m_FPSTotalRowCount = 0;
	//unsigned int m_colorBitShift = 2;
	unsigned int m_colorBitShift = 0;
};

