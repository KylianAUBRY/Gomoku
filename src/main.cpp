#include "engine/GameState.hpp"
#include "front/GameUI.hpp"

int main() {
  GameState state;
  GameUI ui;

  ui.run(state);

  return 0;
}
