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
    , m_selectedIdx(-1)
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

    SetWindowPos(NULL, 0, 0, 680, 560, SWP_NOMOVE | SWP_NOZORDER);

    Engine_Startup();

    return TRUE;
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
    CRect statusRc(OFFSET_X, 8, clientRc.right - OFFSET_X, 32);
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(RGB(60, 60, 60));
    CFont statusFont;
    statusFont.CreatePointFont(105, _T("微软雅黑"));
    CFont* pOldF = memDC.SelectObject(&statusFont);

    CString strInfo;
    if (snap.gameStatus == 1) strInfo = _T("【对局结束】红方取得胜利！");
    else if (snap.gameStatus == 2) strInfo = _T("【对局结束】蓝方取得胜利！");
    else if (snap.gameStatus == 3) strInfo = _T("【对局结束】双方和棋！");
    else strInfo = (snap.currentTurn == 0) ? _T("【当前回合】玩家 (红方)") : _T("【当前回合】电脑 (蓝方) 思考中...");

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

    int col = (point.x - OFFSET_X) / GRID_SIZE;
    int row = (point.y - OFFSET_Y) / GRID_SIZE;

    if (col >= 0 && col < BOARD_COLS && row >= 0 && row < BOARD_ROWS)
    {
        uint8_t clickedIdx = static_cast<uint8_t>(row * BOARD_COLS + col);

        MsgBoardSnapshot snap;
        Engine_GetSnapshot(snap);

        if (snap.gameStatus == 0)
        {
            if (m_selectedIdx == -1) {
                //未选中时，点击己方棋子则高亮。
                if (snap.board[clickedIdx] >= 1 && snap.board[clickedIdx] <= 8) {
                    m_selectedIdx = clickedIdx;
                    Invalidate(FALSE);
                }
            }
            else {
                if (snap.board[clickedIdx] >= 1 && snap.board[clickedIdx] <= 8) {
                    //改选其他己方棋子。
                    m_selectedIdx = clickedIdx;
                    Invalidate(FALSE);
                }
                else {
                    //提交走子请求。
                    bool moved = Engine_TryMove(static_cast<uint8_t>(m_selectedIdx), clickedIdx);
                    m_selectedIdx = -1;

                    if (moved) {
                        Invalidate(FALSE);
                        UpdateWindow();

                        //玩家走完后触发AI。
                        Engine_TriggerAi();
                        Invalidate(FALSE);
                    }
                }
            }
        }
    }

    CDialogEx::OnLButtonDown(nFlags, point);
}