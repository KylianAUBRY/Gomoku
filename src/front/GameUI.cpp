#include "GameUI.hpp"
#include "../engine/Rules.hpp"
#include "Input.hpp"
#include <iostream>

GameUI::GameUI()
    : window(sf::VideoMode(
                 {(unsigned int)((BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN),
                  (unsigned int)((BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN)}),
             "Gomoku - Local PvP") {
  window.setFramerateLimit(60);
  current_state = UIState::MAIN_MENU;
  menu_selection = 0; // Default to Solo

  if (!font.openFromFile("assets/font.ttf")) {
    std::cerr << "Failed to load font from assets/font.ttf" << std::endl;
  }
}

void GameUI::run(GameState &state) {
  while (window.isOpen()) {

    // Delegate all input logic to Input module
    Input::process_events(window, current_state, menu_selection, state);

    // Rendering phase
    if (current_state == UIState::MAIN_MENU) {
      render_menu();
    } else {
      render(state);
    }
  }
}

void GameUI::render_menu() {
  window.clear(sf::Color(40, 40, 40));

  sf::Text title(font, "GOMOKU", 60);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Bold);
  title.setPosition(
      {window.getSize().x / 2.0f - title.getGlobalBounds().size.x / 2.0f,
       100.0f});

  sf::Text solo(font, "Mode Solo (vs IA)", 40);
  solo.setFillColor(menu_selection == 0 ? sf::Color::Yellow : sf::Color::White);
  // Optional hover feedback can also be applied based on mouse position in
  // Input::process_events but we reflect selection state here:
  sf::FloatRect solo_bounds = solo.getGlobalBounds();
  solo.setPosition(
      {window.getSize().x / 2.0f - solo_bounds.size.x / 2.0f, 300.0f});

  sf::Text multi(font, "Mode Multi (Local 1v1)", 40);
  multi.setFillColor(menu_selection == 1 ? sf::Color::Yellow
                                         : sf::Color::White);
  sf::FloatRect multi_bounds = multi.getGlobalBounds();
  multi.setPosition(
      {window.getSize().x / 2.0f - multi_bounds.size.x / 2.0f, 400.0f});

  window.draw(title);
  window.draw(solo);
  window.draw(multi);

  window.display();
}

void GameUI::render(const GameState &state) {
  window.clear(sf::Color(220, 179, 92)); // Goban wood color

  draw_board();
  draw_stones(state);

  if (current_state == UIState::PLAYING_SOLO ||
      current_state == UIState::PLAYING_MULTI) {
    draw_hud(state);

    if (state.game_over) {
      draw_game_over(state);
    }
  }

  window.display();
}

void GameUI::draw_board() {
  // Draw grid
  sf::RectangleShape line(sf::Vector2f((BOARD_SIZE - 1) * CELL_SIZE, 2));
  line.setFillColor(sf::Color::Black);

  for (int i = 0; i < BOARD_SIZE; ++i) {
    // Horizontal
    line.setPosition({(float)MARGIN, (float)(MARGIN + i * CELL_SIZE - 1)});
    window.draw(line);

    // Vertical (rotate)
    sf::RectangleShape vline(sf::Vector2f(2, (BOARD_SIZE - 1) * CELL_SIZE));
    vline.setFillColor(sf::Color::Black);
    vline.setPosition({(float)(MARGIN + i * CELL_SIZE - 1), (float)MARGIN});
    window.draw(vline);
  }
}

void GameUI::draw_stones(const GameState &state) {
  float radius = CELL_SIZE * 0.4f;
  sf::CircleShape stone(radius);
  stone.setOrigin({radius, radius});

  for (int y = 0; y < BOARD_SIZE; ++y) {
    for (int x = 0; x < BOARD_SIZE; ++x) {
      if (state.board.get(y, x) == BLACK) {
        stone.setFillColor(sf::Color::Black);
        stone.setPosition(
            {(float)(MARGIN + x * CELL_SIZE), (float)(MARGIN + y * CELL_SIZE)});
        window.draw(stone);
      } else if (state.board.get(y, x) == WHITE) {
        stone.setFillColor(sf::Color::White);
        stone.setPosition(
            {(float)(MARGIN + x * CELL_SIZE), (float)(MARGIN + y * CELL_SIZE)});
        window.draw(stone);
      }
    }
  }
}

void GameUI::draw_hud(const GameState &state) {
  sf::Text info(font,
                current_state == UIState::PLAYING_SOLO
                    ? "Mode Solo - Esc to Quit"
                    : "Mode Multi - Esc to Quit",
                20);
  info.setFillColor(sf::Color::White);
  info.setPosition({10, 10});
  window.draw(info);

  // Draw Captures
  sf::Text captures_text(
      font,
      "Captures (B: " + std::to_string(state.black_captures) +
          " | W: " + std::to_string(state.white_captures) + ")",
      20);
  captures_text.setFillColor(sf::Color::White);
  captures_text.setPosition({window.getSize().x - 220.0f, 10.0f});
  window.draw(captures_text);

  // Draw Current turn
  std::string turn_str = "Tour : ";
  turn_str += (state.current_player == Player::BLACK) ? "NOIR" : "BLANC";
  sf::Text turn_text(font, turn_str, 20);
  turn_text.setFillColor(state.current_player == Player::BLACK
                             ? sf::Color::Black
                             : sf::Color::White);
  turn_text.setPosition(
      {window.getSize().x / 2.0f - turn_text.getGlobalBounds().size.x / 2.0f,
       10.0f});
  window.draw(turn_text);

  // Draw AI Timer (Only in Solo mode)
  if (current_state == UIState::PLAYING_SOLO) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "AI Time: %.2f ms",
             state.last_ai_move_time_ms);
    sf::Text timer_text(font, buffer, 20);
    timer_text.setFillColor(sf::Color::Cyan); // Distinct color for timer
    timer_text.setPosition({10.0f, window.getSize().y - 30.0f});
    window.draw(timer_text);
  }
}

void GameUI::draw_game_over(const GameState &state) {
  bool black_wins = Rules::check_win_condition(state, Player::BLACK) ||
                    Rules::check_win_by_capture(state, Player::BLACK);
  bool white_wins = Rules::check_win_condition(state, Player::WHITE) ||
                    Rules::check_win_by_capture(state, Player::WHITE);

  std::string win_str = "GAME OVER";
  if (black_wins)
    win_str = "BLACK WINS!";
  else if (white_wins)
    win_str = "WHITE WINS!";

  sf::RectangleShape overlay(
      sf::Vector2f(window.getSize().x, window.getSize().y));
  overlay.setFillColor(sf::Color(0, 0, 0, 150));
  window.draw(overlay);

  sf::Text win_msg(font, win_str, 50);
  win_msg.setFillColor(sf::Color::Red);
  win_msg.setStyle(sf::Text::Bold);
  win_msg.setPosition(
      {window.getSize().x / 2.0f - win_msg.getGlobalBounds().size.x / 2.0f,
       window.getSize().y / 2.0f - 100});
  window.draw(win_msg);

  // Rejouer Button
  sf::RectangleShape btn(sf::Vector2f(200, 60));
  btn.setFillColor(sf::Color(100, 100, 100)); // Dark grey base
  btn.setPosition(
      {window.getSize().x / 2.0f - 100, window.getSize().y / 2.0f + 20});
  window.draw(btn);

  sf::Text replay_txt(font, "Rejouer", 30);
  replay_txt.setFillColor(sf::Color::White);
  replay_txt.setPosition(
      {window.getSize().x / 2.0f - replay_txt.getGlobalBounds().size.x / 2.0f,
       window.getSize().y / 2.0f + 30});
  window.draw(replay_txt);
}
