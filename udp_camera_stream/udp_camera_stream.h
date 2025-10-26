
// udp_camera_stream.h : main header file for the udp_camera_stream application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


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
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern UDPCameraStreamApp theApp;
