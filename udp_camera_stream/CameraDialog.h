#pragma once

#include "resource.h"

#include <chrono>
#include <mutex>
#include <queue>
#include <thread>

#include "Common.h"
#include "Communicator.h"

class CCameraDialog : public CDialogEx
{
public:
	CCameraDialog(CWnd* pParent = nullptr);

protected:
	HICON m_hIcon;

	void DoDataExchange(CDataExchange* pDX) override;
	BOOL OnInitDialog() override;
	BOOL PreTranslateMessage(MSG* pMsg) override;
	void OnOK() override;

	void OnColorDepthChanged();
	void OnSkipRowsChanged();
	LRESULT OnPacketReceived(WPARAM /*wParam*/, LPARAM /*lParam*/);

	afx_msg void OnPaint();
	afx_msg void PreTranslateMsg();

	DECLARE_MESSAGE_MAP()

	void startWorkerThread();
	void processUDP();
	void addLogString(CString str);
	void updateMinMaxRow(int* minRow, int* maxRow);
	void incCurRow(int increment, int* minRow, int* maxRow);
	void parsePacketHeader(const std::vector<unsigned char>& data, uint16_t& frame, uint16_t& row, uint8_t& flags, uint8_t& skipRows, uint8_t& format);

private:
	CImage m_image;
	int m_colorDepth = 0;
	int m_skipRows = 0;
	std::thread m_thread;
	Communicator m_communicator;
	State m_state = State::Undefined;
	std::mutex m_queueMutex;
	std::queue<std::vector<unsigned char>> m_frameQueue;

	int m_width = 0;
	int m_height = 0;
	uint16_t m_curRow = 0;

	std::chrono::time_point<std::chrono::system_clock> m_lastPacketTimestamp{};
	std::chrono::time_point<std::chrono::system_clock> m_startFPSTimestamp{};
	std::chrono::time_point<std::chrono::system_clock> m_startGlobalFPSTimestamp{};

	unsigned int m_frameCount = 0;
	unsigned int m_globalFrameCount = 0;
	unsigned int m_FPSTotalRowCount = 0;

private:
	CComboBox* m_pColorDepthComboBox = nullptr;
	CEdit* m_pSkipRowsEdit = nullptr;
	CListBox* m_pLogListBox = nullptr;
};