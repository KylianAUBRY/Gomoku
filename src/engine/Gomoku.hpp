#pragma once
#include <array>
#include <chrono>
#include <vector>
#include <cstdint>

constexpr int SIZE = 19;
constexpr int DEPTH_LIMIT = 5;
constexpr int RANGE = 1;
constexpr int BitBoard_SIZE = 6; // ceil(19*19 / 64) = 6

extern int evalTable[262144][2];

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
    bool isLegalMove(const BitBoard& board, int row, int col, Cell player);
    

private:
    std::vector<Move> generateMoves(const BitBoard& board, Cell player);
    Move minimax(int depth, BitBoard& board, Cell player, int alpha, int beta);
    Move minimax2(int depth, BitBoard& board, Cell player, int alpha, int beta);
};

int makeMove(BitBoard& board, const Move& move, Cell player);
