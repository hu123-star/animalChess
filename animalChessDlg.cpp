#include "pch.h"
#include "framework.h"
#include "animalChess.h"
#include "animalChessDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {
constexpr UINT IDC_LAN_ADDRESS = 5001;
constexpr UINT IDC_LAN_PORT = 5002;
constexpr UINT IDC_LOCAL_AI = 5003;
constexpr UINT IDC_LAN_HOST = 5004;
constexpr UINT IDC_LAN_JOIN = 5005;
constexpr UINT IDC_LAN_DISCONNECT = 5006;

constexpr int LAN_PANEL_LEFT = 635;
constexpr int LAN_PANEL_RIGHT = 895;

CString Utf8ToCString(const std::string& value)
{
    return CString(CA2T(value.c_str(), CP_UTF8));
}

std::string CStringToUtf8(const CString& value)
{
    return std::string(CT2A(value, CP_UTF8));
}
}

CanimalChessDlg::CanimalChessDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_ANIMALCHESS_DIALOG, pParent)
    , m_selectedIdx(-1)
    , m_gameMode(GameMode::LocalAi)
    , m_waitingForGuestConfirmation(false)
    , m_pendingSrc(255)
    , m_pendingDst(255)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CanimalChessDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CanimalChessDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_LBUTTONDOWN()
    ON_BN_CLICKED(IDC_LOCAL_AI, &CanimalChessDlg::OnLocalAiClicked)
    ON_BN_CLICKED(IDC_LAN_HOST, &CanimalChessDlg::OnHostClicked)
    ON_BN_CLICKED(IDC_LAN_JOIN, &CanimalChessDlg::OnJoinClicked)
    ON_BN_CLICKED(IDC_LAN_DISCONNECT, &CanimalChessDlg::OnDisconnectClicked)
    ON_MESSAGE(WM_APP_LAN_STATE_CHANGED, &CanimalChessDlg::OnLanStateChanged)
    ON_MESSAGE(WM_APP_LAN_MOVE_RECEIVED, &CanimalChessDlg::OnLanMoveReceived)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CanimalChessDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // 棋盘由父窗口双缓冲绘制，排除子控件区域可避免状态刷新时覆盖右侧控件。
    ModifyStyle(0, WS_CLIPCHILDREN);
    SetWindowPos(NULL, 0, 0, 930, 560, SWP_NOMOVE | SWP_NOZORDER);

    Engine_Startup();
    CreateNetworkControls();
    UpdateNetworkControls();

    return TRUE;
}

void CanimalChessDlg::CreateNetworkControls()
{
    const DWORD childTextStyle = WS_CHILD | WS_VISIBLE;

    m_lanGroup.Create(_T("局域网联机"), childTextStyle | BS_GROUPBOX,
        CRect(LAN_PANEL_LEFT, 32, LAN_PANEL_RIGHT, 472), this, 0);
    m_addressLabel.Create(_T("房主地址"), childTextStyle,
        CRect(LAN_PANEL_LEFT + 15, 62, LAN_PANEL_RIGHT - 15, 82), this);
    m_addressEdit.Create(childTextStyle | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
        CRect(LAN_PANEL_LEFT + 15, 84, LAN_PANEL_RIGHT - 15, 108), this, IDC_LAN_ADDRESS);
    m_addressEdit.SetWindowText(_T("127.0.0.1"));
    m_addressEdit.LimitText(253);

    m_portLabel.Create(_T("端口"), childTextStyle,
        CRect(LAN_PANEL_LEFT + 15, 118, LAN_PANEL_RIGHT - 15, 138), this);
    m_portEdit.Create(childTextStyle | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
        CRect(LAN_PANEL_LEFT + 15, 140, LAN_PANEL_RIGHT - 15, 164), this, IDC_LAN_PORT);
    m_portEdit.SetWindowText(_T("9002"));
    m_portEdit.LimitText(5);

    m_localAiButton.Create(_T("本地人机（重新开始）"), childTextStyle | WS_TABSTOP | BS_PUSHBUTTON,
        CRect(LAN_PANEL_LEFT + 15, 182, LAN_PANEL_RIGHT - 15, 212), this, IDC_LOCAL_AI);
    m_hostButton.Create(_T("创建房间（红方）"), childTextStyle | WS_TABSTOP | BS_PUSHBUTTON,
        CRect(LAN_PANEL_LEFT + 15, 220, LAN_PANEL_RIGHT - 15, 250), this, IDC_LAN_HOST);
    m_joinButton.Create(_T("加入房间（蓝方）"), childTextStyle | WS_TABSTOP | BS_PUSHBUTTON,
        CRect(LAN_PANEL_LEFT + 15, 258, LAN_PANEL_RIGHT - 15, 288), this, IDC_LAN_JOIN);
    m_disconnectButton.Create(_T("断开联机"), childTextStyle | WS_TABSTOP | BS_PUSHBUTTON,
        CRect(LAN_PANEL_LEFT + 15, 296, LAN_PANEL_RIGHT - 15, 326), this, IDC_LAN_DISCONNECT);
    m_networkStatus.Create(_T("状态：本地人机"), childTextStyle | SS_LEFT,
        CRect(LAN_PANEL_LEFT + 15, 346, LAN_PANEL_RIGHT - 15, 452), this);

    CFont* font = GetFont();
    CWnd* controls[] = {
        &m_lanGroup, &m_addressLabel, &m_addressEdit, &m_portLabel, &m_portEdit,
        &m_localAiButton, &m_hostButton, &m_joinButton, &m_disconnectButton,
        &m_networkStatus
    };
    for (CWnd* control : controls) {
        control->SetFont(font);
    }
}

void CanimalChessDlg::SetNetworkStatus(const CString& text)
{
    if (m_networkStatus.GetSafeHwnd() != nullptr) {
        m_networkStatus.SetWindowText(text);
    }
}

bool CanimalChessDlg::ReadPort(uint16_t& port) const
{
    CString portText;
    m_portEdit.GetWindowText(portText);
    portText.Trim();

    if (portText.IsEmpty()) return false;

    TCHAR* end = nullptr;
    const unsigned long parsedPort = _tcstoul(portText.GetString(), &end, 10);
    if (end == portText.GetString() || *end != _T('\0') || parsedPort == 0 || parsedPort > 65535) {
        return false;
    }

    port = static_cast<uint16_t>(parsedPort);
    return true;
}

bool CanimalChessDlg::ReadConnectionSettings(CString& address, uint16_t& port) const
{
    m_addressEdit.GetWindowText(address);
    address.Trim();
    return !address.IsEmpty() && ReadPort(port);
}

bool CanimalChessDlg::IsLocalPlayersTurn(const MsgBoardSnapshot& snap) const
{
    if (snap.gameStatus != 0) return false;

    switch (m_gameMode) {
    case GameMode::LocalAi:
        return snap.currentTurn == 0;
    case GameMode::LanHost:
        return m_lanSession.GetState() == LanState::Connected && snap.currentTurn == 0;
    case GameMode::LanGuest:
        return m_lanSession.GetState() == LanState::Connected &&
            !m_waitingForGuestConfirmation && snap.currentTurn == 1;
    }
    return false;
}

bool CanimalChessDlg::IsLocalPiece(uint8_t piece) const
{
    if (m_gameMode == GameMode::LanGuest) {
        return piece >= 17 && piece <= 24;
    }
    return piece >= 1 && piece <= 8;
}

void CanimalChessDlg::ResetSelection()
{
    m_selectedIdx = -1;
}

void CanimalChessDlg::UpdateNetworkControls()
{
    if (m_addressEdit.GetSafeHwnd() == nullptr) return;

    const LanState state = m_lanSession.GetState();
    const bool sessionActive = state == LanState::Hosting ||
        state == LanState::Connecting || state == LanState::Connected;

    m_addressEdit.EnableWindow(!sessionActive);
    m_portEdit.EnableWindow(!sessionActive);
    m_hostButton.EnableWindow(!sessionActive);
    m_joinButton.EnableWindow(!sessionActive);
    m_disconnectButton.EnableWindow(sessionActive);
    m_localAiButton.EnableWindow(TRUE);
}

void CanimalChessDlg::OnLocalAiClicked()
{
    m_lanSession.Stop();
    m_gameMode = GameMode::LocalAi;
    m_waitingForGuestConfirmation = false;
    m_pendingSrc = 255;
    m_pendingDst = 255;
    ResetSelection();
    Engine_Startup();
    SetNetworkStatus(_T("状态：本地人机"));
    UpdateNetworkControls();
    Invalidate(FALSE);
}

void CanimalChessDlg::OnHostClicked()
{
    uint16_t port = 0;
    if (!ReadPort(port)) {
        SetNetworkStatus(_T("状态：请输入有效端口（1-65535）"));
        return;
    }

    m_lanSession.Stop();
    m_gameMode = GameMode::LanHost;
    m_waitingForGuestConfirmation = false;
    m_pendingSrc = 255;
    m_pendingDst = 255;
    ResetSelection();
    Engine_Startup();

    if (!m_lanSession.StartHost(GetSafeHwnd(), port)) {
        CString status;
        status.Format(_T("状态：创建房间失败\r\n%s"),
            Utf8ToCString(m_lanSession.GetLastError()).GetString());
        SetNetworkStatus(status);
    }
    else {
        CString status;
        status.Format(_T("状态：等待蓝方加入\r\n本机 IPv4：%s\r\n端口：%u"),
            Utf8ToCString(LanSession::GetLocalIPv4Address()).GetString(),
            static_cast<unsigned int>(port));
        SetNetworkStatus(status);
    }

    UpdateNetworkControls();
    Invalidate(FALSE);
}

void CanimalChessDlg::OnJoinClicked()
{
    CString address;
    uint16_t port = 0;
    if (!ReadConnectionSettings(address, port)) {
        SetNetworkStatus(_T("状态：请输入有效地址和端口（1-65535）"));
        return;
    }

    m_lanSession.Stop();
    m_gameMode = GameMode::LanGuest;
    m_waitingForGuestConfirmation = false;
    m_pendingSrc = 255;
    m_pendingDst = 255;
    ResetSelection();
    Engine_Startup();

    if (!m_lanSession.Connect(GetSafeHwnd(), CStringToUtf8(address), port)) {
        CString status;
        status.Format(_T("状态：连接失败\r\n%s"),
            Utf8ToCString(m_lanSession.GetLastError()).GetString());
        SetNetworkStatus(status);
    }
    else {
        CString status;
        status.Format(_T("状态：正在连接 %s:%u..."), address.GetString(),
            static_cast<unsigned int>(port));
        SetNetworkStatus(status);
    }

    UpdateNetworkControls();
    Invalidate(FALSE);
}

void CanimalChessDlg::OnDisconnectClicked()
{
    m_lanSession.Stop();
    m_waitingForGuestConfirmation = false;
    m_pendingSrc = 255;
    m_pendingDst = 255;
    ResetSelection();
    SetNetworkStatus(_T("状态：联机已停止，可重新创建或加入房间"));
    UpdateNetworkControls();
    Invalidate(FALSE);
}

LRESULT CanimalChessDlg::OnLanStateChanged(WPARAM wParam, LPARAM lParam)
{
    if (LanNotificationGeneration(wParam) != m_lanSession.GetGeneration()) return 0;

    const LanState state = static_cast<LanState>(LanNotificationValue(wParam));
    const LanRole role = static_cast<LanRole>(lParam);

    // 同一会话可能快速经历多个状态；忽略已被后续状态取代的排队消息。
    if (state != m_lanSession.GetState()) return 0;

    if (role == LanRole::Host) m_gameMode = GameMode::LanHost;
    else if (role == LanRole::Guest) m_gameMode = GameMode::LanGuest;

    CString status;
    switch (state) {
    case LanState::Stopped:
        if (m_gameMode == GameMode::LocalAi) {
            status = _T("状态：本地人机");
        }
        else {
            status = _T("状态：联机已停止，可重新创建或加入房间");
        }
        break;
    case LanState::Hosting:
    {
        uint16_t port = 0;
        ReadPort(port);
        status.Format(_T("状态：等待蓝方加入\r\n本机 IPv4：%s\r\n端口：%u"),
            Utf8ToCString(LanSession::GetLocalIPv4Address()).GetString(),
            static_cast<unsigned int>(port));
        break;
    }
    case LanState::Connecting:
        status = _T("状态：正在连接房主...");
        break;
    case LanState::Connected:
        status = (role == LanRole::Host)
            ? _T("状态：已连接，你是红方（先手）")
            : _T("状态：已连接，你是蓝方（后手）");
        break;
    case LanState::Disconnected:
        status = _T("状态：对方已断开，可重新创建或加入房间");
        m_waitingForGuestConfirmation = false;
        m_pendingSrc = 255;
        m_pendingDst = 255;
        ResetSelection();
        break;
    case LanState::Failed:
        status.Format(_T("状态：联机失败\r\n%s"),
            Utf8ToCString(m_lanSession.GetLastError()).GetString());
        m_waitingForGuestConfirmation = false;
        m_pendingSrc = 255;
        m_pendingDst = 255;
        ResetSelection();
        break;
    }

    SetNetworkStatus(status);
    UpdateNetworkControls();
    Invalidate(FALSE);
    return 0;
}

LRESULT CanimalChessDlg::OnLanMoveReceived(WPARAM wParam, LPARAM lParam)
{
    if (LanNotificationGeneration(wParam) != m_lanSession.GetGeneration()) return 0;
    if (m_lanSession.GetState() != LanState::Connected) return 0;

    const uint8_t src = LanNotificationValue(wParam);
    if (src >= BOARD_CELL_COUNT || lParam < 0 ||
        static_cast<WPARAM>(lParam) >= BOARD_CELL_COUNT) {
        SetNetworkStatus(_T("状态：收到无效走子数据，联机已停止"));
        m_waitingForGuestConfirmation = false;
        m_pendingSrc = 255;
        m_pendingDst = 255;
        ResetSelection();
        m_lanSession.Stop();
        UpdateNetworkControls();
        Invalidate(FALSE);
        return 0;
    }

    const uint8_t dst = static_cast<uint8_t>(lParam);
    MsgBoardSnapshot snap{};
    Engine_GetSnapshot(snap);

    if (m_lanSession.GetRole() == LanRole::Host && m_gameMode == GameMode::LanHost) {
        const uint8_t sourcePiece = snap.board[src];
        const bool isBlueTurnRequest = snap.gameStatus == 0 && snap.currentTurn == 1 &&
            sourcePiece >= 17 && sourcePiece <= 24;

        // 房主是唯一规则权威；网络层只传请求，规则仍在 UI/引擎线程仲裁。
        if (!isBlueTurnRequest || !Engine_IsLegalMove(src, dst) || !Engine_TryMove(src, dst)) {
            SetNetworkStatus(_T("状态：收到蓝方非法走子，联机已停止"));
            m_waitingForGuestConfirmation = false;
            m_pendingSrc = 255;
            m_pendingDst = 255;
            ResetSelection();
            m_lanSession.Stop();
            UpdateNetworkControls();
            Invalidate(FALSE);
            return 0;
        }

        if (!m_lanSession.SendMove(src, dst)) {
            SetNetworkStatus(_T("状态：走子已执行，但回传蓝方失败"));
        }
        else {
            SetNetworkStatus(_T("状态：已连接，你是红方（先手）"));
        }

        ResetSelection();
        Invalidate(FALSE);
        return 0;
    }

    if (m_lanSession.GetRole() == LanRole::Guest && m_gameMode == GameMode::LanGuest) {
        const uint8_t sourcePiece = snap.board[src];
        const bool isBlueMove = sourcePiece >= 17 && sourcePiece <= 24;

        if (isBlueMove && (!m_waitingForGuestConfirmation ||
            src != m_pendingSrc || dst != m_pendingDst)) {
            SetNetworkStatus(_T("状态：主机确认与本地请求不一致，联机已停止"));
            m_waitingForGuestConfirmation = false;
            m_pendingSrc = 255;
            m_pendingDst = 255;
            ResetSelection();
            m_lanSession.Stop();
            UpdateNetworkControls();
            Invalidate(FALSE);
            return 0;
        }

        if (!Engine_IsLegalMove(src, dst) || !Engine_TryMove(src, dst)) {
            SetNetworkStatus(_T("状态：双方棋局不同步，联机已停止"));
            m_waitingForGuestConfirmation = false;
            m_pendingSrc = 255;
            m_pendingDst = 255;
            ResetSelection();
            m_lanSession.Stop();
            UpdateNetworkControls();
            Invalidate(FALSE);
            return 0;
        }

        if (isBlueMove) {
            m_waitingForGuestConfirmation = false;
            m_pendingSrc = 255;
            m_pendingDst = 255;
        }
        SetNetworkStatus(_T("状态：已连接，你是蓝方（后手）"));
        ResetSelection();
        Invalidate(FALSE);
    }

    return 0;
}

void CanimalChessDlg::DrawMoveArrow(CDC* pDC, CPoint ptStart, CPoint ptEnd)
{
    if (ptStart == ptEnd) return;

    CPen pen(PS_SOLID, 3, RGB(255, 69, 0));
    CPen* pOldPen = pDC->SelectObject(&pen);
    CBrush brush(RGB(255, 69, 0));
    CBrush* pOldBrush = pDC->SelectObject(&brush);

    pDC->Ellipse(ptStart.x - 5, ptStart.y - 5, ptStart.x + 5, ptStart.y + 5);

    pDC->MoveTo(ptStart);
    pDC->LineTo(ptEnd);

    double angle = atan2((double)(ptEnd.y - ptStart.y), (double)(ptEnd.x - ptStart.x));
    double arrowLen = 12.0;
    double arrowAngle = 0.5;

    CPoint p1(ptEnd.x - (int)(arrowLen * cos(angle - arrowAngle)),
        ptEnd.y - (int)(arrowLen * sin(angle - arrowAngle)));
    CPoint p2(ptEnd.x - (int)(arrowLen * cos(angle + arrowAngle)),
        ptEnd.y - (int)(arrowLen * sin(angle + arrowAngle)));

    CPoint pts[3] = { ptEnd, p1, p2 };
    pDC->Polygon(pts, 3);

    pDC->SelectObject(pOldPen);
    pDC->SelectObject(pOldBrush);
}

void CanimalChessDlg::OnPaint()
{
    CPaintDC dc(this);

    CRect clientRc;
    GetClientRect(&clientRc);
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, clientRc.Width(), clientRc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    memDC.FillSolidRect(&clientRc, RGB(242, 244, 248));

    //拉取快照，纯依据快照数据绘制。
    MsgBoardSnapshot snap;
    Engine_GetSnapshot(snap);

    const int GRID_SIZE = 65;
    const int OFFSET_X = 30;
    const int OFFSET_Y = 40;

    for (uint8_t i = 0; i < BOARD_CELL_COUNT; i++) {
        int row = i / BOARD_COLS;
        int col = i % BOARD_COLS;

        int px = OFFSET_X + col * GRID_SIZE;
        int py = OFFSET_Y + row * GRID_SIZE;
        CRect rc(px, py, px + GRID_SIZE, py + GRID_SIZE);

        TerrainType t = Engine_GetTerrainByIndex(i);
        if (t == TERRAIN_RIVER) {
            memDC.FillSolidRect(&rc, RGB(140, 200, 245));
            memDC.SetTextColor(RGB(0, 100, 180));
            memDC.DrawText(_T("～水～"), &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else if (t == TERRAIN_RED_DEN) {
            memDC.FillSolidRect(&rc, RGB(255, 190, 200));
            memDC.SetTextColor(RGB(180, 0, 0));
            memDC.DrawText(_T("红穴"), &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else if (t == TERRAIN_BLACK_DEN) {
            memDC.FillSolidRect(&rc, RGB(190, 210, 245));
            memDC.SetTextColor(RGB(0, 0, 180));
            memDC.DrawText(_T("蓝穴"), &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else if (t == TERRAIN_RED_TRAP || t == TERRAIN_BLACK_TRAP) {
            memDC.FillSolidRect(&rc, RGB(230, 230, 230));
            memDC.SetTextColor(RGB(120, 120, 120));
            memDC.DrawText(_T("陷阱"), &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else {
            memDC.FillSolidRect(&rc, RGB(252, 246, 232));
        }

        memDC.DrawEdge(&rc, EDGE_ETCHED, BF_RECT);

        uint8_t pc = snap.board[i];
        if (pc > 0) {
            CRect pieceRc = rc;
            pieceRc.DeflateRect(6, 6);

            bool isRed = (pc <= 8);
            CBrush pBrush(isRed ? RGB(255, 240, 240) : RGB(240, 245, 255));
            CPen pPen(PS_SOLID, 2, isRed ? RGB(200, 30, 30) : RGB(30, 90, 200));
            CPen* pOld = memDC.SelectObject(&pPen);
            CBrush* pOldB = memDC.SelectObject(&pBrush);

            memDC.Ellipse(&pieceRc);

            memDC.SetBkMode(TRANSPARENT);
            memDC.SetTextColor(isRed ? RGB(180, 0, 0) : RGB(0, 50, 180));

            CFont font;
            font.CreatePointFont(130, _T("微软雅黑"));
            CFont* pOldFont = memDC.SelectObject(&font);

            CString strName = CA2T(Engine_GetPieceName(pc));
            memDC.DrawText(strName, &pieceRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            memDC.SelectObject(pOldFont);
            memDC.SelectObject(pOld);
            memDC.SelectObject(pOldB);
        }
    }

    if (m_selectedIdx >= 0 && m_selectedIdx < BOARD_CELL_COUNT) {
        int row = m_selectedIdx / BOARD_COLS;
        int col = m_selectedIdx % BOARD_COLS;
        CRect selRc(OFFSET_X + col * GRID_SIZE, OFFSET_Y + row * GRID_SIZE,
            OFFSET_X + (col + 1) * GRID_SIZE, OFFSET_Y + (row + 1) * GRID_SIZE);

        CPen goldPen(PS_SOLID, 4, RGB(255, 190, 0));
        CPen* pOldPen = memDC.SelectObject(&goldPen);
        memDC.SelectStockObject(NULL_BRUSH);
        memDC.Rectangle(&selRc);
        memDC.SelectObject(pOldPen);
    }

    if (snap.lastSrc < BOARD_CELL_COUNT && snap.lastDst < BOARD_CELL_COUNT) {
        int sRow = snap.lastSrc / BOARD_COLS, sCol = snap.lastSrc % BOARD_COLS;
        int dRow = snap.lastDst / BOARD_COLS, dCol = snap.lastDst % BOARD_COLS;

        CPoint ptStart(OFFSET_X + sCol * GRID_SIZE + GRID_SIZE / 2, OFFSET_Y + sRow * GRID_SIZE + GRID_SIZE / 2);
        CPoint ptEnd(OFFSET_X + dCol * GRID_SIZE + GRID_SIZE / 2, OFFSET_Y + dRow * GRID_SIZE + GRID_SIZE / 2);

        DrawMoveArrow(&memDC, ptStart, ptEnd);
    }

    //状态文本提示。
    CRect statusRc(OFFSET_X, 8, 615, 32);
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(RGB(60, 60, 60));
    CFont statusFont;
    statusFont.CreatePointFont(105, _T("微软雅黑"));
    CFont* pOldF = memDC.SelectObject(&statusFont);

    CString strInfo;
    if (snap.gameStatus == 1) strInfo = _T("【对局结束】红方取得胜利！");
    else if (snap.gameStatus == 2) strInfo = _T("【对局结束】蓝方取得胜利！");
    else if (snap.gameStatus == 3) strInfo = _T("【对局结束】双方和棋！");
    else if (m_gameMode == GameMode::LocalAi) {
        strInfo = (snap.currentTurn == 0)
            ? _T("【当前回合】玩家（红方）")
            : _T("【当前回合】电脑（蓝方）思考中...");
    }
    else if (m_gameMode == GameMode::LanHost) {
        const LanState state = m_lanSession.GetState();
        if (state == LanState::Hosting) {
            strInfo = _T("【局域网】等待蓝方连接...");
        }
        else if (state == LanState::Connected) {
            strInfo = (snap.currentTurn == 0)
                ? _T("【你的回合】红方")
                : _T("【对方回合】等待蓝方走子...");
        }
        else {
            strInfo = _T("【局域网】当前未连接");
        }
    }
    else {
        const LanState state = m_lanSession.GetState();
        if (state == LanState::Connecting) {
            strInfo = _T("【局域网】正在连接红方...");
        }
        else if (state != LanState::Connected) {
            strInfo = _T("【局域网】当前未连接");
        }
        else if (m_waitingForGuestConfirmation) {
            strInfo = _T("【确认中】等待房主确认走子...");
        }
        else {
            strInfo = (snap.currentTurn == 1)
                ? _T("【你的回合】蓝方")
                : _T("【对方回合】等待红方走子...");
        }
    }

    memDC.DrawText(strInfo, &statusRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    memDC.SelectObject(pOldF);

    dc.BitBlt(0, 0, clientRc.Width(), clientRc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

HCURSOR CanimalChessDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CanimalChessDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    const int GRID_SIZE = 65;
    const int OFFSET_X = 30;
    const int OFFSET_Y = 40;

    if (point.x < OFFSET_X || point.y < OFFSET_Y) {
        CDialogEx::OnLButtonDown(nFlags, point);
        return;
    }

    int col = (point.x - OFFSET_X) / GRID_SIZE;
    int row = (point.y - OFFSET_Y) / GRID_SIZE;

    if (col >= 0 && col < BOARD_COLS && row >= 0 && row < BOARD_ROWS)
    {
        uint8_t clickedIdx = static_cast<uint8_t>(row * BOARD_COLS + col);

        MsgBoardSnapshot snap;
        Engine_GetSnapshot(snap);

        if (IsLocalPlayersTurn(snap))
        {
            if (m_selectedIdx == -1) {
                //未选中时，点击己方棋子则高亮。
                if (IsLocalPiece(snap.board[clickedIdx])) {
                    m_selectedIdx = clickedIdx;
                    Invalidate(FALSE);
                }
            }
            else {
                if (IsLocalPiece(snap.board[clickedIdx])) {
                    //改选其他己方棋子。
                    m_selectedIdx = clickedIdx;
                    Invalidate(FALSE);
                }
                else {
                    const uint8_t src = static_cast<uint8_t>(m_selectedIdx);
                    ResetSelection();

                    if (m_gameMode == GameMode::LocalAi) {
                        if (Engine_TryMove(src, clickedIdx)) {
                            Invalidate(FALSE);
                            UpdateWindow();

                            // 只有本地人机模式会触发 AI；联网走子均由远端玩家产生。
                            Engine_TriggerAi();
                            Invalidate(FALSE);
                        }
                    }
                    else if (m_gameMode == GameMode::LanHost) {
                        // 房主先在权威棋盘执行红方走子，再把已确认走子广播给客机。
                        if (Engine_TryMove(src, clickedIdx)) {
                            if (!m_lanSession.SendMove(src, clickedIdx)) {
                                SetNetworkStatus(_T("状态：红方走子已执行，但发送失败"));
                            }
                            Invalidate(FALSE);
                        }
                    }
                    else {
                        // 客机只做无副作用预检；必须等房主回传确认后才修改棋盘。
                        if (Engine_IsLegalMove(src, clickedIdx)) {
                            if (m_lanSession.SendMove(src, clickedIdx)) {
                                m_waitingForGuestConfirmation = true;
                                m_pendingSrc = src;
                                m_pendingDst = clickedIdx;
                                SetNetworkStatus(_T("状态：走子已发送，等待房主确认..."));
                            }
                            else {
                                SetNetworkStatus(_T("状态：走子发送失败"));
                            }
                            Invalidate(FALSE);
                        }
                    }
                    // 非法走子同样要清除刚刚取消的选中框。
                    Invalidate(FALSE);
                }
            }
        }
    }

    CDialogEx::OnLButtonDown(nFlags, point);
}

void CanimalChessDlg::OnDestroy()
{
    m_lanSession.Stop();
    CDialogEx::OnDestroy();
}
