#pragma once
#include <cstdint>
#include <vector>
#include <array>

constexpr int BOARD_ROWS = 7;
constexpr int BOARD_COLS = 9;
constexpr int BOARD_CELL_COUNT = 63;

constexpr int BOARD_RANK_TOP = 3;
constexpr int BOARD_RANK_BOTTOM = 9;
constexpr int BOARD_FILE_LEFT = 3;
constexpr int BOARD_FILE_RIGHT = 11;

enum TerrainType {
    TERRAIN_LAND = 0,
    TERRAIN_RED_DEN = 1,
    TERRAIN_RED_TRAP = 2,
    TERRAIN_RIVER = 3,
    TERRAIN_BLACK_TRAP = 4,
    TERRAIN_BLACK_DEN = 5
};

#pragma pack(push, 1)
struct MsgBoardSnapshot {
    uint8_t gameStatus;
    uint8_t currentTurn;
    uint8_t lastSrc;
    uint8_t lastDst;
    uint8_t board[BOARD_CELL_COUNT];
};
#pragma pack(pop)

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

    static int IndexToSq(uint8_t idx);
    static uint8_t SqToIndex(int sq);

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
};

class AlphaBetaSearcher {
public:
    AlphaBetaSearcher();
    int SearchBestMove(JungleBoard& board, int depth);

private:
    std::array<int, 65536> m_historyTable;
    int QuiescenceSearch(JungleBoard& board, int alpha, int beta, int depth);
    int AlphaBeta(JungleBoard& board, int depth, int alpha, int beta, bool isRoot = false);
};

//对外导出的通用数据与操作接口，方便后续直接对接UE。
void Engine_Startup();
void Engine_GetSnapshot(MsgBoardSnapshot& outSnapshot);
bool Engine_IsLegalMove(uint8_t srcIdx, uint8_t dstIdx);
bool Engine_TryMove(uint8_t srcIdx, uint8_t dstIdx);
bool Engine_TriggerAi();
TerrainType Engine_GetTerrainByIndex(uint8_t idx);
const char* Engine_GetPieceName(uint8_t pc);
