#pragma once
#include <string>
#include <vector>
#include <array>

constexpr int BOARD_RANK_TOP = 3;
constexpr int BOARD_RANK_BOTTOM = 9;
constexpr int BOARD_FILE_LEFT = 3;
constexpr int BOARD_FILE_RIGHT = 11;

enum AnimalType {
    ANIMAL_ELEPHANT = 0,
    ANIMAL_LION = 1,
    ANIMAL_TIGER = 2,
    ANIMAL_LEOPARD = 3,
    ANIMAL_WOLF = 4,
    ANIMAL_DOG = 5,
    ANIMAL_CAT = 6,
    ANIMAL_MOUSE = 7
};

enum TerrainType {
    TERRAIN_LAND = 0,
    TERRAIN_RED_DEN = 1,
    TERRAIN_RED_TRAP = 2,
    TERRAIN_RIVER = 3,
    TERRAIN_BLACK_TRAP = 4,
    TERRAIN_BLACK_DEN = 5
};

enum MoveResult {
    MOVE_RES_NONE = 0,
    MOVE_RES_SELECT,
    MOVE_RES_MOVED,
    MOVE_RES_ILLEGAL,
    MOVE_RES_WIN,
    MOVE_RES_LOSS
};

enum DifficultyLevel {
    DIFF_EASY = 1,
    DIFF_MEDIUM = 2,
    DIFF_HARD = 3
};

struct MoveRecord {
    int moveVal;
    int capturedPiece;
    int movedPiece;
};

class JungleBoard {
public:
    JungleBoard();
    void Reset();

    bool MakeMove(int mv);
    void UndoMove();

    int  GenerateMoves(std::vector<int>& outMoves, bool capturesOnly = false) const;
    bool IsLegalMove(int mv) const;
    bool IsGameOver() const;
    bool IsMate() const;
    bool IsRepetition() const;

    int  GetPiece(int sq) const;
    int  GetSide() const { return m_side; }

    bool CanMove(int src, int dst) const;
    bool CanJump(int src, int dst) const;
    bool CanEat(int src, int dst) const;

    static bool InBoard(int sq);
    static TerrainType GetTerrain(int sq);
    static int CoordToSq(int x, int y) { return x + (y << 4); }
    static int SqToFileX(int sq) { return sq & 15; }
    static int SqToRankY(int sq) { return sq >> 4; }

private:
    std::array<int, 256> m_squares;
    int m_side;
    std::vector<MoveRecord> m_history;

    void AddPiece(int sq, int piece);
    void DelPiece(int sq, int piece);
};

class JungleEvaluator {
public:
    static int Evaluate(const JungleBoard& board);

private:
    static int EvaluateMaterial(const JungleBoard& board);
    static int EvaluateDenProximity(const JungleBoard& board);
    static int EvaluateRiverControl(const JungleBoard& board);
    static int EvaluateTrapSafety(const JungleBoard& board);
};

class AlphaBetaSearcher {
public:
    AlphaBetaSearcher();
    int SearchBestMove(JungleBoard& board, DifficultyLevel level, int timeLimitMs);

private:
    std::array<int, 65536> m_historyTable;

    int QuiescenceSearch(JungleBoard& board, int alpha, int beta, int depth);
    int AlphaBeta(JungleBoard& board, int depth, int alpha, int beta, bool isRoot = false);
};

void Engine_Startup();
int  Engine_GetTurn();
int  Engine_GetPiece(int sq);
TerrainType Engine_GetTerrain(int sq);
int  Engine_GetSelectedSq();
void Engine_ClearSelectedSq();
int  Engine_GetLastMove();
bool Engine_IsGameOver();

MoveResult Engine_ClickSquare(int sq, std::string& outMsg);
MoveResult Engine_AiMove(int& outMove, std::string& outMsg);
void Engine_SetDifficulty(DifficultyLevel level);
DifficultyLevel Engine_GetDifficulty();

inline int Engine_CoordToSq(int x, int y) { return JungleBoard::CoordToSq(x, y); }
inline int Engine_SqToFileX(int sq) { return JungleBoard::SqToFileX(sq); }
inline int Engine_SqToRankY(int sq) { return JungleBoard::SqToRankY(sq); }
const char* Engine_GetPieceName(int pc);