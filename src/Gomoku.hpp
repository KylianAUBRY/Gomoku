#pragma once
#include <array>
#include <chrono>
#include <vector>

constexpr int SIZE = 19; //19 x 19 board
constexpr int DEPTH_LIMIT = 10; 
constexpr int RANGE = 2; // rayon autour des pièces existantes

enum Cell {
    EMPTY,
    BLACK,
    WHITE
};

using Board = std::array<std::array<Cell, SIZE>, SIZE>;

struct Move {
    int row;
    int col;
    int score;
    long computeTimeMs;
};

class Gomoku {
public:
    Board board;

    Gomoku();

    Move getBestMove(const Board& board, Cell player);

private:
    std::vector<Move> generateMoves(const Board& board);
    Move minimax(int depth, bool maximizingPlayer);
};