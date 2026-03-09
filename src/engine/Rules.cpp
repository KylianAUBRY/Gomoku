#include "Rules.hpp"

namespace Rules {

bool check_alignment_at(const Bitboard &board, int sx, int sy, int dx, int dy) {
  int count = 0;
  int x = sx, y = sy;

  while (in_bounds(x, y) && board.get_bit(x, y)) {
    count++;
    x += dx;
    y += dy;
  }

  // Check opposite direction to get the full line
  x = sx - dx;
  y = sy - dy;
  while (in_bounds(x, y) && board.get_bit(x, y)) {
    count++;
    x -= dx;
    y -= dy;
  }

  return count >= 5;
}

bool check_win_condition(const GameState &state, Player player) {
  const Bitboard &board =
      (player == Player::BLACK) ? state.black_board : state.white_board;

  // Iterate through all placed stones of that player
  for (int y = 0; y < 19; ++y) {
    for (int x = 0; x < 19; ++x) {
      if (board.get_bit(x, y)) {
        // Check all 4 axes: Horizontal, Vertical, Diagonal 1, Diagonal 2
        if (check_alignment_at(board, x, y, 1, 0) ||
            check_alignment_at(board, x, y, 0, 1) ||
            check_alignment_at(board, x, y, 1, 1) ||
            check_alignment_at(board, x, y, 1, -1)) {
          return true;
        }
      }
    }
  }
  return false;
}

CaptureResult process_captures(GameState &state, int x, int y,
                               Player current_player) {
  CaptureResult result = {false, 0};
  Bitboard &own_board =
      (current_player == Player::BLACK) ? state.black_board : state.white_board;
  Bitboard &opp_board =
      (current_player == Player::BLACK) ? state.white_board : state.black_board;

  // 8 directions to check for captures: (dx, dy)
  int directions[8][2] = {{1, 0}, {-1, 0},  {0, 1},  {0, -1},
                          {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};

  for (auto &dir : directions) {
    int dx = dir[0], dy = dir[1];
    int x1 = x + dx, y1 = y + dy;
    int x2 = x + 2 * dx, y2 = y + 2 * dy;
    int x3 = x + 3 * dx, y3 = y + 3 * dy;

    if (in_bounds(x3, y3)) {
      // If sequence is: Own(x,y) - Opp(x1,y1) - Opp(x2,y2) - Own(x3,y3)
      if (opp_board.get_bit(x1, y1) && opp_board.get_bit(x2, y2) &&
          own_board.get_bit(x3, y3)) {
        // Capture! Remove opponent stones.
        opp_board.clear_bit(x1, y1);
        opp_board.clear_bit(x2, y2);
        result.captured = true;
        result.count += 2;
      }
    }
  }

  if (result.captured) {
    if (current_player == Player::BLACK)
      state.black_captures += result.count;
    else
      state.white_captures += result.count;
  }

  return result;
}

bool is_valid_move(const GameState &state, int x, int y) {
  if (!state.is_empty(x, y))
    return false;
  // Placeholder for Double-Three verification
  return true;
}

bool check_win_by_capture(const GameState &state, Player player) {
  if (player == Player::BLACK)
    return state.black_captures >= 10;
  return state.white_captures >= 10;
}

} // namespace Rules
