#include "pch.h"
#include "framework.h"
#include "MainFrm.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace std::chrono_literals;

constexpr UINT WM_PACKET_RECEIVED = WM_APP + 1;

const int g_bufLength = 65536;
const unsigned int g_port = 8080;
//const unsigned int g_defImageWidth = 1330;
//const unsigned int g_defImageHeight = 990;
//const unsigned int g_defImageWidth = 2028;
//const unsigned int g_defImageHeight = 1520;
const unsigned int g_defImageWidth = 800;
const unsigned int g_defImageHeight = 600;

const int g_listWidth = 400;
const int g_elmPadding = 20;

const unsigned int R = 2;
const unsigned int G = 1;
const unsigned int B = 0;

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

Color CMainFrame::getBayerColor(size_t curCol, unsigned char data)
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

Color CMainFrame::getYUVColor(size_t curCol, unsigned char data)
{
	Color color;

	color.R = data;
	color.G = data;
	color.B = data;

	return color;
}

Color CMainFrame::getColor(size_t curCol, unsigned char data)
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

LRESULT CMainFrame::OnPacketReceived(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	auto now = std::chrono::system_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPacketTimestamp) > 100ms)
	{
		m_curRow = 0;
		m_startFPSTimestamp = now;

		m_logList.AddString(_T("New streaming"));
	}

	m_lastPacketTimestamp = now;

	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startFPSTimestamp) > 1000ms)
	{
		CString fps;
		fps.Format(_T("FPS: %d"), m_frameCount);
		m_logList.AddString(fps);

		m_startFPSTimestamp = now;
		m_frameCount = 0;
	}

	//if ((m_frameQueue.size() > 0) && (m_frameQueue.size() < 10))
	//{
	//	CString str;
	//	str.Format(_T("Packets in queue: %d"), m_frameQueue.size());
	//	m_logList.AddString(str);
	//}

	std::vector<unsigned char> data;
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		if (m_frameQueue.empty())
			return 0;

		data = std::move(m_frameQueue.front());
		m_frameQueue.pop();
	}

	//return 0;

	BYTE* pixels = (BYTE*)m_image.GetBits();
	if (pixels)
	{
		int width = m_image.GetWidth();
		int height = m_image.GetHeight();
		int pitch = m_image.GetPitch();

		size_t rowShift = m_curRow * pitch;
		size_t dataInd = 0;
		size_t curCol = 0;

		for (size_t dataInd = 0; dataInd < data.size(); ++dataInd)
		{
			// SBGGR10 Bayer image data format: 10 bits per pixel.
			// It means 4 bytes of 4 pixels data and 5th byte containing
			// additional bits of pixel data of the previous 4 bytes,
			// 2 bits per each byte.
			if ((m_pixelFormat == PixelFormat::BAYER_RGGB12) && (dataInd % 3 == 2))
				continue;
			else
			if ((m_pixelFormat == PixelFormat::BAYER_RGGB10) && (dataInd % 5 == 4))
				continue;

			//for (int i = 0; i < 2; ++i)
			{
				size_t pixelShift = rowShift + 3 * curCol;

				Color color = getColor(curCol, data[dataInd]);

				pixels[pixelShift + R] = color.R;
				pixels[pixelShift + G] = color.G;
				pixels[pixelShift + B] = color.B;

				++curCol;
			}
		}

		++m_curRow;
		if (m_curRow >= height)
		{
			m_curRow = 0;
			++m_frameCount;
		}

		//// Invalidate just single row
		CRect dirtyRect(0, m_curRow - 1, m_width, m_curRow + 1);
		InvalidateRect(&dirtyRect, FALSE);

		//// Invalidate whole image
		//if (m_curRow == m_height -1)
		//{
		//	auto start = std::chrono::system_clock::now();
		//	//CRect dirtyRect(0, 0, m_curRow - 1, m_height);
		//	CRect dirtyRect(0, m_curRow - 1, m_width, m_curRow + 1);
		//	//InvalidateRect(&dirtyRect, FALSE);
		//	auto end = std::chrono::system_clock::now();
		//	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		//	//m_logList.AddString(_T("Frame rendered"));
		//}
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
	m_logList.AddString(_T("UDP Camera Stream Started"));

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
			::PostMessage(m_hWnd, WM_PACKET_RECEIVED, 0, 0);
		}
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