#pragma once
#include "engine.h"
#include "LanNetwork.h"

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
    enum class GameMode {
        LocalAi,
        LanHost,
        LanGuest
    };

    HICON m_hIcon;
    int m_selectedIdx;

    CButton m_lanGroup;
    CStatic m_addressLabel;
    CEdit m_addressEdit;
    CStatic m_portLabel;
    CEdit m_portEdit;
    CButton m_localAiButton;
    CButton m_hostButton;
    CButton m_joinButton;
    CButton m_disconnectButton;
    CStatic m_networkStatus;

    LanSession m_lanSession;
    GameMode m_gameMode;
    bool m_waitingForGuestConfirmation;
    uint8_t m_pendingSrc;
    uint8_t m_pendingDst;

    void DrawMoveArrow(CDC* pDC, CPoint ptStart, CPoint ptEnd);
    void CreateNetworkControls();
    void UpdateNetworkControls();
    void SetNetworkStatus(const CString& text);
    bool ReadPort(uint16_t& port) const;
    bool ReadConnectionSettings(CString& address, uint16_t& port) const;
    bool IsLocalPlayersTurn(const MsgBoardSnapshot& snap) const;
    bool IsLocalPiece(uint8_t piece) const;
    void ResetSelection();

    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLocalAiClicked();
    afx_msg void OnHostClicked();
    afx_msg void OnJoinClicked();
    afx_msg void OnDisconnectClicked();
    afx_msg LRESULT OnLanStateChanged(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnLanMoveReceived(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};
