#include "engine/GameState.hpp"
#include "front/GameUI.hpp"
#include "Gomoku.hpp"
int main() {
  Gomoku game;
  GameState state;
  GameUI ui;

  ui.run(state);

  return 0;
}
