#include "GameUI.hpp"
#include "../engine/Rules.hpp"
#include "Input.hpp"
#include <iostream>

GameUI::GameUI()
    : window(sf::VideoMode(
                 {(unsigned int)((BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN +
                                 HISTORY_WIDTH),
                  (unsigned int)((BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN +
                                 100)}), // +100 for bottom HUD bar
             "Gomoku - Local PvP") {
  window.setFramerateLimit(60);
  current_state = UIState::MAIN_MENU;
  menu_selection = 0; // Default to Solo

  if (!font.openFromFile("assets/font.ttf")) {
    std::cerr << "Failed to load font from assets/font.ttf" << std::endl;
  }
}

void GameUI::run(GameState &state, Gomoku &gomoku) {
  while (window.isOpen()) {

    // Delegate all input logic to Input module
    Input::process_events(window, current_state, menu_selection, state, gomoku);

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
    draw_best_move(state);
    draw_hud(state);
    draw_history(state);

    if (state.game_over) {
      draw_game_over(state);
    }
  }

  window.display();
}

void GameUI::draw_board() {
  // Draw grid lines
  sf::RectangleShape line(sf::Vector2f((BOARD_SIZE - 1) * CELL_SIZE, 2));
  line.setFillColor(sf::Color::Black);

  for (int i = 0; i < BOARD_SIZE; ++i) {
    // Horizontal
    line.setPosition({(float)MARGIN, (float)(MARGIN + i * CELL_SIZE - 1)});
    window.draw(line);

    // Vertical
    sf::RectangleShape vline(sf::Vector2f(2, (BOARD_SIZE - 1) * CELL_SIZE));
    vline.setFillColor(sf::Color::Black);
    vline.setPosition({(float)(MARGIN + i * CELL_SIZE - 1), (float)MARGIN});
    window.draw(vline);
  }

  // Draw coordinates
  // X axis: Letters (A-T, skipping I)
  // Y axis: Numbers (1-19 or 19-1) - we use 1-19 top to bottom here
  for (int i = 0; i < BOARD_SIZE; ++i) {
    // Column Letter
    char col_char = 'A' + i;
    if (col_char >= 'I')
      col_char++; // Skip 'I'
    std::string col_str(1, col_char);

    sf::Text col_text_top(font, col_str, 18);
    col_text_top.setFillColor(sf::Color::Black);
    col_text_top.setPosition(
        {(float)(MARGIN + i * CELL_SIZE -
                 col_text_top.getGlobalBounds().size.x / 2.0f),
         (float)(MARGIN - 30.0f)});
    window.draw(col_text_top);

    sf::Text col_text_bot(font, col_str, 18);
    col_text_bot.setFillColor(sf::Color::Black);
    col_text_bot.setPosition(
        {(float)(MARGIN + i * CELL_SIZE -
                 col_text_bot.getGlobalBounds().size.x / 2.0f),
         (float)(MARGIN + (BOARD_SIZE - 1) * CELL_SIZE + 10.0f)});
    window.draw(col_text_bot);

    // Row Number
    std::string row_str =
        std::to_string(i + 1); // 1 to 19 (or 19 to 1 if you prefer inverted)

    sf::Text row_text_left(font, row_str, 18);
    row_text_left.setFillColor(sf::Color::Black);
    row_text_left.setPosition(
        {(float)(MARGIN - 30.0f -
                 row_text_left.getGlobalBounds().size.x / 2.0f),
         (float)(MARGIN + i * CELL_SIZE - 12.0f)});
    window.draw(row_text_left);

    sf::Text row_text_right(font, row_str, 18);
    row_text_right.setFillColor(sf::Color::Black);
    row_text_right.setPosition(
        {(float)(MARGIN + (BOARD_SIZE - 1) * CELL_SIZE + 15.0f),
         (float)(MARGIN + i * CELL_SIZE - 12.0f)});
    window.draw(row_text_right);
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
  float hud_y = (BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN; // Start of HUD area
  float hud_height = 100.0f;

  // Background banner
  sf::RectangleShape hud_bg(sf::Vector2f(window.getSize().x, hud_height));
  hud_bg.setFillColor(sf::Color(40, 40, 40)); // Dark grey
  hud_bg.setPosition({0, hud_y});
  window.draw(hud_bg);

  // Left: Mode Info
  sf::Text info(font,
                current_state == UIState::PLAYING_SOLO
                    ? "Mode Solo - Esc to Quit"
                    : "Mode Multi - Esc to Quit",
                20);
  info.setFillColor(sf::Color::White);
  info.setPosition({20.0f, hud_y + 15.0f});
  window.draw(info);

  // Left (below info): AI Timer
  if (current_state == UIState::PLAYING_SOLO) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "AI Time: %.2f ms",
             state.last_ai_move_time_ms);
    sf::Text timer_text(font, buffer, 18);
    timer_text.setFillColor(sf::Color::Cyan); // Distinct color for timer
    timer_text.setPosition({20.0f, hud_y + 55.0f});
    window.draw(timer_text);
  }

  // Center: Current turn
  std::string turn_str = "Tour " + std::to_string(state.current_turn) + " : ";
  turn_str += (state.current_player == Player::BLACK) ? "NOIR" : "BLANC";
  sf::Text turn_text(font, turn_str, 24);

  if (state.current_player == Player::BLACK) {
    turn_text.setFillColor(sf::Color::Black);
    turn_text.setOutlineColor(sf::Color::White);
    turn_text.setOutlineThickness(1.0f);
  } else {
    turn_text.setFillColor(sf::Color::White);
  }

  turn_text.setPosition(
      {window.getSize().x / 2.0f - turn_text.getGlobalBounds().size.x / 2.0f,
       hud_y + 35.0f});
  window.draw(turn_text);

  // Right: Captures
  sf::Text captures_text(
      font,
      "Captures (B: " + std::to_string(state.black_captures) +
          " | W: " + std::to_string(state.white_captures) + ")",
      20);
  captures_text.setFillColor(sf::Color::White);
  captures_text.setPosition({window.getSize().x - 220.0f, hud_y + 35.0f});
  window.draw(captures_text);
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

void GameUI::draw_history(const GameState &state) {
  float hist_x = (BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN;
  float hist_width = HISTORY_WIDTH;
  float hist_height = window.getSize().y;

  // Background for history panel
  sf::RectangleShape hist_bg(sf::Vector2f(hist_width, hist_height));
  hist_bg.setFillColor(sf::Color(50, 50, 50));
  hist_bg.setPosition({hist_x, 0});
  window.draw(hist_bg);

  // Title
  sf::Text title(font, "Historique", 26);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Underlined | sf::Text::Bold);
  title.setPosition({hist_x + 10.0f, 10.0f});
  window.draw(title);

  // Render recent moves (last 23)
  size_t display_count = 23;
  size_t start_idx = 0;
  if (state.move_history.size() > display_count) {
    start_idx = state.move_history.size() - display_count;
  }

  float y_offset = 60.0f;
  float radius = 10.0f;
  sf::CircleShape stone(radius);
  stone.setOrigin({radius, radius});

  for (size_t i = start_idx; i < state.move_history.size(); ++i) {
    const GameMove &mv = state.move_history[i];

    // Stone icon
    stone.setPosition({hist_x + 20.0f, y_offset + 10.0f});
    if (mv.player == Player::BLACK) {
      stone.setFillColor(sf::Color::Black);
      stone.setOutlineColor(sf::Color::White);
      stone.setOutlineThickness(1.0f);
    } else {
      stone.setFillColor(sf::Color::White);
      stone.setOutlineThickness(0.0f);
    }
    window.draw(stone);

    // X/Y formatted (Letter and Number based on grid display)
    char col_char = 'A' + mv.x;
    if (col_char >= 'I')
      col_char++; // Skip 'I' for display
    std::string coord_str =
        std::string(1, col_char) + "-" + std::to_string(mv.y + 1);

    // Text: Tour N : x-y
    std::string line_str =
        "Tour " + std::to_string(mv.turn) + " : " + coord_str;
    sf::Text line_text(font, line_str, 18);
    line_text.setFillColor(sf::Color::White);
    line_text.setPosition({hist_x + 40.0f, y_offset});
    window.draw(line_text);

    y_offset += 30.0f;
  }
}

void GameUI::draw_best_move(const GameState &state) {
  if (state.game_over)
    return;
  if (state.best_move_suggestion.row < 0 || state.best_move_suggestion.col < 0)
    return;

  float radius = CELL_SIZE * 0.4f;
  sf::CircleShape ghost(radius);
  ghost.setOrigin({radius, radius});

  // Semi-transparent color matching the current player
  if (state.current_player == Player::BLACK) {
    ghost.setFillColor(sf::Color(0, 0, 0, 100));
  } else {
    ghost.setFillColor(sf::Color(255, 255, 255, 100));
  }

  ghost.setPosition(
      {(float)(MARGIN + state.best_move_suggestion.col * CELL_SIZE),
       (float)(MARGIN + state.best_move_suggestion.row * CELL_SIZE)});
  window.draw(ghost);
}
