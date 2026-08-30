#pragma once
#include "engine.h"

class CanimalChessDlg : public CDialogEx
{
public:
    CanimalChessDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ANIMALCHESS_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    HICON m_hIcon;
    CString m_strStatus;

    CRect m_rcBtnEasy;
    CRect m_rcBtnMedium;
    CRect m_rcBtnHard;
    CRect m_rcBtnReset;

    void DrawMoveArrow(CDC* pDC, CPoint ptStart, CPoint ptEnd);

    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()
};