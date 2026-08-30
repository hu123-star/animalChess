#include "pch.h"
#include "framework.h"
#include "animalChess.h"
#include "animalChessDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CanimalChessDlg::CanimalChessDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_ANIMALCHESS_DIALOG, pParent)
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
END_MESSAGE_MAP()

BOOL CanimalChessDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    SetWindowPos(NULL, 0, 0, 1020, 670, SWP_NOMOVE | SWP_NOZORDER);

    Engine_Startup();
    m_strStatus = _T("【游戏开始】红方(玩家)走棋。请点击左侧己方棋子开始！");

    return TRUE;
}

void CanimalChessDlg::DrawMoveArrow(CDC* pDC, CPoint ptStart, CPoint ptEnd)
{
    if (ptStart == ptEnd) return;

    CPen pen(PS_SOLID, 3, RGB(255, 69, 0));
    CPen* pOldPen = pDC->SelectObject(&pen);
    CBrush brush(RGB(255, 69, 0));
    CBrush* pOldBrush = pDC->SelectObject(&brush);

    pDC->Ellipse(ptStart.x - 6, ptStart.y - 6, ptStart.x + 6, ptStart.y + 6);

    pDC->MoveTo(ptStart);
    pDC->LineTo(ptEnd);

    double angle = atan2((double)(ptEnd.y - ptStart.y), (double)(ptEnd.x - ptStart.x));
    double arrowLen = 14.0;
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

    const int GRID_SIZE = 65;
    const int OFFSET_X = 30;
    const int OFFSET_Y = 40;

    for (int y = BOARD_RANK_TOP; y <= BOARD_RANK_BOTTOM; y++) {
        for (int x = BOARD_FILE_LEFT; x <= BOARD_FILE_RIGHT; x++) {
            int sq = Engine_CoordToSq(x, y);
            int px = OFFSET_X + (x - BOARD_FILE_LEFT) * GRID_SIZE;
            int py = OFFSET_Y + (y - BOARD_RANK_TOP) * GRID_SIZE;
            CRect rc(px, py, px + GRID_SIZE, py + GRID_SIZE);

            TerrainType t = Engine_GetTerrain(sq);
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

            int pc = Engine_GetPiece(sq);
            if (pc > 0) {
                CRect pieceRc = rc;
                pieceRc.DeflateRect(6, 6);

                CBrush pBrush(pc < 16 ? RGB(255, 240, 240) : RGB(240, 245, 255));
                CPen pPen(PS_SOLID, 2, pc < 16 ? RGB(200, 30, 30) : RGB(30, 90, 200));
                CPen* pOld = memDC.SelectObject(&pPen);
                CBrush* pOldB = memDC.SelectObject(&pBrush);

                memDC.Ellipse(&pieceRc);

                memDC.SetBkMode(TRANSPARENT);
                memDC.SetTextColor(pc < 16 ? RGB(180, 0, 0) : RGB(0, 50, 180));

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
    }

    int selSq = Engine_GetSelectedSq();
    if (selSq != 0) {
        int sx = Engine_SqToFileX(selSq) - BOARD_FILE_LEFT;
        int sy = Engine_SqToRankY(selSq) - BOARD_RANK_TOP;
        CRect selRc(OFFSET_X + sx * GRID_SIZE, OFFSET_Y + sy * GRID_SIZE,
            OFFSET_X + (sx + 1) * GRID_SIZE, OFFSET_Y + (sy + 1) * GRID_SIZE);

        CPen goldPen(PS_SOLID, 4, RGB(255, 190, 0));
        CPen* pOldPen = memDC.SelectObject(&goldPen);
        memDC.SelectStockObject(NULL_BRUSH);
        memDC.Rectangle(&selRc);
        memDC.SelectObject(pOldPen);
    }

    int lastMv = Engine_GetLastMove();
    if (lastMv != 0) {
        int src = lastMv & 255;
        int dst = lastMv >> 8;
        int sx = Engine_SqToFileX(src) - BOARD_FILE_LEFT;
        int sy = Engine_SqToRankY(src) - BOARD_RANK_TOP;
        int dx = Engine_SqToFileX(dst) - BOARD_FILE_LEFT;
        int dy = Engine_SqToRankY(dst) - BOARD_RANK_TOP;

        CPoint ptStart(OFFSET_X + sx * GRID_SIZE + GRID_SIZE / 2, OFFSET_Y + sy * GRID_SIZE + GRID_SIZE / 2);
        CPoint ptEnd(OFFSET_X + dx * GRID_SIZE + GRID_SIZE / 2, OFFSET_Y + dy * GRID_SIZE + GRID_SIZE / 2);

        DrawMoveArrow(&memDC, ptStart, ptEnd);
    }

    int panelLeft = OFFSET_X + 9 * GRID_SIZE + 25;
    int panelTop = OFFSET_Y;
    int panelRight = clientRc.right - 25;
    int panelBottom = OFFSET_Y + 7 * GRID_SIZE;

    CRect infoRc(panelLeft, panelTop, panelRight, panelBottom);
    memDC.FillSolidRect(&infoRc, RGB(255, 255, 255));
    memDC.DrawEdge(&infoRc, EDGE_SUNKEN, BF_RECT);

    CRect headerRc(panelLeft, panelTop, panelRight, panelTop + 40);
    memDC.FillSolidRect(&headerRc, RGB(40, 70, 120));

    CFont headerFont;
    headerFont.CreatePointFont(120, _T("微软雅黑"));
    CFont* pOldF = memDC.SelectObject(&headerFont);
    memDC.SetTextColor(RGB(255, 255, 255));
    memDC.SetBkMode(TRANSPARENT);
    memDC.DrawText(_T("人机对战"), &headerRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int btnWidth = (panelRight - panelLeft - 50) / 4;
    int btnTop = panelTop + 48;
    int btnBottom = btnTop + 28;

    m_rcBtnEasy = CRect(panelLeft + 10, btnTop, panelLeft + 10 + btnWidth, btnBottom);
    m_rcBtnMedium = CRect(panelLeft + 20 + btnWidth, btnTop, panelLeft + 20 + btnWidth * 2, btnBottom);
    m_rcBtnHard = CRect(panelLeft + 30 + btnWidth * 2, btnTop, panelLeft + 30 + btnWidth * 3, btnBottom);
    m_rcBtnReset = CRect(panelLeft + 40 + btnWidth * 3, btnTop, panelRight - 10, btnBottom);

    DifficultyLevel curDiff = Engine_GetDifficulty();

    CFont btnFont;
    btnFont.CreatePointFont(95, _T("微软雅黑"));
    memDC.SelectObject(&btnFont);

    auto DrawDiffBtn = [&](CRect rc, CString text, bool active, COLORREF actBg) {
        if (active) {
            memDC.FillSolidRect(&rc, actBg);
            memDC.SetTextColor(RGB(255, 255, 255));
        }
        else {
            memDC.FillSolidRect(&rc, RGB(240, 242, 245));
            memDC.SetTextColor(RGB(60, 60, 60));
        }
        memDC.DrawEdge(&rc, EDGE_RAISED, BF_RECT);
        memDC.DrawText(text, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        };

    DrawDiffBtn(m_rcBtnEasy, _T("简单"), curDiff == DIFF_EASY, RGB(46, 139, 87));
    DrawDiffBtn(m_rcBtnMedium, _T("中等"), curDiff == DIFF_MEDIUM, RGB(218, 140, 16));
    DrawDiffBtn(m_rcBtnHard, _T("困难"), curDiff == DIFF_HARD, RGB(178, 34, 34));
    DrawDiffBtn(m_rcBtnReset, _T("重开"), false, RGB(70, 130, 180));

    CRect turnRc(panelLeft + 15, panelTop + 84, panelRight - 15, panelTop + 112);
    bool isRedTurn = (Engine_GetTurn() == 0);
    memDC.FillSolidRect(&turnRc, isRedTurn ? RGB(255, 235, 235) : RGB(235, 245, 255));
    memDC.DrawEdge(&turnRc, EDGE_ETCHED, BF_RECT);

    CFont turnFont;
    turnFont.CreatePointFont(100, _T("微软雅黑"));
    memDC.SelectObject(&turnFont);
    memDC.SetTextColor(isRedTurn ? RGB(180, 0, 0) : RGB(0, 60, 180));
    CString strTurn = isRedTurn ? _T("【当前回合】红方") : _T("【当前回合】蓝方");
    memDC.DrawText(strTurn, &turnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    CRect textRc(panelLeft + 18, panelTop + 120, panelRight - 18, panelBottom - 80);
    CFont textFont;
    textFont.CreatePointFont(100, _T("微软雅黑"));
    memDC.SelectObject(&textFont);
    memDC.SetTextColor(RGB(40, 40, 40));
    memDC.DrawText(m_strStatus, &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK);

    CRect tipRc(panelLeft + 12, panelBottom - 110, panelRight - 12, panelBottom - 8);
    memDC.FillSolidRect(&tipRc, RGB(248, 249, 250));
    memDC.DrawEdge(&tipRc, EDGE_ETCHED, BF_RECT);

    CFont tipFont;
    tipFont.CreatePointFont(90, _T("微软雅黑"));
    memDC.SelectObject(&tipFont);
    memDC.SetTextColor(RGB(100, 100, 100));
    CString strRuleTip = _T("吃法：象狮虎豹狼狗猫鼠象\n特殊：鼠可游水,狮虎跳过河,水陆互不吃,陷阱全削弱");
    CRect tipTextRc = tipRc;
    tipTextRc.DeflateRect(8, 5);
    memDC.DrawText(strRuleTip, &tipTextRc, DT_LEFT | DT_TOP | DT_WORDBREAK);

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
    if (m_rcBtnEasy.PtInRect(point)) {
        Engine_SetDifficulty(DIFF_EASY);
        m_strStatus = _T("【难度已切换】当前 AI 难度：简单 (适合新手对弈)");
        Invalidate(FALSE);
        return;
    }
    if (m_rcBtnMedium.PtInRect(point)) {
        Engine_SetDifficulty(DIFF_MEDIUM);
        m_strStatus = _T("【难度已切换】当前 AI 难度：中等 (攻守平衡)");
        Invalidate(FALSE);
        return;
    }
    if (m_rcBtnHard.PtInRect(point)) {
        Engine_SetDifficulty(DIFF_HARD);
        m_strStatus = _T("【难度已切换】当前 AI 难度：困难 (全算力应战)");
        Invalidate(FALSE);
        return;
    }
    if (m_rcBtnReset.PtInRect(point)) {
        Engine_Startup();
        m_strStatus = _T("【棋局已重置】红方(玩家)先行，请落子。");
        Invalidate(FALSE);
        return;
    }

    const int GRID_SIZE = 65;
    const int OFFSET_X = 30;
    const int OFFSET_Y = 40;

    int fileX = (point.x - OFFSET_X) / GRID_SIZE + BOARD_FILE_LEFT;
    int rankY = (point.y - OFFSET_Y) / GRID_SIZE + BOARD_RANK_TOP;

    if (fileX >= BOARD_FILE_LEFT && fileX <= BOARD_FILE_RIGHT &&
        rankY >= BOARD_RANK_TOP && rankY <= BOARD_RANK_BOTTOM)
    {
        int sq = Engine_CoordToSq(fileX, rankY);
        std::string msg;
        MoveResult res = Engine_ClickSquare(sq, msg);

        m_strStatus = CA2T(msg.c_str());

        //强制先渲染玩家走步。
        Invalidate(FALSE);
        UpdateWindow();

        if (res == MOVE_RES_MOVED && Engine_GetTurn() == 1 && !Engine_IsGameOver())
        {
            int aiMv = 0;
            std::string aiMsg;
            Engine_AiMove(aiMv, aiMsg);

            m_strStatus += _T("\n\n");
            m_strStatus += CA2T(aiMsg.c_str());

            Invalidate(FALSE);
            UpdateWindow();
        }
    }

    CDialogEx::OnLButtonDown(nFlags, point);
}