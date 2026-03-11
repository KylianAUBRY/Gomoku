#pragma once
#include <array>
#include <chrono>
#include <vector>
#include <cstdint>

constexpr int SIZE = 19;
constexpr int DEPTH_LIMIT = 10;
constexpr int RANGE = 2;
constexpr int BitBoard_SIZE = 6; // ceil(19*19 / 64) = 6

extern int16_t evalTable[19683];


enum Cell {
    EMPTY,
    BLACK,
    WHITE
};

// index = row * SIZE + col, bit dans black[] ou white[]
struct BitBoard {
    uint64_t black[BitBoard_SIZE] = {};
    uint64_t white[BitBoard_SIZE] = {};

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

    Move getBestMove(const BitBoard& board, Cell player);
    bool isLegalMove(const BitBoard& board, int row, int col, Cell player);

private:
    std::vector<Move> generateMoves(const BitBoard& board, Cell player);
    Move minimax(int depth, BitBoard& board, Cell player);
};
