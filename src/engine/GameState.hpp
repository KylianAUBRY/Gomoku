#pragma once

#include "Gomoku.hpp"
#include <vector>

enum class Player { NONE = 0, BLACK = 1, WHITE = 2 };

struct GameMove {
  Player player;
  int x;
  int y;
  int turn;
};

struct GameState {
  BitBoard board;
  Player current_player = Player::BLACK;

  bool game_over = false;

  double last_ai_move_time_ms = 0.0;
  int current_turn = 1;

  // Bot vs Bot average times
  double black_avg_time_ms = 0.0;
  int    black_move_count  = 0;
  double white_avg_time_ms = 0.0;
  int    white_move_count  = 0;

  Move best_move_suggestion = {-1, -1, 0, 0};

  std::vector<GameMove> move_history;

  void reset() {
    board = BitBoard(); // Clear board
    current_player = Player::BLACK;
    game_over = false;
    last_ai_move_time_ms = 0.0;
    current_turn = 1;
    black_avg_time_ms = 0.0;
    black_move_count  = 0;
    white_avg_time_ms = 0.0;
    white_move_count  = 0;
    best_move_suggestion = {-1, -1, 0, 0};
    move_history.clear();
  }

  inline bool is_empty(int x, int y) const { return board.isEmpty(y, x); }

  // Places a stone. Returns true if successful, false if invalid/occupied.
  inline bool place_stone(int x, int y) {
    if (x < 0 || x >= 19 || y < 0 || y >= 19 || !is_empty(x, y)) {
      return false; // Invalid move
    }
    if (current_player == Player::BLACK) {
      if (makeMove(board, {y, x, 0, 0}, BLACK) == 1) {
        return false; // Move was illegal (e.g. double-three)
      }
      current_player = Player::WHITE;
    } else {
      if (makeMove(board, {y, x, 0, 0}, WHITE) == 1) {
        return false; // Move was illegal (e.g. double-three)
      }
      current_player = Player::BLACK;
    }

    move_history.push_back(
        {current_player == Player::BLACK ? Player::WHITE : Player::BLACK, x, y,
         current_turn});
    current_turn++;
    return true;
  }
};
