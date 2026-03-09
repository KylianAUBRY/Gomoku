#pragma once

#include "Bitboard.hpp"

enum class Player { NONE = 0, BLACK = 1, WHITE = 2 };

struct GameState {
  Bitboard black_board;
  Bitboard white_board;
  Player current_player = Player::BLACK;

  int black_captures = 0;
  int white_captures = 0;

  inline bool is_empty(int x, int y) const {
    return !black_board.get_bit(x, y) && !white_board.get_bit(x, y);
  }

  // Places a stone. Returns true if successful, false if invalid/occupied.
  inline bool place_stone(int x, int y) {
    if (x < 0 || x >= 19 || y < 0 || y >= 19 || !is_empty(x, y)) {
      return false; // Invalid move
    }

    if (current_player == Player::BLACK) {
      black_board.set_bit(x, y);
      current_player = Player::WHITE;
    } else {
      white_board.set_bit(x, y);
      current_player = Player::BLACK;
    }
    return true;
  }
};
