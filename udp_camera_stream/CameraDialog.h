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

	void OnColorDepthElementChanged();
	void OnColorDepthValueChanged();
	void OnSkipRowsChanged();
	void OnMinPacketSizeChanged();
	void OnTargetFPSChanged();
	LRESULT OnPacketReceived(WPARAM /*wParam*/, LPARAM /*lParam*/);
	LRESULT OnUpdateDialogDataFromVars(WPARAM /*wParam*/, LPARAM /*lParam*/);

	afx_msg void OnPaint();
	afx_msg void OnClickedSmoothImage();
	afx_msg void OnClickedRefreshFullFrame();

	DECLARE_MESSAGE_MAP()

	void setState(State state);
	void initStreamData();
	void resetStream();

	bool processStreamParams(const DataBuffer& data, int size);
	void startWorkerThread();
	void processCommunication();
	void addLogString(CString str);
	void updateMinMaxRow(int* minRow, int* maxRow);
	void onRowFinished(int increment, int* minRow, int* maxRow);
	void onFrameFinished();
	void onMeasurePeriodFinished();
	bool parsePacketHeader(const DataBuffer& data, FrameIndex& frame, RowIndex& row, uint8_t& flags, RowIndex& skipRows, uint8_t& format);

private:
	CImage m_image;
	int m_colorDepth = 0;
	int m_skipRows = 0;
	RowIndex m_packetSkipRows = 0;
	int m_minPacketSize = 0;
	uint8_t m_targetFPS = 0;
	BOOL m_bSmoothFirstFrame = false;
	BOOL m_bRefreshFullFrame = true;

	std::thread m_thread;
	Communicator m_communicator;
	DebugInfo debugInfo;
	State m_state = State::Undefined;
	std::mutex m_queueMutex;
	std::queue<DataBuffer> m_frameQueue;

	int m_width = 0;
	int m_height = 0;
	RowIndex m_curRow = 0;

	std::chrono::time_point<std::chrono::system_clock> m_lastPacketTimestamp{};
	std::chrono::time_point<std::chrono::system_clock> m_startFPSTimestamp{};
	std::chrono::time_point<std::chrono::system_clock> m_startGlobalFPSTimestamp{};
	std::chrono::time_point<std::chrono::system_clock> m_stateSwitchTimestamp{};

	unsigned int m_frameCount = 0;
	unsigned int m_globalFrameCount = 0;
	unsigned int m_FPSTotalRowCount = 0;
	unsigned int m_wholeFramesRowCount = 0;
	unsigned int m_currentFrameRowCount = 0;
	unsigned int m_dataBitsCount = 0;
	unsigned int m_fullDataBitsCount = 0;

private:
	CComboBox* m_pColorDepthComboBox = nullptr;
	CEdit* m_pSkipRowsEdit = nullptr;
	CEdit* m_pTargetFPSEdit = nullptr;
	CListBox* m_pLogListBox = nullptr;

	CEdit* m_pFPSByFramesEdit = nullptr;
	CEdit* m_pFPSByRowsEdit = nullptr;
	CEdit* m_pDataRateEdit = nullptr;
	CEdit* m_pFullDataRateEdit = nullptr;
	CEdit* m_pDataLossEdit = nullptr;
};