#include "pch.h"
#include "framework.h"
#include "animalChess.h"
#include "animalChessDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CanimalChessApp, CWinApp)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

CanimalChessApp::CanimalChessApp()
{
}

CanimalChessApp theApp;

BOOL CanimalChessApp::InitInstance()
{
    CWinApp::InitInstance();

    //启动主对话框。
    CanimalChessDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    return FALSE;
}