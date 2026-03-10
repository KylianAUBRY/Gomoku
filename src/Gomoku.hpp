#pragma once
#include <array>

constexpr int SIZE = 19; //19 x 19 board

enum Cell {
    EMPTY,
    BLACK,
    WHITE
};

using Board = std::array<std::array<Cell, SIZE>, SIZE>;
/* ==================== include ==================== */

# include <unistd.h>
# include <stdio.h>

/* ==================== structure ==================== */

class Gomoku {
public:
    Board board;
    Gomoku();

};