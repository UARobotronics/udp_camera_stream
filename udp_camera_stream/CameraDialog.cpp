#include "pch.h"

#include "framework.h"
#include "afxdialogex.h"

#include <string>

#include "CameraDialog.h"
#include "udp_camera_stream.h"

constexpr UINT WM_PACKET_RECEIVED = WM_APP + 1;
constexpr UINT WM_UPDATE_DIALOG_DATA_FROM_VARS = WM_APP + 2;

const unsigned int R = 2;
const unsigned int G = 1;
const unsigned int B = 0;

const int g_bufLength = 65536;
const int g_defImageWidth = 800;
const int g_defImageHeight = 600;
const unsigned int g_dataHeaderSize = 4;
const int g_maxFrameValue = 1 << 13 - 1;
const unsigned int g_packetHeaderSize = 42; // UDP and IP network packet headers

const bool g_addDebugInfo = true;
const size_t g_DebugInfoReserve = 50000;

//const std::string g_dstAddress = "192.168.50.122"; // Direct server IP
//const std::string g_dstAddress = "10.99.9.122";
const std::string g_dstAddress = "192.168.50.237"; // Mikrotik IP
const unsigned int g_dstPort = 6666;

using namespace std::chrono_literals;

BEGIN_MESSAGE_MAP(CCameraDialog, CDialogEx)
	ON_WM_PAINT()
	ON_CBN_SELCHANGE(IDC_COMBO_COLOR_DEPTH, &CCameraDialog::OnColorDepthElementChanged)
	ON_BN_CLICKED(IDC_CHECK_FIRST_FRAME_SMOOTH, &CCameraDialog::OnClickedSmoothImage)
	ON_BN_CLICKED(IDC_CHECK_REFRESH_FULL_FRAME, &CCameraDialog::OnClickedRefreshFullFrame)
	ON_MESSAGE(WM_PACKET_RECEIVED, &CCameraDialog::OnPacketReceived)
	ON_MESSAGE(WM_UPDATE_DIALOG_DATA_FROM_VARS, &CCameraDialog::OnUpdateDialogDataFromVars)
END_MESSAGE_MAP()

CString colorDepthToString(uint8_t depth)
{
	switch (depth)
	{
	case ColorDepth::_8_BITS: return CString("8 bits");
	case ColorDepth::_7_BITS: return CString("7 bits");
	case ColorDepth::_6_BITS: return CString("6 bits");
	case ColorDepth::_5_BITS: return CString("5 bits");
	case ColorDepth::_4_BITS: return CString("4 bits");
	case ColorDepth::_3_BITS: return CString("3 bits");
	case ColorDepth::_2_BITS: return CString("2 bits");
	case ColorDepth::_1_BITS: return CString("1 bit");
	}
}

CCameraDialog::CCameraDialog(CWnd* pParent)
	:CDialogEx(IDD_CAMERA_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CCameraDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_SKIP_ROWS, m_skipRows);
	DDX_Text(pDX, IDC_EDIT_MIN_PACKET_SIZE, m_minPacketSize);
	DDX_Text(pDX, IDC_EDIT_TARGET_FPS, m_targetFPS);
	DDX_Check(pDX, IDC_CHECK_FIRST_FRAME_SMOOTH, m_bSmoothFirstFrame);
	DDX_Check(pDX, IDC_CHECK_REFRESH_FULL_FRAME, m_bRefreshFullFrame);
}

BOOL CCameraDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, TRUE);	 // Set big icon
	SetIcon(m_hIcon, FALSE); // Set small icon

	// TODO: Add extra initialization here
	m_pColorDepthComboBox = (CComboBox*)GetDlgItem(IDC_COMBO_COLOR_DEPTH);
	if (m_pColorDepthComboBox)
	{
		int ind = m_pColorDepthComboBox->AddString(_T("8 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_8_BITS));
		ind = m_pColorDepthComboBox->AddString(_T("7 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_7_BITS));
		ind = m_pColorDepthComboBox->AddString(_T("6 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_6_BITS));
		ind = m_pColorDepthComboBox->AddString(_T("5 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_5_BITS));
		ind = m_pColorDepthComboBox->AddString(_T("4 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_4_BITS));
		ind = m_pColorDepthComboBox->AddString(_T("3 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_3_BITS));
		ind = m_pColorDepthComboBox->AddString(_T("2 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_2_BITS));
		ind = m_pColorDepthComboBox->AddString(_T("1 bit"));
		m_pColorDepthComboBox->SetItemData(ind, int(ColorDepth::_1_BITS));

		m_pColorDepthComboBox->SetCurSel(0);
	}

	m_pSkipRowsEdit = (CEdit*)GetDlgItem(IDC_EDIT_SKIP_ROWS);
	m_pTargetFPSEdit = (CEdit*)GetDlgItem(IDC_EDIT_TARGET_FPS);
	m_pLogListBox = (CListBox*)GetDlgItem(IDC_LIST_LOG);

	m_pFPSByFramesEdit = (CEdit*)GetDlgItem(IDC_EDIT_FPS_BY_FRAMES);
	m_pFPSByRowsEdit = (CEdit*)GetDlgItem(IDC_EDIT_FPS_BY_ROWS);
	m_pDataRateEdit = (CEdit*)GetDlgItem(IDC_EDIT_DATARATE);
	m_pFullDataRateEdit = (CEdit*)GetDlgItem(IDC_EDIT_FULL_DATARATE);
	m_pDataLossEdit = (CEdit*)GetDlgItem(IDC_EDIT_DATA_LOSS);
	m_pPacketProcessTimeEdit = (CEdit*)GetDlgItem(IDC_EDIT_PACKET_PROCESS_TIME);

	// Height is negative to create top-down image
	m_image.Create(g_defImageWidth, -g_defImageHeight, 8);

	// Grayscale image requires to setup palette
	RGBQUAD colorTable[256];
	for (int i = 0; i < 256; ++i)
	{
		colorTable[i].rgbRed = i;
		colorTable[i].rgbGreen = i;
		colorTable[i].rgbBlue = i;
	}

	m_image.SetColorTable(0, 256, colorTable);
	m_width = g_defImageWidth;
	m_height = g_defImageHeight;

	bool b = m_communicator.init();
	if (!b)
		return FALSE;

	m_communicator.setDestination(g_dstAddress, g_dstPort);
	setState(State::ConnectToServer);

	debugInfo.packetData.reserve(g_DebugInfoReserve);

	resetStream();
	startWorkerThread();

	return TRUE;
}

BOOL CCameraDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		UpdateData(TRUE);

		if (GetFocus() == GetDlgItem(IDC_EDIT_SKIP_ROWS))
			OnSkipRowsChanged();

		if (GetFocus() == GetDlgItem(IDC_EDIT_MIN_PACKET_SIZE))
			OnMinPacketSizeChanged();

		if (GetFocus() == GetDlgItem(IDC_EDIT_TARGET_FPS))
			OnTargetFPSChanged();

		return TRUE; // still swallow Enter
	}
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
		return TRUE;

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CCameraDialog::setState(State state)
{
	m_state = state;

	switch (state)
	{
		case State::ConnectToServer:
			m_communicator.resetSocketTimeout();
			break;
		case State::WaitConfirmation:
		case State::WaitStreamParams:
			m_communicator.setSocketTimeout(Timeouts::serverConfirmation);
			break;
		case State::ReceiveVideoStream:
			m_communicator.setSocketTimeout(Timeouts::newDataPacket);
			break;
		default:
			break;
	}

	m_stateSwitchTimestamp = std::chrono::system_clock::now();
}

void CCameraDialog::initStreamData()
{
	auto now = std::chrono::system_clock::now();

	m_curRow = 0;
	m_startFPSTimestamp = now;
	m_startGlobalFPSTimestamp = now;
	m_frameCount = 0;
	m_globalFrameCount = 0;
	m_FPSTotalRowCount = 0;
}

void CCameraDialog::resetStream()
{
	initStreamData();

	addLogString(_T("Stream is interrupted. Resetting..."));
}

bool CCameraDialog::processStreamParams(const DataBuffer& data, int size)
{
	if (size != CMD_SEND_STREAM_PARAMS_TO_CLIENT_SIZE)
		return false;

	m_colorDepth = data[1];
	m_skipRows = data[2];
	m_minPacketSize = (data[3] << 8) + data[4];
	m_targetFPS = data[5];

	::PostMessage(m_hWnd, WM_UPDATE_DIALOG_DATA_FROM_VARS, 0, 0);

	return true;
}

void CCameraDialog::startWorkerThread()
{
	m_communicationThread = std::thread(&CCameraDialog::processCommunication, this);
}

void CCameraDialog::processCommunication()
{
	DataBuffer buf(g_bufLength);
	uint32_t packetCount = 0;

	try
	{
		while (true)
		{
			int recvLen;
			bool timeout = false;
			auto now = std::chrono::system_clock::now();
			auto lastSwitchDelay = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stateSwitchTimestamp).count();

			switch (m_state)
			{
			case State::ConnectToServer:
				m_communicator.sendCmdConnectToServer();
				setState(State::WaitConfirmation);

				break;
			case State::WaitConfirmation:
				recvLen = m_communicator.receiveData(buf, &timeout);
				
				if (timeout || (lastSwitchDelay > Timeouts::serverConfirmation))
				{
					// No confirmation received within specified interval. Try again.
					setState(State::ConnectToServer);
					break;
				}

				if (recvLen != CMD_SEND_ACK_TO_CLIENT_SIZE)
					break;

				if (buf[0] != CMD_SEND_ACK_TO_CLIENT)
					break;

				setState(State::WaitStreamParams);

				break;
			case State::WaitStreamParams:
				recvLen = m_communicator.receiveData(buf, &timeout);

				if (timeout || (lastSwitchDelay > Timeouts::serverConfirmation))
				{
					setState(State::ConnectToServer);
					break;
				}

				if (recvLen != CMD_SEND_STREAM_PARAMS_TO_CLIENT_SIZE)
					break;

				if (!processStreamParams(buf, recvLen))
					break;

				initStreamData();

				setState(State::ReceiveVideoStream);

				break;
			case State::ReceiveVideoStream:
				recvLen = m_communicator.receiveData(buf, &timeout);

				if (timeout)
				{
					// Reset connection to camera
					resetStream();
					setState(State::ConnectToServer);

					break;
				}

				if (recvLen < 0)
					break;

				DataBuffer packet(buf.begin(), buf.begin() + recvLen);
				{
					std::lock_guard<std::mutex> lock(m_queueMutex);
					m_frameQueue.push(std::move(packet));
				}
				// notify UI thread that a frame (or packet) is ready
				if (m_hWnd)
					::PostMessage(m_hWnd, WM_PACKET_RECEIVED, 0, 0);

				++packetCount;

				break;
			}
		}
	}
	catch (...)
	{
		return;
	}
}

void CCameraDialog::addLogString(CString str)
{
	m_pLogListBox->AddString(str);
	m_pLogListBox->SetCurSel(m_pLogListBox->GetCount() - 1);
	m_pLogListBox->SetCurSel(-1);
}

void CCameraDialog::updateMinMaxRow(int* minRow, int* maxRow)
{
	if (m_curRow < *minRow)
		*minRow = m_curRow;
	if (m_curRow > *maxRow)
		*maxRow = m_curRow;
}

void CCameraDialog::onRowFinished(int increment, int* minRow, int* maxRow)
{
	updateMinMaxRow(minRow, maxRow);

	m_curRow += increment;

	if (m_curRow >= m_height)
	{
		m_curRow -= m_height;
		onFrameFinished();
	}

	++m_FPSTotalRowCount;
	++m_currentFrameRowCount;

	if (m_frameCount > 0)
		++m_wholeFramesRowCount;
}

void CCameraDialog::onFrameFinished()
{
	++m_frameCount;

	m_currentFrameRowCount = 0;

	if (m_bRefreshFullFrame)
	{
		m_updateRect = { 0, 0, m_width, m_height };
		InvalidateRect(&m_updateRect, FALSE);
		UpdateWindow();
	}
}

void CCameraDialog::onMeasurePeriodFinished()
{
	auto now = std::chrono::system_clock::now();

	auto globalDuration = std::chrono::duration_cast<std::chrono::seconds>(now - m_startFPSTimestamp).count();
	unsigned int FPSByFrame = m_frameCount / globalDuration;
	m_globalFrameCount += m_frameCount;

	CString str;

	if (m_pFPSByFramesEdit)
	{
		str.Format(_T("%d"), FPSByFrame);
		m_pFPSByFramesEdit->SetWindowText(str);
	}

	if (m_pFPSByRowsEdit)
	{
		str.Format(_T("%d"), m_FPSTotalRowCount / m_height);
		m_pFPSByRowsEdit->SetWindowText(str);
	}

	if (m_pDataRateEdit)
	{
		str.Format(_T("%f"), (float)m_dataBitsCount / (1024 * 1024));
		m_pDataRateEdit->SetWindowTextW(str);
	}

	if (m_pFullDataRateEdit)
	{
		str.Format(_T("%f"), (float)m_fullDataBitsCount / (1024 * 1024));
		m_pFullDataRateEdit->SetWindowTextW(str);
	}

	if (m_pDataLossEdit)
	{
		float rowCount = m_wholeFramesRowCount - m_currentFrameRowCount;
		float expectedRowCount = (float)(m_frameCount - 1) * m_height / (m_packetSkipRows + 1);
		float lossPercent = 100.0 - (rowCount / expectedRowCount) * 100.0;
		str.Format(_T("%f"), (float)lossPercent);
		m_pDataLossEdit->SetWindowTextW(str);
	}

	if (m_pPacketProcessTimeEdit && (m_periodPacketCount > 0))
	{
		unsigned long long averageMicros = m_packetProcessMicros / m_periodPacketCount;
		str.Format(_T("%llu"), averageMicros);
		m_pPacketProcessTimeEdit->SetWindowTextW(str);
	}

	m_startFPSTimestamp = now;
	m_frameCount = 0;
	m_FPSTotalRowCount = 0;
	m_wholeFramesRowCount = 0;
	m_dataBitsCount = 0;
	m_fullDataBitsCount = 0;
	m_periodPacketCount = 0;
	m_packetProcessMicros = 0;
}

bool CCameraDialog::parsePacketHeader(const DataBuffer& data, FrameIndex& frame, RowIndex& row, uint8_t& flags, RowIndex& skipRows, uint8_t& colorDepth)
{
	if (data.size() < g_dataHeaderSize)
		return false;

	frame = ((uint16_t)(data[0]) << 5) + ((uint16_t)(data[1]) >> 3);
	row = ((uint16_t)(data[1] & 0x7) << 8) + data[2];
	//row = ((uint16_t)(data[0] & 0xFF) << 8) + data[1];
	flags = data[3] >> 3;
	skipRows = (data[3] >> 3) & 0x7;
	colorDepth = data[3] & 0x7;

	return true;
}

void CCameraDialog::OnOK()
{
}

void CCameraDialog::OnColorDepthElementChanged()
{
	if (!m_pColorDepthComboBox)
		return;

	int ind = m_pColorDepthComboBox->GetCurSel();
	m_colorDepth = m_pColorDepthComboBox->GetItemData(ind);

	m_communicator.sendCmdSetColorDepth(m_colorDepth);
}

void CCameraDialog::OnColorDepthValueChanged()
{
	if (!m_pColorDepthComboBox)
		return;

	for (int ind = m_pColorDepthComboBox->GetCount() - 1; ind >= 0; --ind)
	{
		if (m_pColorDepthComboBox->GetItemData(ind) == m_colorDepth)
		{
			m_pColorDepthComboBox->SetCurSel(ind);
			return;
		}
	}

	m_pColorDepthComboBox->SetCurSel(0);
}

void CCameraDialog::OnSkipRowsChanged()
{
	m_communicator.sendCmdSetSkipRows(m_skipRows);
}

void CCameraDialog::OnMinPacketSizeChanged()
{
	m_communicator.sendCmdSetMinPacketSize(m_minPacketSize);
}

void CCameraDialog::OnTargetFPSChanged()
{
	m_communicator.sendCmdSetTargetFPS(m_targetFPS);
}

LRESULT CCameraDialog::OnPacketReceived(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	auto now = std::chrono::system_clock::now();

	m_lastPacketTimestamp = now;
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startFPSTimestamp).count();

	if (duration > 1000)
		onMeasurePeriodFinished();

	std::queue<DataBuffer> dataQueue;
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		std::swap(m_frameQueue, dataQueue);
	}

	BYTE* pixels = (BYTE*)m_image.GetBits();
	if (pixels)
	{
		int maxRow = -1;
		int minRow = std::numeric_limits<int>::max();
		while (!dataQueue.empty())
		{
			DataBuffer data = dataQueue.front();
			dataQueue.pop();
			size_t dataSize = data.size();

			auto packetProcessStart = std::chrono::system_clock::now();

			unsigned int newImageBitsCount = (dataSize - g_dataHeaderSize) * 8;
			m_dataBitsCount += newImageBitsCount;
			m_fullDataBitsCount += (dataSize + g_packetHeaderSize) * 8;
			++m_periodPacketCount;

			if (data.size() < m_minPacketSize)
				continue;

			int width = m_image.GetWidth();
			int height = m_image.GetHeight();
			int pitch = m_image.GetPitch();

			uint8_t flags;
			uint8_t colorDepth;
			FrameIndex curFrame;

			static RowIndex prevRow = 0;
			static FrameIndex prevCurFrame = 0;
			RowIndex frameCurRow;
			if (!parsePacketHeader(data, curFrame, frameCurRow, flags, m_packetSkipRows, colorDepth))
				continue;

			if ((frameCurRow < m_curRow) && (m_curRow - frameCurRow > m_height / 2))
				onFrameFinished();

			m_curRow = frameCurRow;

			if ((m_curRow > 0) && (prevRow != m_curRow - 1))
			{
				CString strRows;
				strRows.Format(_T("Rows skipped! Frame: %d, Cur row: %d, prev row: %d"), curFrame, m_curRow, prevRow);
				//addLogString(strRows);
			}

			bool skip = false;
			if ((prevCurFrame > curFrame) && (prevCurFrame - curFrame < 500))
				skip = true;

			//if (m_curRow < prevRow)
			//	onFrameFinished();

			if (skip)
				continue;

			if (m_curRow >= m_height)
				continue;

			prevRow = m_curRow;
			prevCurFrame = curFrame;

			size_t rowShift = m_curRow * pitch;
			size_t curCol = 0;
			int rowPixels = 0;

			int colorSmooth = 1 << colorDepth;
			int maxLuminance = 256 - colorSmooth;

			for (size_t dataInd = g_dataHeaderSize; dataInd < dataSize; dataInd += 1)
			{
				uint8_t dataByte = data[dataInd];

				// Compress several adjacent pixels into one color value
				int count = (dataByte % colorSmooth) + 1;
				rowPixels += count;
				uint8_t luminance = dataByte - (dataByte % colorSmooth);
				luminance = (float)(luminance) / maxLuminance * 255;
				for (int i = 0; i < count; ++i)
				{
					size_t pixelShift = rowShift + curCol;
					pixels[pixelShift] = luminance;

					if (m_bSmoothFirstFrame)
					{
						//if ((curFrame + m_curRow) % (skipRows + 1) == 0)
						if (m_curRow % (m_packetSkipRows + 1) == 0)
						{
							switch (m_packetSkipRows)
							{
							case 1:
							case 2:
							case 3:
							case 4:
								if ((m_curRow > 0) && (m_curRow < m_height) && (curCol > 1) && (curCol < m_width - 2))
								{
									uint16_t prevRow = m_curRow - m_packetSkipRows - 1;
									size_t prevPixelInd = pixelShift - (m_packetSkipRows + 1) /*row*/ * pitch;
									uint8_t prevLuminance = pixels[prevPixelInd];

									for (int i = 1; i < m_packetSkipRows + 1; ++i)
									{
										//size_t blendPixelInd = pixelShift - 1 /*row*/ * pitch - 1 /*column*/ * 3;
										size_t blendPixelInd = pixelShift - i /*row*/ * pitch;
										uint16_t blendLuminance = ((m_packetSkipRows + 1 - i) * luminance + i * prevLuminance) / (m_packetSkipRows + 1);

										pixels[blendPixelInd] = blendLuminance;
									}
								}

								break;
							default:
								break;
							}
						}
					}

					++curCol;

					if (curCol >= m_width)
					{
						rowPixels = 0;
						curCol = 0;

						onRowFinished(m_packetSkipRows + 1, &minRow, &maxRow);
						rowShift = m_curRow * pitch;

						if (g_addDebugInfo)
							debugInfo.packetData.emplace_back(curFrame, m_curRow);
					}
				}
			}

			// In case if part of row data was lost, in the loop above it won't be
			// treated as finished, so process its finishing here.
			if (curCol > 0)
				onRowFinished(m_packetSkipRows + 1, &minRow, &maxRow);

			auto now = std::chrono::system_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - packetProcessStart).count();
			m_packetProcessMicros += duration;
		}

		// Invalidate just updated area
		if (!m_bRefreshFullFrame)
		{
			if (minRow <= 0)
				minRow = 1;
			m_updateRect = { 0, minRow - 1, m_width, maxRow + 1 };
			InvalidateRect(&m_updateRect, FALSE);
			UpdateWindow();
		}
	}

	return 0;
}

LRESULT CCameraDialog::OnUpdateDialogDataFromVars(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	UpdateData(FALSE);

	OnColorDepthValueChanged();

	return 0;
}

void CCameraDialog::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CPaintDC dc(this);

		// Draw your image
		if ((HBITMAP)m_image)
		{
			//m_image.Draw(dc.GetSafeHdc(), 0, 0);

			m_image.BitBlt(dc.GetSafeHdc(),
				m_updateRect.left, m_updateRect.top, // Top left of the dst
				m_updateRect.Width(), m_updateRect.Height(),
				m_updateRect.left, m_updateRect.top,
				SRCCOPY);
		}

		CDialogEx::OnPaint();
	}
}

void CCameraDialog::OnClickedSmoothImage()
{
	UpdateData(TRUE);
}

void CCameraDialog::OnClickedRefreshFullFrame()
{
	UpdateData(TRUE);
}