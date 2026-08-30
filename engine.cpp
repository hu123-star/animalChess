#include "pch.h"
#include "engine.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>

static const char g_szNames[24][5] = {
    "　", "穴", "阱", "■", "阱", "穴", "　", "　",
    "象", "狮", "虎", "豹", "狼", "狗", "猫", "鼠",
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
  0,0,0,10, 0, 8, 0, 0, 0,23, 0,17,0,0,0,0,
  0,0,0, 0,14, 0, 0, 0, 0, 0,21, 0,0,0,0,0,
  0,0,0, 0, 0,12, 0, 0, 0,19, 0, 0,0,0,0,0,
  0,0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,
  0,0,0, 0, 0,11, 0, 0, 0,20, 0, 0,0,0,0,0,
  0,0,0, 0,13, 0, 0, 0, 0, 0,22, 0,0,0,0,0,
  0,0,0, 9, 0,15, 0, 0, 0,16, 0,18,0,0,0,0,
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
static inline int PIECE_TYPE(int pc) { return pc & 7; }
static inline int SIDE_TAG(int sd) { return 8 + (sd << 3); }
static inline int OPP_SIDE_TAG(int sd) { return 16 - (sd << 3); }
static inline bool IN_RIVER(int sq) { return g_fortTerrain[sq] == 3; }
static inline bool IN_DEN(int sq, int side) { return side == 0 ? (g_fortTerrain[sq] == 1) : (g_fortTerrain[sq] == 5); }
static inline bool IN_TRAP(int sq, int side) { return side == 0 ? (g_fortTerrain[sq] == 2) : (g_fortTerrain[sq] == 4); }

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
    if (pc != ANIMAL_LION && pc != ANIMAL_TIGER) return false;
    if (!g_jumpable[src] || !g_jumpable[dst]) return false;

    for (int i = 0; i < 4; i++) {
        if (dst - src == JUMP_DELTA[i]) {
            for (int j = src + DELTA[i]; j != dst && InBoard(j); j += DELTA[i]) {
                int pieceInWater = m_squares[j];
                //河道内有任何鼠则无法跳跃。
                if (pieceInWater != 0 && PIECE_TYPE(pieceInWater) == ANIMAL_MOUSE) return false;
                if (!IN_RIVER(j)) return false;
            }
            return true;
        }
    }
    return false;
}

bool JungleBoard::CanMove(int src, int dst) const {
    int pc = PIECE_TYPE(m_squares[src]);
    if (pc == ANIMAL_MOUSE) {
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

    int oppSide = (m_squares[dst] < 16) ? 0 : 1;
    if (IN_TRAP(dst, oppSide)) return true;

    if (as == ANIMAL_MOUSE && bs == ANIMAL_ELEPHANT) return true;
    if (as == ANIMAL_ELEPHANT && bs == ANIMAL_MOUSE) return false;

    return as <= bs;
}

bool JungleBoard::IsLegalMove(int mv) const {
    int src = SRC(mv), dst = DST(mv);
    if (!InBoard(src) || !InBoard(dst)) return false;

    int pcSrc = m_squares[src];
    int pcDst = m_squares[dst];
    int selfSideTag = SIDE_TAG(m_side);
    int oppSideTag = OPP_SIDE_TAG(m_side);

    if ((pcSrc & selfSideTag) == 0) return false;
    if ((pcDst & selfSideTag) != 0) return false;
    if (IN_DEN(dst, m_side)) return false;

    for (int i = 0; i < 4; i++) {
        if (src + DELTA[i] == dst) {
            if (!CanMove(src, dst)) return false;
            if (pcDst == 0) return true;
            return ((pcDst & oppSideTag) != 0 && CanEat(src, dst));
        }
    }

    for (int i = 0; i < 4; i++) {
        if (src + JUMP_DELTA[i] == dst) {
            if (!CanJump(src, dst)) return false;
            if (pcDst == 0) return true;
            return ((pcDst & oppSideTag) != 0 && CanEat(src, dst));
        }
    }

    return false;
}

int JungleBoard::GenerateMoves(std::vector<int>& outMoves, bool capturesOnly) const {
    outMoves.clear();
    int selfSideTag = SIDE_TAG(m_side);
    int oppSideTag = OPP_SIDE_TAG(m_side);

    for (int src = 0; src < 256; src++) {
        if (!InBoard(src)) continue;
        int pcSrc = m_squares[src];
        if ((pcSrc & selfSideTag) == 0) continue;

        for (int i = 0; i < 4; i++) {
            int dst = src + DELTA[i];
            if (!InBoard(dst) || IN_DEN(dst, m_side)) continue;
            int pcDst = m_squares[dst];
            if (!CanMove(src, dst)) continue;
            if (pcDst == 0) {
                if (!capturesOnly) outMoves.push_back(MOVE(src, dst));
            }
            else if ((pcDst & oppSideTag) != 0 && CanEat(src, dst)) {
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
            else if ((pcDst & oppSideTag) != 0 && CanEat(src, dst)) {
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
    if (m_squares[99] >= 16 && m_squares[99] <= 23) return true;
    if (m_squares[107] >= 8 && m_squares[107] <= 15) return true;
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

int JungleEvaluator::EvaluateMaterial(const JungleBoard& board) {
    int redScore = 0, blueScore = 0;
    for (int sq = 0; sq < 256; sq++) {
        if (!JungleBoard::InBoard(sq)) continue;
        int pc = board.GetPiece(sq);
        if (pc == 0) continue;
        int type = PIECE_TYPE(pc);
        int val = g_basePieceVal[type];
        if (pc < 16) redScore += val;
        else blueScore += val;
    }
    return (board.GetSide() == 0) ? (redScore - blueScore) : (blueScore - redScore);
}

int JungleEvaluator::EvaluateDenProximity(const JungleBoard& board) {
    int redScore = 0, blueScore = 0;
    for (int sq = 0; sq < 256; sq++) {
        if (!JungleBoard::InBoard(sq)) continue;
        int pc = board.GetPiece(sq);
        if (pc == 0) continue;

        int x = JungleBoard::SqToFileX(sq);
        int y = JungleBoard::SqToRankY(sq);

        if (pc < 16) {
            int dist = std::abs(x - 11) + std::abs(y - 6);
            redScore += (14 - dist) * 8;
        }
        else {
            int dist = std::abs(x - 3) + std::abs(y - 6);
            blueScore += (14 - dist) * 8;
        }
    }
    return (board.GetSide() == 0) ? (redScore - blueScore) : (blueScore - redScore);
}

int JungleEvaluator::EvaluateRiverControl(const JungleBoard& board) {
    int redScore = 0, blueScore = 0;
    for (int sq = 0; sq < 256; sq++) {
        if (!JungleBoard::InBoard(sq)) continue;
        int pc = board.GetPiece(sq);
        if (pc == 0) continue;

        int type = PIECE_TYPE(pc);
        bool inRiver = IN_RIVER(sq);

        if (type == ANIMAL_MOUSE && inRiver) {
            if (pc < 16) redScore += 45;
            else blueScore += 45;
        }

        if ((type == ANIMAL_LION || type == ANIMAL_TIGER) && g_jumpable[sq]) {
            if (pc < 16) redScore += 30;
            else blueScore += 30;
        }
    }
    return (board.GetSide() == 0) ? (redScore - blueScore) : (blueScore - redScore);
}

int JungleEvaluator::EvaluateTrapSafety(const JungleBoard& board) {
    int redScore = 0, blueScore = 0;
    for (int sq = 0; sq < 256; sq++) {
        if (!JungleBoard::InBoard(sq)) continue;
        int pc = board.GetPiece(sq);
        if (pc == 0) continue;

        if (pc < 16 && IN_TRAP(sq, 0)) redScore -= 300;
        if (pc >= 16 && IN_TRAP(sq, 1)) blueScore -= 300;
    }
    return (board.GetSide() == 0) ? (redScore - blueScore) : (blueScore - redScore);
}

int JungleEvaluator::Evaluate(const JungleBoard& board) {
    return EvaluateMaterial(board) + EvaluateDenProximity(board) + EvaluateRiverControl(board) + EvaluateTrapSafety(board);
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

int AlphaBetaSearcher::SearchBestMove(JungleBoard& board, DifficultyLevel level, int timeLimitMs) {
    std::vector<int> moves;
    board.GenerateMoves(moves);
    if (moves.empty()) return 0;
    if (moves.size() == 1) return moves[0];

    if (level == DIFF_EASY) {
        std::vector<std::pair<int, int>> scoredMoves;
        for (int mv : moves) {
            board.MakeMove(mv);
            int score = -AlphaBeta(board, 1, -999999, 999999, false);
            board.UndoMove();
            scoredMoves.push_back({ score, mv });
        }
        std::sort(scoredMoves.begin(), scoredMoves.end(), std::greater<std::pair<int, int>>());
        int pickCount = std::min<int>(3, static_cast<int>(scoredMoves.size()));
        return scoredMoves[rand() % pickCount].second;
    }

    if (level == DIFF_MEDIUM) {
        return AlphaBeta(board, 3, -999999, 999999, true);
    }

    //困难模式搜索深度为 5 层。
    return AlphaBeta(board, 5, -999999, 999999, true);
}

static JungleBoard g_board;
static AlphaBetaSearcher g_searcher;
static DifficultyLevel g_difficulty = DIFF_HARD;
static int g_selectedSq = 0;
static int g_lastMove = 0;
static bool g_gameOver = false;

static std::string ExplainIllegalReason(int src, int dst) {
    int pcSrc = g_board.GetPiece(src);
    int pcDst = g_board.GetPiece(dst);
    int as = PIECE_TYPE(pcSrc);

    if (IN_DEN(dst, g_board.GetSide())) return "【规则违规】动物禁止进入己方兽穴！";
    if (IN_RIVER(src) != IN_RIVER(dst)) {
        if (pcDst != 0) return "【规则违规】水陆隔绝：水中鼠不可吃岸上棋子，岸上棋子亦不可吃水中鼠！";
        if (as != ANIMAL_MOUSE) return "【规则违规】只有【鼠】可以下水游泳，其余动物均不能下水！";
    }

    if ((as == ANIMAL_LION || as == ANIMAL_TIGER) && g_jumpable[src] && g_jumpable[dst]) {
        for (int i = 0; i < 4; i++) {
            if (dst - src == JUMP_DELTA[i]) {
                for (int j = src + DELTA[i]; j != dst && JungleBoard::InBoard(j); j += DELTA[i]) {
                    if (g_board.GetPiece(j) != 0 && PIECE_TYPE(g_board.GetPiece(j)) == ANIMAL_MOUSE) {
                        return "【跳河失败】河道中间有【鼠】阻隔视线与跳跃路线，狮/虎无法跳河！";
                    }
                }
            }
        }
    }

    if (pcDst != 0) {
        int bs = PIECE_TYPE(pcDst);
        if (as == ANIMAL_ELEPHANT && bs == ANIMAL_MOUSE) return "【吃子失败】规则限制：庞大的【象】不能吃体型灵巧的【鼠】！";
        if (as > bs) {
            char buf[128];
            sprintf_s(buf, "【吃子失败】等级压制：【%s】无法吃掉更高等级的【%s】（等级：象>狮>虎>豹>狼>狗>猫>鼠>象）！",
                g_szNames[pcSrc], g_szNames[pcDst]);
            return std::string(buf);
        }
    }

    return "【规则违规】该位置不可达，每次只能向上下左右移动 1 格（狮虎可直线跳河）！";
}

void Engine_Startup() {
    g_board.Reset();
    g_selectedSq = 0;
    g_lastMove = 0;
    g_gameOver = false;
}

int Engine_GetTurn() { return g_board.GetSide(); }
int Engine_GetPiece(int sq) { return g_board.GetPiece(sq); }
TerrainType Engine_GetTerrain(int sq) { return JungleBoard::GetTerrain(sq); }
int Engine_GetSelectedSq() { return g_selectedSq; }
void Engine_ClearSelectedSq() { g_selectedSq = 0; }
int Engine_GetLastMove() { return g_lastMove; }
bool Engine_IsGameOver() { return g_gameOver; }

void Engine_SetDifficulty(DifficultyLevel level) { g_difficulty = level; }
DifficultyLevel Engine_GetDifficulty() { return g_difficulty; }

const char* Engine_GetPieceName(int pc) {
    if (pc < 0 || pc >= 24) return "　";
    return g_szNames[pc];
}

MoveResult Engine_ClickSquare(int sq, std::string& outMsg) {
    if (g_gameOver) {
        outMsg = "对局已结束，请重新开始！";
        return MOVE_RES_NONE;
    }

    int pc = g_board.GetPiece(sq);
    int selfSideTag = SIDE_TAG(g_board.GetSide());

    if ((pc & selfSideTag) != 0) {
        g_selectedSq = sq;
        outMsg = std::string("已选中【") + g_szNames[pc] + "】，请点击目标格子移动。";
        return MOVE_RES_SELECT;
    }

    if (g_selectedSq != 0) {
        int src = g_selectedSq, dst = sq;
        int mv = MOVE(src, dst);

        if (!g_board.IsLegalMove(mv)) {
            outMsg = ExplainIllegalReason(src, dst);
            return MOVE_RES_ILLEGAL;
        }

        int pcSrc = g_board.GetPiece(src);
        int pcDst = g_board.GetPiece(dst);

        g_board.MakeMove(mv);
        g_lastMove = mv;
        g_selectedSq = 0;

        char buf[256];
        if (pcDst != 0) {
            sprintf_s(buf, "【成功吃子】我方【%s】按照斗兽规则成功吃掉对方【%s】！", g_szNames[pcSrc], g_szNames[pcDst]);
        }
        else {
            sprintf_s(buf, "我方【%s】移动完成，轮到对方思考！", g_szNames[pcSrc]);
        }
        outMsg = buf;

        if (g_board.IsMate()) {
            g_gameOver = true;
            outMsg += " ★ 恭喜！你成功攻占了对方的兽穴，取得最终胜利！";
            return MOVE_RES_WIN;
        }
        if (g_board.IsRepetition()) {
            g_gameOver = true;
            outMsg += " ★ 长打循环违规判负！";
            return MOVE_RES_LOSS;
        }
        return MOVE_RES_MOVED;
    }

    outMsg = "请先点击己方棋子进行选择。";
    return MOVE_RES_NONE;
}

MoveResult Engine_AiMove(int& outMove, std::string& outMsg) {
    if (g_gameOver) return MOVE_RES_NONE;
    srand(static_cast<unsigned int>(time(nullptr)));

    int mv = g_searcher.SearchBestMove(g_board, g_difficulty, 800);
    outMove = mv;

    if (mv == 0) {
        g_gameOver = true;
        outMsg = "对方已无路可走，认输！你赢了！";
        return MOVE_RES_WIN;
    }

    int src = SRC(mv), dst = DST(mv);
    int pcSrc = g_board.GetPiece(src);
    int pcDst = g_board.GetPiece(dst);

    g_lastMove = mv;
    g_board.MakeMove(mv);

    char buf[256];
    if (pcDst != 0) {
        sprintf_s(buf, "【对方吃子】对方的【%s】吃掉了你的【%s】！", g_szNames[pcSrc], g_szNames[pcDst]);
    }
    else {
        sprintf_s(buf, "对方走了【%s】，轮到你走棋！", g_szNames[pcSrc]);
    }
    outMsg = buf;

    if (g_board.IsMate()) {
        g_gameOver = true;
        outMsg += " 对方占领了你的兽穴，对方获胜！";
        return MOVE_RES_LOSS;
    }
    if (g_board.IsRepetition()) {
        g_gameOver = true;
        outMsg += " 对方长打循环违规判负，你获得了胜利！";
        return MOVE_RES_WIN;
    }
    return MOVE_RES_MOVED;
}