#pragma once

#include "../engine/GameState.hpp"
#include <SFML/Graphics.hpp>

enum class UIState { MAIN_MENU, PLAYING_SOLO, PLAYING_MULTI };

// Layout constants
static constexpr float CELL_SIZE = 40.0f;
static constexpr int BOARD_SIZE = 19;
static constexpr float MARGIN = 50.0f;

class GameUI {
public:
  GameUI();

  // Main run loop
  void run(GameState &state);

  // Getters for external input abstraction
  sf::RenderWindow &getWindow() { return window; }
  UIState &getCurrentState() { return current_state; }
  int &getMenuSelection() { return menu_selection; }

private:
  sf::RenderWindow window;
  sf::Font font;
  UIState current_state;
  int menu_selection;

  // Render operations
  void render_menu();
  void render(const GameState &state);
};
