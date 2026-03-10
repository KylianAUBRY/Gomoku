#include "Gomoku.hpp"

Gomoku::Gomoku() {
        for (int i = 0; i < SIZE; ++i) {
            for (int j = 0; j < SIZE; ++j) {
                board[i][j] = EMPTY;
            }
        }
    }