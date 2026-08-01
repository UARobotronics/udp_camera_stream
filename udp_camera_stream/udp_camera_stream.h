
// udp_camera_stream.h : main header file for the udp_camera_stream application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols

enum class ColorDepth {
	_8_BITS = 0,
	_7_BITS = 1,
	_6_BITS = 2,
	_5_BITS = 3,
	_4_BITS = 4,
	_3_BITS = 5,
	_2_BITS = 6,
	_1_BITS = 7,
};

// CudpcamerastreamApp:
// See udp_camera_stream.cpp for the implementation of this class
//

class UDPCameraStreamApp : public CWinApp
{
public:
	UDPCameraStreamApp() noexcept;


// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

public:
	DECLARE_MESSAGE_MAP()
};

extern UDPCameraStreamApp theApp;
