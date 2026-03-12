#pragma once

#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include <SFML/Graphics.hpp>

enum class UIState { MAIN_MENU, PLAYING_SOLO, PLAYING_MULTI };

// Layout constants
static constexpr float CELL_SIZE = 35.0f;
static constexpr int BOARD_SIZE = 19;
static constexpr float MARGIN = 45.0f;
static constexpr float HISTORY_WIDTH = 250.0f;

class GameUI {
public:
  GameUI();

  // Main run loop
  void run(GameState &state, Gomoku &gomoku);

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

  void draw_board();
  void draw_stones(const GameState &state);
  void draw_hud(const GameState &state);
  void draw_game_over(const GameState &state);
  void draw_history(const GameState &state);
  void draw_best_move(const GameState &state);
};
