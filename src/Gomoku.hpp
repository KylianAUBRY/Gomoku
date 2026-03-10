#pragma once
#include <array>
#include <chrono>

constexpr int SIZE = 19; //19 x 19 board

enum Cell {
    EMPTY,
    BLACK,
    WHITE
};

using Board = std::array<std::array<Cell, SIZE>, SIZE>;

struct Move {
    int row;
    int col;

    long computeTimeMs;
};

class Gomoku {
public:
    Board board;

    Gomoku();

    Move getBestMove(const Board& board, Cell player);

private:
    Move minimax(const Board& board, Cell player);
};