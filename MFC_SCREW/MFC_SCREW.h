#pragma once

#ifndef __AFXWIN_H__
#error "pch.h를 포함하기 전에 'afxwin.h'를 포함해야 합니다."
#endif

#include "resource.h"

class CMFCSCREWApp : public CWinApp
{
public:
	CMFCSCREWApp();

public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};

extern CMFCSCREWApp theApp;