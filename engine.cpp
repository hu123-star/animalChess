#include "pch.h"
#include "engine.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>

static const char g_szNames[25][5] = {
    "　",
    "象", "狮", "虎", "豹", "狼", "狗", "猫", "鼠",
    "　", "　", "　", "　", "　", "　", "　", "　",
    "象", "狮", "虎", "豹", "狼", "狗", "猫", "鼠"
};

static const int g_basePieceVal[8] = { 1000, 800, 700, 450, 300, 200, 150, 100 };
static const int DELTA[4] = { -16, -1, 1, 16 };
static const int JUMP_DELTA[4] = { -48, -4, 4, 48 };

static const char g_fortTerrain[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,3,3,3,0,0,0,0,0,0,0,
  0,0,0,4,0,0,3,3,3,0,0,2,0,0,0,0,
  0,0,0,1,4,0,0,0,0,0,2,5,0,0,0,0,
  0,0,0,4,0,0,3,3,3,0,0,2,0,0,0,0,
  0,0,0,0,0,0,3,3,3,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const int g_startupPieces[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0, 3, 0, 1, 0, 0, 0,24, 0,18,0,0,0,0,
  0,0,0, 0, 7, 0, 0, 0, 0, 0,22, 0,0,0,0,0,
  0,0,0, 0, 0, 5, 0, 0, 0,20, 0, 0,0,0,0,0,
  0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,
  0,0,0, 0, 0, 4, 0, 0, 0,21, 0, 0,0,0,0,0,
  0,0,0, 0, 6, 0, 0, 0, 0, 0,23, 0,0,0,0,0,
  0,0,0, 2, 0, 8, 0, 0, 0,17, 0,19,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const bool g_jumpable[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,
  0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,
  0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,
  0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,
  0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,
  0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,
  0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static inline int SRC(int mv) { return mv & 255; }
static inline int DST(int mv) { return mv >> 8; }
static inline int MOVE(int src, int dst) { return src | (dst << 8); }
static inline int PIECE_TYPE(int pc) { return (pc <= 8) ? (pc - 1) : (pc - 17); }
static inline bool IS_RED(int pc) { return pc >= 1 && pc <= 8; }
static inline bool IS_BLUE(int pc) { return pc >= 17 && pc <= 24; }
static inline bool IN_RIVER(int sq) { return g_fortTerrain[sq] == 3; }
static inline bool IN_DEN(int sq, int side) { return side == 0 ? (g_fortTerrain[sq] == 1) : (g_fortTerrain[sq] == 5); }
static inline bool IN_TRAP(int sq, int side) { return side == 0 ? (g_fortTerrain[sq] == 2) : (g_fortTerrain[sq] == 4); }

int JungleBoard::IndexToSq(uint8_t idx) {
    if (idx >= BOARD_CELL_COUNT) return 0;
    int row = idx / BOARD_COLS;
    int col = idx % BOARD_COLS;
    return CoordToSq(col + BOARD_FILE_LEFT, row + BOARD_RANK_TOP);
}

uint8_t JungleBoard::SqToIndex(int sq) {
    int x = SqToFileX(sq);
    int y = SqToRankY(sq);
    if (x < BOARD_FILE_LEFT || x > BOARD_FILE_RIGHT || y < BOARD_RANK_TOP || y > BOARD_RANK_BOTTOM) return 255;
    return static_cast<uint8_t>((y - BOARD_RANK_TOP) * BOARD_COLS + (x - BOARD_FILE_LEFT));
}

JungleBoard::JungleBoard() {
    Reset();
}

bool JungleBoard::InBoard(int sq) {
    int x = SqToFileX(sq), y = SqToRankY(sq);
    return (x >= BOARD_FILE_LEFT && x <= BOARD_FILE_RIGHT &&
        y >= BOARD_RANK_TOP && y <= BOARD_RANK_BOTTOM);
}

TerrainType JungleBoard::GetTerrain(int sq) {
    if (sq < 0 || sq >= 256) return TERRAIN_LAND;
    return static_cast<TerrainType>(g_fortTerrain[sq]);
}

void JungleBoard::AddPiece(int sq, int piece) {
    m_squares[sq] = piece;
}

void JungleBoard::DelPiece(int sq, int piece) {
    m_squares[sq] = 0;
}

void JungleBoard::Reset() {
    m_squares.fill(0);
    m_side = 0;
    m_history.clear();

    for (int sq = 0; sq < 256; sq++) {
        if (InBoard(sq) && g_startupPieces[sq] != 0) {
            AddPiece(sq, g_startupPieces[sq]);
        }
    }
}

int JungleBoard::GetPiece(int sq) const {
    if (sq < 0 || sq >= 256) return 0;
    return m_squares[sq];
}

bool JungleBoard::CanJump(int src, int dst) const {
    int pc = PIECE_TYPE(m_squares[src]);
    if (pc != 1 && pc != 2) return false;
    if (!g_jumpable[src] || !g_jumpable[dst]) return false;

    for (int i = 0; i < 4; i++) {
        if (dst - src == JUMP_DELTA[i]) {
            for (int j = src + DELTA[i]; j != dst && InBoard(j); j += DELTA[i]) {
                int p = m_squares[j];
                //河道中有鼠则无法越过。
                if (p != 0 && PIECE_TYPE(p) == 7) return false;
                if (!IN_RIVER(j)) return false;
            }
            return true;
        }
    }
    return false;
}

bool JungleBoard::CanMove(int src, int dst) const {
    int pc = PIECE_TYPE(m_squares[src]);
    if (pc == 7) {
        for (int i = 0; i < 4; i++) {
            if (dst - src == DELTA[i]) return true;
        }
        return false;
    }
    if (IN_RIVER(dst)) return false;
    for (int i = 0; i < 4; i++) {
        if (dst - src == DELTA[i]) return true;
    }
    return false;
}

bool JungleBoard::CanEat(int src, int dst) const {
    //水陆互不相吃。
    if (IN_RIVER(src) != IN_RIVER(dst)) return false;

    int as = PIECE_TYPE(m_squares[src]);
    int bs = PIECE_TYPE(m_squares[dst]);

    int oppSide = IS_RED(m_squares[dst]) ? 0 : 1;
    if (IN_TRAP(dst, oppSide)) return true;

    if (as == 7 && bs == 0) return true;
    if (as == 0 && bs == 7) return false;

    return as <= bs;
}

bool JungleBoard::IsLegalMove(int mv) const {
    int src = SRC(mv), dst = DST(mv);
    if (!InBoard(src) || !InBoard(dst)) return false;

    int pcSrc = m_squares[src];
    int pcDst = m_squares[dst];

    if (m_side == 0 && !IS_RED(pcSrc)) return false;
    if (m_side == 1 && !IS_BLUE(pcSrc)) return false;

    if (m_side == 0 && IS_RED(pcDst)) return false;
    if (m_side == 1 && IS_BLUE(pcDst)) return false;

    if (IN_DEN(dst, m_side)) return false;

    for (int i = 0; i < 4; i++) {
        if (src + DELTA[i] == dst) {
            if (!CanMove(src, dst)) return false;
            if (pcDst == 0) return true;
            return CanEat(src, dst);
        }
    }

    for (int i = 0; i < 4; i++) {
        if (src + JUMP_DELTA[i] == dst) {
            if (!CanJump(src, dst)) return false;
            if (pcDst == 0) return true;
            return CanEat(src, dst);
        }
    }

    return false;
}

int JungleBoard::GenerateMoves(std::vector<int>& outMoves, bool capturesOnly) const {
    outMoves.clear();

    for (int src = 0; src < 256; src++) {
        if (!InBoard(src)) continue;
        int pcSrc = m_squares[src];
        if (m_side == 0 && !IS_RED(pcSrc)) continue;
        if (m_side == 1 && !IS_BLUE(pcSrc)) continue;

        for (int i = 0; i < 4; i++) {
            int dst = src + DELTA[i];
            if (!InBoard(dst) || IN_DEN(dst, m_side)) continue;
            int pcDst = m_squares[dst];
            if (!CanMove(src, dst)) continue;
            if (pcDst == 0) {
                if (!capturesOnly) outMoves.push_back(MOVE(src, dst));
            }
            else if ((m_side == 0 ? IS_BLUE(pcDst) : IS_RED(pcDst)) && CanEat(src, dst)) {
                outMoves.push_back(MOVE(src, dst));
            }
        }

        for (int i = 0; i < 4; i++) {
            int dst = src + JUMP_DELTA[i];
            if (!InBoard(dst) || IN_DEN(dst, m_side)) continue;
            int pcDst = m_squares[dst];
            if (!CanJump(src, dst)) continue;
            if (pcDst == 0) {
                if (!capturesOnly) outMoves.push_back(MOVE(src, dst));
            }
            else if ((m_side == 0 ? IS_BLUE(pcDst) : IS_RED(pcDst)) && CanEat(src, dst)) {
                outMoves.push_back(MOVE(src, dst));
            }
        }
    }
    return static_cast<int>(outMoves.size());
}

bool JungleBoard::MakeMove(int mv) {
    int src = SRC(mv), dst = DST(mv);
    int captured = m_squares[dst];
    int moving = m_squares[src];

    if (captured != 0) DelPiece(dst, captured);
    DelPiece(src, moving);
    AddPiece(dst, moving);

    m_history.push_back({ mv, captured, moving });
    m_side = 1 - m_side;
    return true;
}

void JungleBoard::UndoMove() {
    if (m_history.empty()) return;
    auto last = m_history.back();
    m_history.pop_back();

    int src = SRC(last.moveVal), dst = DST(last.moveVal);
    DelPiece(dst, last.movedPiece);
    AddPiece(src, last.movedPiece);
    if (last.capturedPiece != 0) {
        AddPiece(dst, last.capturedPiece);
    }

    m_side = 1 - m_side;
}

bool JungleBoard::IsMate() const {
    if (IS_BLUE(m_squares[99])) return true;
    if (IS_RED(m_squares[107])) return true;
    return false;
}

bool JungleBoard::IsRepetition() const {
    if (m_history.size() < 16) return false;
    int len = static_cast<int>(m_history.size());
    for (int i = len - 1; i >= len - 8; i--) {
        if (m_history[i].capturedPiece != 0) return false;
    }
    int targetMv = m_history.back().moveVal;
    int count = 0;
    for (int i = len - 1; i >= 0 && i >= len - 24; i -= 2) {
        if (m_history[i].moveVal == targetMv) count++;
    }
    return count >= 5;
}

bool JungleBoard::IsGameOver() const {
    return IsMate() || IsRepetition();
}

int JungleEvaluator::Evaluate(const JungleBoard& board) {
    int redScore = 0, blueScore = 0;
    for (int sq = 0; sq < 256; sq++) {
        if (!JungleBoard::InBoard(sq)) continue;
        int pc = board.GetPiece(sq);
        if (pc == 0) continue;

        int type = PIECE_TYPE(pc);
        int val = g_basePieceVal[type];
        int x = JungleBoard::SqToFileX(sq);
        int y = JungleBoard::SqToRankY(sq);

        if (IS_RED(pc)) {
            int dist = std::abs(x - 11) + std::abs(y - 6);
            redScore += val + (14 - dist) * 8;
            if (type == 7 && IN_RIVER(sq)) redScore += 40;
            if (IN_TRAP(sq, 0)) redScore -= 300;
        }
        else {
            int dist = std::abs(x - 3) + std::abs(y - 6);
            blueScore += val + (14 - dist) * 8;
            if (type == 7 && IN_RIVER(sq)) blueScore += 40;
            if (IN_TRAP(sq, 1)) blueScore -= 300;
        }
    }
    return (board.GetSide() == 0) ? (redScore - blueScore) : (blueScore - redScore);
}

AlphaBetaSearcher::AlphaBetaSearcher() {
    m_historyTable.fill(0);
}

int AlphaBetaSearcher::QuiescenceSearch(JungleBoard& board, int alpha, int beta, int depth) {
    if (board.IsMate()) return -99999;
    int val = JungleEvaluator::Evaluate(board);
    if (val >= beta) return beta;
    if (val > alpha) alpha = val;
    if (depth <= 0) return val;

    std::vector<int> moves;
    board.GenerateMoves(moves, true);

    for (int mv : moves) {
        board.MakeMove(mv);
        int score = -QuiescenceSearch(board, -beta, -alpha, depth - 1);
        board.UndoMove();

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

int AlphaBetaSearcher::AlphaBeta(JungleBoard& board, int depth, int alpha, int beta, bool isRoot) {
    if (board.IsMate()) return -99999 + (10 - depth);
    if (depth <= 0) return QuiescenceSearch(board, alpha, beta, 4);

    std::vector<int> moves;
    board.GenerateMoves(moves);
    if (moves.empty()) return -99999;

    std::sort(moves.begin(), moves.end(), [&](int a, int b) {
        return m_historyTable[a] > m_historyTable[b];
        });

    int bestMove = moves[0];
    int bestScore = -1000000;

    for (int mv : moves) {
        board.MakeMove(mv);
        if (board.IsRepetition()) {
            board.UndoMove();
            continue;
        }

        int score = -AlphaBeta(board, depth - 1, -beta, -alpha, false);
        board.UndoMove();

        if (score > bestScore) {
            bestScore = score;
            bestMove = mv;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            m_historyTable[mv] += depth * depth;
            break;
        }
    }

    return isRoot ? bestMove : bestScore;
}

int AlphaBetaSearcher::SearchBestMove(JungleBoard& board, int depth) {
    std::vector<int> moves;
    board.GenerateMoves(moves);
    if (moves.empty()) return 0;
    if (moves.size() == 1) return moves[0];

    return AlphaBeta(board, depth, -999999, 999999, true);
}

static JungleBoard g_board;
static AlphaBetaSearcher g_searcher;
static int g_lastMove = 0;
static uint8_t g_gameStatus = 0;

void Engine_Startup() {
    g_board.Reset();
    g_lastMove = 0;
    g_gameStatus = 0;
}

void Engine_GetSnapshot(MsgBoardSnapshot& outSnapshot) {
    outSnapshot.gameStatus = g_gameStatus;
    outSnapshot.currentTurn = static_cast<uint8_t>(g_board.GetSide());

    if (g_lastMove != 0) {
        outSnapshot.lastSrc = JungleBoard::SqToIndex(SRC(g_lastMove));
        outSnapshot.lastDst = JungleBoard::SqToIndex(DST(g_lastMove));
    }
    else {
        outSnapshot.lastSrc = 255;
        outSnapshot.lastDst = 255;
    }

    for (uint8_t i = 0; i < BOARD_CELL_COUNT; i++) {
        int sq = JungleBoard::IndexToSq(i);
        outSnapshot.board[i] = static_cast<uint8_t>(g_board.GetPiece(sq));
    }
}

bool Engine_TryMove(uint8_t srcIdx, uint8_t dstIdx) {
    if (g_gameStatus != 0) return false;

    int src = JungleBoard::IndexToSq(srcIdx);
    int dst = JungleBoard::IndexToSq(dstIdx);
    int mv = MOVE(src, dst);

    if (!g_board.IsLegalMove(mv)) return false;

    g_board.MakeMove(mv);
    g_lastMove = mv;

    if (g_board.IsMate()) {
        g_gameStatus = 1;
    }
    else if (g_board.IsRepetition()) {
        g_gameStatus = 3;
    }

    return true;
}

bool Engine_TriggerAi() {
    if (g_gameStatus != 0 || g_board.GetSide() != 1) return false;

    int mv = g_searcher.SearchBestMove(g_board, 4);
    if (mv == 0) {
        g_gameStatus = 1;
        return true;
    }

    g_lastMove = mv;
    g_board.MakeMove(mv);

    if (g_board.IsMate()) {
        g_gameStatus = 2;
    }
    else if (g_board.IsRepetition()) {
        g_gameStatus = 3;
    }

    return true;
}

TerrainType Engine_GetTerrainByIndex(uint8_t idx) {
    int sq = JungleBoard::IndexToSq(idx);
    return JungleBoard::GetTerrain(sq);
}

const char* Engine_GetPieceName(uint8_t pc) {
    if (pc >= 25) return "　";
    return g_szNames[pc];
}