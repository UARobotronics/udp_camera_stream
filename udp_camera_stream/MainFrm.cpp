#include "pch.h"

#include "framework.h"
#include "MainFrm.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#include <limits>

#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace std::chrono_literals;

constexpr UINT WM_PACKET_RECEIVED = WM_APP + 1;

const int g_bufLength = 65536;
const unsigned int g_port = 8080;
const unsigned int g_defImageWidth = 800;
const unsigned int g_defImageHeight = 600;
const unsigned int g_packetHeaderSize = 4;

const int g_listWidth = 500;
const int g_elmPadding = 20;

const unsigned int R = 2;
const unsigned int G = 1;
const unsigned int B = 0;

ColorDepth intToColorDepth(uint8_t depth)
{
	switch (depth)
	{
		case 0 : return ColorDepth::_8_BYTES;
		case 1 : return ColorDepth::_7_BYTES;
		case 2 : return ColorDepth::_6_BYTES;
		case 3 : return ColorDepth::_5_BYTES;
	}
}

CString colorDepthToString(uint8_t depth)
{
	switch (depth)
	{
		case ColorDepth::_8_BYTES: return CString("8 bytes");
		case ColorDepth::_7_BYTES: return CString("7 bytes");
		case ColorDepth::_6_BYTES: return CString("6 bytes");
		case ColorDepth::_5_BYTES: return CString("5 bytes");
	}
}

CMainFrame::CMainFrame() noexcept
	: CFrameWnd()
{
}

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_PACKET_RECEIVED, &CMainFrame::OnPacketReceived)
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_width = g_defImageWidth;
	m_height = g_defImageHeight;
	m_curRow = 0;

	// To create top-down image, height must be negative
	m_image.Create(m_width, -m_height, 24);

	// Create the list box
	if (!m_logList.Create(
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
		CRect(0, 0, 200, 100),  // Initial size, will be updated in UpdateLayout
		this,
		1234))  // Any unique control ID
	{
		AfxMessageBox(_T("Failed to create list box"));

		return -1;
	}

	// start UDP receive worker
	StartStreamWorker();

	return 0;
}

void CMainFrame::OnDestroy()
{
	StopStreamWorker();
	CFrameWnd::OnDestroy();
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWnd::PreCreateWindow(cs))
		return FALSE;

	// Remove the menu
	cs.hMenu = NULL;

	// Set initial window size to fit image + list + padding
	cs.cx = g_defImageWidth + g_listWidth + g_elmPadding * 3; // Left padding + between padding + right padding
	cs.cy = g_defImageHeight + g_elmPadding * 2;             // Top + bottom padding

	cs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;

	return TRUE;
}

void CMainFrame::StartStreamWorker()
{
	if (m_streamRunning.exchange(true))
		return; // already running

	m_thread = std::thread(&CMainFrame::loop, this);
}

void CMainFrame::StopStreamWorker()
{
	if (!m_streamRunning.exchange(false))
		return;

	if (m_thread.joinable())
		m_thread.join();
}

inline Color CMainFrame::getBayerColor(size_t curCol, unsigned char data)
{
	Color color;

	int pitch = m_image.GetPitch();
	size_t rowShift = m_curRow * pitch;
	size_t pixelShift = rowShift + 3 * curCol;
	BYTE* pixels = (BYTE*)m_image.GetBits();

	if (m_curRow % 2 == 0)
	{
		if (curCol % 2 == 0)
		{
			// B pixel
			color.B = data;
			if (curCol > 0)
				color.G = pixels[pixelShift - 1 * 3 + G];
			if ((m_curRow > 0) && (curCol > 0))
				color.R = pixels[pixelShift - 1 * pitch - 1 * 3 + R];
		}
		else
		{
			// G pixel
			color.G = data;
			if (curCol > 0)
				color.B = pixels[pixelShift - 1 * 3 + B];
			if (m_curRow > 0)
				color.R = pixels[pixelShift - 1 * pitch + R];
		}
	}
	else
	{
		if (curCol % 2 == 0)
		{
			// G pixel
			color.G = data;
			if (m_curRow > 0)
				color.B = pixels[pixelShift - 1 * pitch + B];
			if (curCol > 0)
				color.R = pixels[pixelShift - 1 * 3 + R];
		}
		else
		{
			// R pixel
			color.R = data;
			if (m_curRow > 0)
				color.G = pixels[pixelShift - 1 * pitch + G];
			if ((m_curRow > 0) && (curCol > 0))
				color.B = pixels[pixelShift - 1 * pitch - 1 * 3 + B];
		}
	}

	// Black level is 16 for both 10 and 12 bit Bayer formats.
	// Take it into account.
	color.R = (color.R > 0x10) ? color.R - 0x10 : 0;
	color.G = (color.G > 0x10) ? color.G - 0x10 : 0;
	color.B = (color.B > 0x10) ? color.B - 0x10 : 0;

	return color;
}

inline Color CMainFrame::getYUVColor(size_t curCol, unsigned char data)
{
	Color color;

	color.R = data;
	color.G = data;
	color.B = data;

	return color;
}

inline Color CMainFrame::getColor(size_t curCol, unsigned char data)
{
	Color color;

	switch (m_pixelFormat)
	{
	case PixelFormat::BAYER_RGGB12:
	case PixelFormat::BAYER_RGGB10:
		color = getBayerColor(curCol, data);
		break;
	case PixelFormat::YUV420:
		color = getYUVColor(curCol, data);
		break;
	default:
		return color;
	}

	return color;
}

void CMainFrame::addLogString(CString str)
{
	m_logList.AddString(str);
	m_logList.SetCurSel(m_logList.GetCount() - 1);
	m_logList.SetCurSel(-1);
}

void CMainFrame::updateMinMaxRow(int* minRow, int* maxRow)
{
	if (m_curRow < *minRow)
		*minRow = m_curRow;
	if (m_curRow > *maxRow)
		*maxRow = m_curRow;

	++m_FPSTotalRowCount;
}

void CMainFrame::incCurRow(int increment, int* minRow, int* maxRow)
{
	updateMinMaxRow(minRow, maxRow);

	m_curRow += increment;

	if (m_curRow >= m_height)
		m_curRow = 0;
}

void CMainFrame::parsePacketHeader(const std::vector<unsigned char>& data, uint16_t& frame, uint16_t& row, uint8_t& flags, uint8_t& skipRows, uint8_t& colorDepth)
{
	if (data.size() < g_packetHeaderSize)
		return;

	frame = ((uint16_t)(data[0]) << 5) + ((uint16_t)(data[1]) >> 3);
	row = ((uint16_t)(data[1] & 0x7) << 8) + data[2];
	//row = ((uint16_t)(data[0] & 0xFF) << 8) + data[1];
	flags = data[3] >> 2;
	skipRows = (data[3] >> 2) & 0x7;
	colorDepth = data[3] & 0x3;
}

LRESULT CMainFrame::OnPacketReceived(WPARAM /*wParam*/, LPARAM /*lParam*/)
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
			static uint8_t prevColorDepth = uint8_t(ColorDepth::_8_BYTES);
			static uint8_t prevSkipRows = 0;
			static uint16_t prevCurFrame = 0;
			parsePacketHeader(data, curFrame, m_curRow, flags, skipRows, colorDepth);

			if (prevColorDepth != colorDepth)
			{
				CString strDepth = colorDepthToString(colorDepth);
				addLogString(strDepth);
			}

			if (prevSkipRows != skipRows)
			{
				CString strRows;
				strRows.Format(_T("Now skipping %d rows every frame"), skipRows);
				addLogString(strRows);
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

				// SBGGR10 Bayer image data format: 10 bits per pixel.
				// It means 4 bytes of 4 pixels data and 5th byte containing
				// additional bits of pixel data of the previous 4 bytes,
				// 2 bits per each byte.
				if ((m_pixelFormat == PixelFormat::BAYER_RGGB12) && (dataInd % 3 == 2))
					continue;
				else
				if ((m_pixelFormat == PixelFormat::BAYER_RGGB10) && (dataInd % 5 == 4))
					continue;

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

BOOL CMainFrame::OnEraseBkgnd(CDC* pDC)
{
	// let default handle background erase; you may render latest m_image in OnPaint.
	//return CFrameWnd::OnEraseBkgnd(pDC);
	return true;
}

void CMainFrame::UpdateFrame()
{
	if (!m_image.IsNull())
	{
		CDC* pDC = GetDC();
		if (pDC)
		{
			CRect rect;
			GetClientRect(&rect);

			// Draw the image at its original size at (0,0)
			m_image.BitBlt(pDC->m_hDC, 0, 0);

			ReleaseDC(pDC);
		}
	}

	// Optional: Set list box properties
	m_logList.SetFont(GetFont());  // Use same font as frame

	// Add initial text if desired
	//m_logList.AddString();
	addLogString(_T("UDP Camera Stream Started"));

	// Start UDP worker
	StartStreamWorker();

	// Initial layout
	UpdateLayout();
}

// Add size handler to reposition controls:
void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize(nType, cx, cy);
	UpdateLayout();
}

void CMainFrame::UpdateLayout()
{
	if (!m_logList.GetSafeHwnd())
		return;

	CRect clientRect;
	GetClientRect(&clientRect);

	// Position the list box on the right with padding
	m_logList.MoveWindow(
		clientRect.right - g_listWidth - g_elmPadding,  // Left position with padding from right edge
		g_elmPadding,                                  // Top position with padding
		g_listWidth,                              // Width
		clientRect.Height() - g_elmPadding * 2);      // Height with padding top and bottom

	// If needed, update image position to maintain padding
	if (!m_image.IsNull())
	{
		CRect imageRect;
		imageRect.left = g_elmPadding;
		imageRect.top = g_elmPadding;
		imageRect.right = clientRect.right - g_listWidth - g_elmPadding * 2;
		imageRect.bottom = clientRect.bottom - g_elmPadding;

		// Optional: Handle image scaling if window is resized smaller than image size
		// For now, we'll just let it clip
		InvalidateRect(&imageRect, FALSE);
	}
}

void CMainFrame::loop()
{
	// Basic Winsock UDP receive loop.
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		m_streamRunning = false;
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		WSACleanup();
		m_streamRunning = false;
		return;
	}

	// bind to any address, port 5000 (adjust as needed)
	sockaddr_in si;
	ZeroMemory(&si, sizeof(si));
	si.sin_family = AF_INET;
	si.sin_addr.s_addr = INADDR_ANY;
	si.sin_port = htons(g_port);

	if (bind(s, reinterpret_cast<sockaddr*>(&si), sizeof(si)) == SOCKET_ERROR) {
		int error = WSAGetLastError();
		closesocket(s);
		WSACleanup();
		m_streamRunning = false;

		return;
	}

	// large enough buffer for a UDP packet (increase if you use fragmentation/ larger MTU)
	static std::vector<unsigned char> buf(g_bufLength);
	static uint32_t packetCount = 0;

	while (m_streamRunning) {
		sockaddr_in from;
		int fromlen = sizeof(from);
		int recvLen = recvfrom(s, reinterpret_cast<char*>(buf.data()), g_bufLength, 0,
			reinterpret_cast<sockaddr*>(&from), &fromlen);
		if (recvLen == SOCKET_ERROR) {
			int err = WSAGetLastError();
			// optionally handle specific errors or sleep short time on non-fatal ones
			if (!m_streamRunning)
				break;

			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			continue;
		}

		// push the received packet (copy only the bytes received)
		std::vector<unsigned char> packet(buf.begin(), buf.begin() + recvLen);
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			m_frameQueue.push(std::move(packet));
		}

		// notify UI thread that a frame (or packet) is ready
		if (m_hWnd) {
			//if (packetCount % 4 == 0)
				::PostMessage(m_hWnd, WM_PACKET_RECEIVED, 0, 0);
		}

		++packetCount;
	}

	closesocket(s);
	WSACleanup();
}

void CMainFrame::OnPaint()
{
	if (m_image.IsNull())
		return;

	CPaintDC dc(this);

	// Get the region that needs updating
	CRect updateRect;
	dc.GetClipBox(&updateRect);

	// Draw only that region
	m_image.BitBlt(dc.m_hDC,
		updateRect.left, updateRect.top,
		updateRect.Width(), updateRect.Height(),
		updateRect.left, updateRect.top);
}