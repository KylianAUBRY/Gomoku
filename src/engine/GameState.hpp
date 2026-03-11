#pragma once

#include "Gomoku.hpp"

enum class Player { NONE = 0, BLACK = 1, WHITE = 2 };

struct GameState {
  BitBoard board;
  Player current_player = Player::BLACK;

  int black_captures = 0;
  int white_captures = 0;
  bool game_over = false;

  double last_ai_move_time_ms = 0.0;

  void reset() {
    board = BitBoard(); // Clear board
    current_player = Player::BLACK;
    black_captures = 0;
    white_captures = 0;
    game_over = false;
    last_ai_move_time_ms = 0.0;
  }

  inline bool is_empty(int x, int y) const { return board.isEmpty(y, x); }

  // Places a stone. Returns true if successful, false if invalid/occupied.
  inline bool place_stone(int x, int y) {
    if (x < 0 || x >= 19 || y < 0 || y >= 19 || !is_empty(x, y)) {
      return false; // Invalid move
    }

    if (current_player == Player::BLACK) {
      board.set(y, x, BLACK);
      current_player = Player::WHITE;
    } else {
      board.set(y, x, WHITE);
      current_player = Player::BLACK;
    }
    return true;
  }
};
