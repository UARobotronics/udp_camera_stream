#include "pch.h"

#include "framework.h"
#include "afxdialogex.h"

#include <string>

#include "CameraDialog.h"
#include "udp_camera_stream.h"

constexpr UINT WM_PACKET_RECEIVED = WM_APP + 1;

const unsigned int R = 2;
const unsigned int G = 1;
const unsigned int B = 0;

const int g_bufLength = 65536;
const unsigned int g_defImageWidth = 800;
const unsigned int g_defImageHeight = 600;
const unsigned int g_packetHeaderSize = 4;

const std::string g_dstAddress = "192.168.50.122";
const unsigned int g_dstPort = 6666;

using namespace std::chrono_literals;

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

BEGIN_MESSAGE_MAP(CCameraDialog, CDialogEx)
	ON_WM_PAINT()
	ON_CBN_SELCHANGE(IDC_COMBO_COLOR_DEPTH, &CCameraDialog::OnColorDepthChanged)
	ON_MESSAGE(WM_PACKET_RECEIVED, &CCameraDialog::OnPacketReceived)
END_MESSAGE_MAP()

void CCameraDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_SKIP_ROWS, m_skipRows);
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

	m_pLogListBox = (CListBox*)GetDlgItem(IDC_LIST_LOG);

	m_image.Create(g_defImageWidth, g_defImageHeight, 24);

	m_width = g_defImageWidth;
	m_height = g_defImageHeight;

	m_state = State::ConnectToServer;

	bool b = m_communicator.init();
	if (!b)
		return FALSE;

	m_communicator.setDestination(g_dstAddress, g_dstPort);

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

		return TRUE; // still swallow Enter
	}
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
		return TRUE;

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CCameraDialog::startWorkerThread()
{
	m_thread = std::thread(&CCameraDialog::processUDP, this);
}

void CCameraDialog::processUDP()
{
	std::vector<unsigned char> buf(g_bufLength);
	uint32_t packetCount = 0;

	try
	{
		while (true)
		{
			int recvLen;

			switch (m_state)
			{
			case State::ConnectToServer:
				m_communicator.sendCmdConnectToServer();
				m_state = State::WaitConfirmation;

				break;
			case State::WaitConfirmation:
				recvLen = m_communicator.receiveData(buf);
				if (recvLen != CMD_SEND_ACK_TO_CLIENT_SIZE)
					break;

				if (buf[0] != CMD_SEND_ACK_TO_CLIENT)
					break;

				m_state = State::ReceiveVideoStream;

				break;
			case State::ReceiveVideoStream:
				recvLen = m_communicator.receiveData(buf);

				// push the received packet (copy only the bytes received)
				std::vector<unsigned char> packet(buf.begin(), buf.begin() + recvLen);
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

	++m_FPSTotalRowCount;
}

void CCameraDialog::incCurRow(int increment, int* minRow, int* maxRow)
{
	updateMinMaxRow(minRow, maxRow);

	m_curRow += increment;

	if (m_curRow >= m_height)
		m_curRow = 0;
}

void CCameraDialog::parsePacketHeader(const std::vector<unsigned char>& data, uint16_t& frame, uint16_t& row, uint8_t& flags, uint8_t& skipRows, uint8_t& colorDepth)
{
	if (data.size() < g_packetHeaderSize)
		return;

	frame = ((uint16_t)(data[0]) << 5) + ((uint16_t)(data[1]) >> 3);
	row = ((uint16_t)(data[1] & 0x7) << 8) + data[2];
	//row = ((uint16_t)(data[0] & 0xFF) << 8) + data[1];
	flags = data[3] >> 3;
	skipRows = (data[3] >> 3) & 0x7;
	colorDepth = data[3] & 0x7;
}

void CCameraDialog::OnOK()
{
}

void CCameraDialog::OnColorDepthChanged()
{
	if (m_pColorDepthComboBox)
	{
		int ind = m_pColorDepthComboBox->GetCurSel();
		m_colorDepth = m_pColorDepthComboBox->GetItemData(ind);

		m_communicator.sendCmdSetColorDepth(m_colorDepth);
	}
}

void CCameraDialog::OnSkipRowsChanged()
{
	m_communicator.sendCmdSetSkipRows(m_skipRows);
}

LRESULT CCameraDialog::OnPacketReceived(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	auto now = std::chrono::system_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPacketTimestamp) > 3000ms)
	{
		m_curRow = 0;
		m_startFPSTimestamp = now;
		m_startGlobalFPSTimestamp = now;
		m_frameCount = 0;
		m_globalFrameCount = 0;
		m_FPSTotalRowCount = 0;

		addLogString(_T("New streaming"));
	}

	m_lastPacketTimestamp = now;
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startFPSTimestamp).count();

	// Output FPS every second
	if (duration > 1000)
	{
		m_globalFrameCount += m_frameCount;

		auto globalDuration = std::chrono::duration_cast<std::chrono::seconds>(now - m_startGlobalFPSTimestamp).count();
		unsigned int globalFPS = m_globalFrameCount / globalDuration;

		CString fps;
		fps.Format(_T("FPS: %d / %d"), /*m_frameCount, */m_FPSTotalRowCount / m_height, globalFPS/*, duration*/);
		//m_logList.AddString(fps);
		//addLogString(fps);

		m_startFPSTimestamp = now;
		m_frameCount = 0;
		m_FPSTotalRowCount = 0;
	}

	//if ((m_frameQueue.size() > 0) && (m_frameQueue.size() < 10))
	//{
	//	CString str;
	//	str.Format(_T("Packets in queue: %d"), m_frameQueue.size());
	//	m_logList.AddString(str);
	//  addLogString(str);
	//}

	//return 0;

	std::queue<std::vector<unsigned char>> dataQueue;
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
			std::vector<unsigned char> data = dataQueue.front();
			dataQueue.pop();

			int width = m_image.GetWidth();
			int height = m_image.GetHeight();
			int pitch = m_image.GetPitch();

			uint8_t flags;
			uint8_t colorDepth;
			uint8_t skipRows;
			uint16_t curFrame;

			static uint16_t prevRow = 0;
			static uint8_t prevColorDepth = uint8_t(ColorDepth::_8_BITS);
			static uint8_t prevSkipRows = 0;
			static uint16_t prevCurFrame = 0;
			parsePacketHeader(data, curFrame, m_curRow, flags, skipRows, colorDepth);

			if (prevColorDepth != colorDepth)
			{
				CString strDepth = colorDepthToString(colorDepth);
				//addLogString(strDepth);
			}

			if (prevSkipRows != skipRows)
			{
				CString strRows;
				strRows.Format(_T("Now skipping %d rows every frame"), skipRows);
				//addLogString(strRows);
			}

			if ((m_curRow > 0) && (prevRow != m_curRow - 1))
			{
				CString strRows;
				strRows.Format(_T("Rows skipped! Frame: %d, Cur row: %d, prev row: %d"), curFrame, m_curRow, prevRow);
				//addLogString(strRows);
			}

			bool skip = false;
			if (prevCurFrame > curFrame)
				skip = true;

			if (m_curRow < prevRow)
			{
				// New frame has started
				++m_frameCount;

				CRect dirtyRect(0, 0, m_width, m_height);
				InvalidateRect(&dirtyRect, FALSE);
				UpdateWindow();
			}

			prevRow = m_curRow;
			prevColorDepth = colorDepth;
			prevSkipRows = skipRows;
			prevCurFrame = curFrame;

			if (skip)
				continue;

			if (m_curRow >= m_height)
				continue;

			size_t rowShift = m_curRow * pitch;
			size_t curCol = 0;
			size_t dataSize = data.size();
			int rowPixels = 0;

			int colorSmooth = 1 << colorDepth;
			int maxLuminance = 256 - colorSmooth;

			for (size_t dataInd = g_packetHeaderSize; dataInd < dataSize; dataInd += 1)
			{
				uint8_t dataByte = data[dataInd];

				// Compress several adjacent pixels into one color value
				int count = (dataByte % colorSmooth) + 1;
				rowPixels += count;
				uint8_t luminance = dataByte - (dataByte % colorSmooth);
				luminance = (float)(luminance) / maxLuminance * 255;
				for (int i = 0; i < count; ++i)
				{
					if (curCol >= m_width)
					{
						rowPixels = 0;
						curCol = 0;

						incCurRow(skipRows + 1, &minRow, &maxRow);
						rowShift = m_curRow * pitch;
					}

					size_t pixelShift = rowShift + 3 * curCol;

					//Color color = getColor(curCol, data[dataInd]);

					//pixels[pixelShift + R] = color.R;
					//pixels[pixelShift + G] = color.G;
					//pixels[pixelShift + B] = color.B;

					pixels[pixelShift + R] = luminance;
					pixels[pixelShift + G] = luminance;
					pixels[pixelShift + B] = luminance;

					++curCol;
				}
			}

			updateMinMaxRow(&minRow, &maxRow);
		}

		// Invalidate just updated area
		//CRect dirtyRect(0, minRow - 1, m_width, maxRow + 1);
		//InvalidateRect(&dirtyRect, FALSE);
	}

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
		if ((HBITMAP)m_image)  // Check if image is loaded
			m_image.Draw(dc.GetSafeHdc(), 0, 0);  // Draw at position (10, 10)

		CDialogEx::OnPaint();
	}
}
