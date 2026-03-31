#pragma once
#include <array>
#include <chrono>
#include <vector>
#include <cstdint>

constexpr int SIZE = 19;
constexpr int DEPTH_LIMIT = 5;
constexpr int MOVE_LIMIT = 20; // nb max de coups explorés par noeud dans minimax
constexpr int RANGE = 1;
constexpr int BitBoard_SIZE = 6; // ceil(19*19 / 64) = 6

extern int evalTable[262144][2];

extern uint64_t zobristTable[SIZE][SIZE][3]; // [row][col][EMPTY=0/BLACK=1/WHITE=2]
extern uint64_t zobristCaptures[256][2];     // [nb_captures][0=white / 1=black]
void initZobrist();


// La TT stocke les scores déjà
// calculés pour éviter de re-explorer ces branches.
enum TTFlag : uint8_t { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };
//  TT_EXACT : score exact — la recherche a exploré tous les coups
//                sans coupure et le score est entre alpha et beta
//
//  TT_LOWER : borne inférieure — vrai score ≥ score stocké
//                (fail-high WHITE : on a coupé car score ≥ beta,
//                 on n'a pas exploré toutes les branches, mais on sait
//                 que le score est au moins aussi bon)
//
//  TT_UPPER : borne supérieure — vrai score ≤ score stocké
//                (fail-low WHITE  : tous les coups étaient ≤ alpha,
//                 ou coupure BLACK : score ≤ alpha)
struct TTEntry {
    uint64_t hash;   // hash complet (vérification anti-collision)
    int      score;
    int8_t   depth;  // profondeur restante (DEPTH_LIMIT - current_depth)
    TTFlag   flag;

};

constexpr size_t TT_SIZE = 1 << 20;
constexpr size_t TT_MASK = TT_SIZE - 1;
static TTEntry ttTable[TT_SIZE];

#define MANUAL_CAPTURE_SCORE 15000

// evalTable[i][1] layout:
// bit 0 : capture blanche vers le haut  (1,2,2,1 à droite du centre)
// bit 1 : capture blanche vers le bas   (1,2,2,1 à gauche du centre)
// bit 2 : capture noire vers le haut    (2,1,1,2 à droite du centre)
// bit 3 : capture noire vers le bas     (2,1,1,2 à gauche du centre)
// bits 6-8 : nb free-three blancs
// bits 9-11: nb free-three noirs

#define GET_WHITE_CAPTURES_UP(f)    (((f) >> 0) & 0x1)
#define GET_WHITE_CAPTURES_DOWN(f)  (((f) >> 1) & 0x1)
#define GET_BLACK_CAPTURES_UP(f)    (((f) >> 2) & 0x1)
#define GET_BLACK_CAPTURES_DOWN(f)  (((f) >> 3) & 0x1)
#define GET_WHITE_THREES(f)         (((f) >> 6) & 0x7)
#define GET_BLACK_THREES(f)         (((f) >> 9) & 0x7)

#define ADD_WHITE_CAPTURES_UP(f)    ((f) |= (1 << 0))
#define ADD_WHITE_CAPTURES_DOWN(f)  ((f) |= (1 << 1))
#define ADD_BLACK_CAPTURES_UP(f)    ((f) |= (1 << 2))
#define ADD_BLACK_CAPTURES_DOWN(f)  ((f) |= (1 << 3))
#define ADD_WHITE_THREES(f)         ((f) += (1 << 6))
#define ADD_BLACK_THREES(f)         ((f) += (1 << 9))

enum Cell {
    EMPTY,
    BLACK,
    WHITE
};

// index = row * SIZE + col, bit dans black[] ou white[]
struct BitBoard {
    uint64_t black[BitBoard_SIZE] = {};
    uint64_t white[BitBoard_SIZE] = {};
    int score = 0;
    uint8_t blackCaptures = 0;
    uint8_t whiteCaptures = 0;
    uint64_t hash = 0; // Hash Zobrist incrémental — mis à jour dans apply_move

    // Retourne la cellule à (row, col)
    Cell get(int row, int col) const {
        int idx = row * SIZE + col;
        int word = idx / 64;
        int bit  = idx % 64;
        if ((black[word] >> bit) & 1ULL) return BLACK;
        if ((white[word] >> bit) & 1ULL) return WHITE;
        return EMPTY;
    }

    // Place une pièce à (row, col)
    void set(int row, int col, Cell player) {
        int idx = row * SIZE + col;
        int word = idx / 64;
        int bit  = idx % 64;
        // Efface les deux
        black[word] &= ~(1ULL << bit);
        white[word] &= ~(1ULL << bit);
        if (player == BLACK) black[word] |= (1ULL << bit);
        else if (player == WHITE) white[word] |= (1ULL << bit);
    }

    // Vérifie si (row, col) est vide
    bool isEmpty(int row, int col) const {
        return get(row, col) == EMPTY;
    }

    bool hasCaptureWin(Cell player) const {
        if (player == BLACK) return blackCaptures >= 10;
        if (player == WHITE) return whiteCaptures >= 10;
        return false;
    }
};

struct Move {
    int row;
    int col;
    int score;
    long computeTimeMs;
};

class Gomoku {
public:
    BitBoard board;

    Gomoku();

    Move getBestMove(BitBoard& board, Cell player);
    Move getBestMove2(BitBoard& board, Cell player);
    Move getBestMove3(BitBoard& board, Cell player);
    bool isLegalMove(const BitBoard& board, int row, int col, Cell player);
    

private:
    std::vector<Move> generateMoves(const BitBoard& board, Cell player);
    Move minimax(int depth, BitBoard& board, Cell player, int alpha, int beta);
    Move minimax2(int depth, BitBoard& board, Cell player, int alpha, int beta);
    Move minimax3(int depth, BitBoard& board, Cell player, int alpha, int beta);
};

int makeMove(BitBoard& board, const Move& move, Cell player);
inline int idx(int row, int col) { return row * SIZE + col; }
inline bool getBit(const uint64_t* bb, int pos) { return (bb[pos / 64] >> (pos % 64)) & 1ULL; }
inline void setBit(uint64_t* bb, int pos) { bb[pos / 64] |= (1ULL << (pos % 64)); }
inline void clearBit(uint64_t* bb, int pos) { bb[pos / 64] &= ~(1ULL << (pos % 64)); }
inline bool isEmpty(const BitBoard& b, int row, int col) { int p = idx(row, col); return !getBit(b.white, p) && !getBit(b.black, p); }
void computeInRange(const BitBoard& b, uint64_t* inRange);
int computeLineScore(const BitBoard& board, int row, int col, int dr, int dc);
void undoMove(BitBoard& board, const Move& move, Cell player);