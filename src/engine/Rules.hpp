#pragma once

#include "GameState.hpp"
#include <vector>

namespace Rules {

struct CaptureResult {
  bool captured;
  int count;
};

// Forward declarations logic
bool check_win_condition(const GameState &state, Player player);
CaptureResult process_captures(GameState &state, int x, int y,
                               Player current_player);
bool is_valid_move(const GameState &state, int x, int y);
bool check_win_by_capture(const GameState &state, Player player);

// Simple helper to check bounds (inlined as it's trivial)
inline bool in_bounds(int x, int y) {
  return x >= 0 && x < 19 && y >= 0 && y < 19;
}
} // namespace Rules
