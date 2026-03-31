#include "GameUI.hpp"
#include "../engine/Rules.hpp"
#include "Input.hpp"
#include <algorithm>
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
  suggestion_shown = false;

  if (!font.openFromFile("assets/font.ttf")) {
    std::cerr << "Failed to load font from assets/font.ttf" << std::endl;
  }
}

void GameUI::run(GameState &state, Gomoku &gomoku) {
  while (window.isOpen()) {

    // Snapshot captures and board before processing input
    uint8_t cap_b_before = state.board.blackCaptures;
    uint8_t cap_w_before = state.board.whiteCaptures;
    BitBoard board_snap = state.board;

    // Delegate all input logic to Input module
    Input::process_events(window, current_state, menu_selection, state, gomoku, suggestion_shown);

    // Detect newly captured stones and register animations
    if (state.board.blackCaptures != cap_b_before || state.board.whiteCaptures != cap_w_before) {
      for (int r = 0; r < 19; r++)
        for (int c = 0; c < 19; c++)
          if (board_snap.get(r, c) != EMPTY && state.board.get(r, c) == EMPTY)
            capture_anims.push_back({r, c, 1.0f});
    }

    // Update capture animation timers
    float dt = frame_clock.restart().asSeconds();
    update_capture_anims(dt);

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
  sf::FloatRect solo_bounds = solo.getGlobalBounds();
  solo.setPosition(
      {window.getSize().x / 2.0f - solo_bounds.size.x / 2.0f, 280.0f});

  sf::Text multi(font, "Mode Multi (Local 1v1)", 40);
  multi.setFillColor(menu_selection == 1 ? sf::Color::Yellow
                                         : sf::Color::White);
  sf::FloatRect multi_bounds = multi.getGlobalBounds();
  multi.setPosition(
      {window.getSize().x / 2.0f - multi_bounds.size.x / 2.0f, 370.0f});

  sf::Text bot(font, "Bot vs Bot", 40);
  bot.setFillColor(menu_selection == 2 ? sf::Color::Yellow : sf::Color::White);
  sf::FloatRect bot_bounds = bot.getGlobalBounds();
  bot.setPosition(
      {window.getSize().x / 2.0f - bot_bounds.size.x / 2.0f, 460.0f});

  sf::Text bench(font, "Benchmark (4 parties)", 40);
  bench.setFillColor(menu_selection == 3 ? sf::Color::Yellow : sf::Color::White);
  sf::FloatRect bench_bounds = bench.getGlobalBounds();
  bench.setPosition(
      {window.getSize().x / 2.0f - bench_bounds.size.x / 2.0f, 550.0f});

  window.draw(title);
  window.draw(solo);
  window.draw(multi);
  window.draw(bot);
  window.draw(bench);

  window.display();
}

void GameUI::render(const GameState &state) {
  window.clear(sf::Color(220, 179, 92)); // Goban wood color

  draw_board();
  draw_stones(state);
  draw_capture_anims();

  if (current_state == UIState::PLAYING_SOLO ||
      current_state == UIState::PLAYING_MULTI ||
      current_state == UIState::PLAYING_BOT ||
      current_state == UIState::PLAYING_BENCHMARK) {
    draw_best_move(state);
    draw_hud(state);
    draw_history(state);

    if (state.game_over) {
      draw_game_over(state, current_state);
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
    // if (col_char >= 'I')
    //   col_char++; // Skip 'I'
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
  std::string mode_str;
  if (current_state == UIState::PLAYING_SOLO)
    mode_str = "Mode Solo - Esc to Quit";
  else if (current_state == UIState::PLAYING_MULTI)
    mode_str = "Mode Multi - Esc to Quit";
  else if (current_state == UIState::PLAYING_BOT)
    mode_str = "Bot vs Bot - Esc to Quit";
  else
    mode_str = "Benchmark - Esc to Quit";
  sf::Text info(font, mode_str, 20);
  info.setFillColor(sf::Color::White);
  info.setPosition({20.0f, hud_y + 15.0f});
  window.draw(info);

  // Left (below info): AI Timer(s)
  if (current_state == UIState::PLAYING_SOLO) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "AI Time: %.2f ms",
             state.last_ai_move_time_ms);
    sf::Text timer_text(font, buffer, 18);
    timer_text.setFillColor(sf::Color::Cyan);
    timer_text.setPosition({20.0f, hud_y + 55.0f});
    window.draw(timer_text);
  } else if (current_state == UIState::PLAYING_BOT) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Moy Bot1(N): %.2f ms  |  Moy Bot2(B): %.2f ms",
             state.black_avg_time_ms, state.white_avg_time_ms);
    sf::Text timer_text(font, buffer, 16);
    timer_text.setFillColor(sf::Color::Cyan);
    timer_text.setPosition({20.0f, hud_y + 55.0f});
    window.draw(timer_text);
  } else if (current_state == UIState::PLAYING_BENCHMARK) {
    char buffer[160];
    int game = state.benchmark_game;
    if (game >= 1 && game <= 4) {
      snprintf(buffer, sizeof(buffer),
               "Partie %d/4  |  Moy getBestMove: %.1f ms  |  Moy getBestMove2: %.1f ms",
               game, state.black_avg_time_ms, state.white_avg_time_ms);
    } else {
      snprintf(buffer, sizeof(buffer), "Benchmark termine — voir le terminal");
    }
    sf::Text timer_text(font, buffer, 15);
    timer_text.setFillColor(sf::Color::Cyan);
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
      "Captures (B: " + std::to_string(state.board.blackCaptures) +
          " | W: " + std::to_string(state.board.whiteCaptures) + ")",
      20);
  captures_text.setFillColor(sf::Color::White);
  captures_text.setPosition({window.getSize().x - 220.0f, hud_y + 35.0f});
  window.draw(captures_text);
}

void GameUI::draw_game_over(const GameState &state, UIState ui_state) {
  // Benchmark done: show a dedicated overlay (no replay button)
  if (ui_state == UIState::PLAYING_BENCHMARK && state.benchmark_game == 5) {
    sf::RectangleShape overlay(
        sf::Vector2f(window.getSize().x, window.getSize().y));
    overlay.setFillColor(sf::Color(0, 0, 0, 170));
    window.draw(overlay);

    sf::Text msg(font, "BENCHMARK TERMINE", 48);
    msg.setFillColor(sf::Color::Yellow);
    msg.setStyle(sf::Text::Bold);
    msg.setPosition(
        {window.getSize().x / 2.0f - msg.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 80});
    window.draw(msg);

    sf::Text sub(font, "Resultats affiches dans le terminal", 26);
    sub.setFillColor(sf::Color::White);
    sub.setPosition(
        {window.getSize().x / 2.0f - sub.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 10});
    window.draw(sub);

    sf::Text esc(font, "Appuyez sur Echap pour revenir au menu", 22);
    esc.setFillColor(sf::Color(180, 180, 180));
    esc.setPosition(
        {window.getSize().x / 2.0f - esc.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f + 40});
    window.draw(esc);
    return;
  }

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

  // Bot vs Bot: display average times
  if (ui_state == UIState::PLAYING_BOT) {
    char buf[128];
    snprintf(buf, sizeof(buf), "Temps moyen Bot1 (Noir): %.2f ms",
             state.black_avg_time_ms);
    sf::Text t1(font, buf, 22);
    t1.setFillColor(sf::Color::Cyan);
    t1.setPosition(
        {window.getSize().x / 2.0f - t1.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 40});
    window.draw(t1);

    snprintf(buf, sizeof(buf), "Temps moyen Bot2 (Blanc): %.2f ms",
             state.white_avg_time_ms);
    sf::Text t2(font, buf, 22);
    t2.setFillColor(sf::Color::Cyan);
    t2.setPosition(
        {window.getSize().x / 2.0f - t2.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 10});
    window.draw(t2);
  }

  // Rejouer Button
  sf::RectangleShape btn(sf::Vector2f(200, 60));
  btn.setFillColor(sf::Color(100, 100, 100));
  btn.setPosition(
      {window.getSize().x / 2.0f - 100, window.getSize().y / 2.0f + 40});
  window.draw(btn);

  sf::Text replay_txt(font, "Rejouer", 30);
  replay_txt.setFillColor(sf::Color::White);
  replay_txt.setPosition(
      {window.getSize().x / 2.0f - replay_txt.getGlobalBounds().size.x / 2.0f,
       window.getSize().y / 2.0f + 50});
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

  // Suggestion button — visible in MULTI and in SOLO on human's turn (BLACK)
  bool show_btn = !state.game_over &&
                  ((current_state == UIState::PLAYING_MULTI) ||
                   (current_state == UIState::PLAYING_SOLO &&
                    state.current_player == Player::BLACK));
  if (show_btn) {
    sf::RectangleShape btn(sf::Vector2f(230.0f, 35.0f));
    btn.setFillColor(suggestion_shown ? sf::Color(60, 140, 60) : sf::Color(60, 90, 160));
    btn.setPosition({hist_x + 10.0f, 50.0f});
    window.draw(btn);

    sf::Text btn_txt(font, suggestion_shown ? "Suggestion active" : "Suggestion", 19);
    btn_txt.setFillColor(sf::Color::White);
    btn_txt.setPosition(
        {hist_x + 10.0f + 115.0f - btn_txt.getGlobalBounds().size.x / 2.0f,
         57.0f});
    window.draw(btn_txt);
  }

  // Render recent moves (last 23)
  size_t display_count = 23;
  size_t start_idx = 0;
  if (state.move_history.size() > display_count) {
    start_idx = state.move_history.size() - display_count;
  }

  float y_offset = 100.0f;
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

void GameUI::update_capture_anims(float dt) {
  for (auto &a : capture_anims)
    a.timer -= dt;
  capture_anims.erase(
    std::remove_if(capture_anims.begin(), capture_anims.end(),
      [](const CaptureAnim &a) { return a.timer <= 0.0f; }),
    capture_anims.end());
}

void GameUI::draw_capture_anims() {
  float arm = CELL_SIZE * 0.38f;
  for (const auto &a : capture_anims) {
    float cx = MARGIN + a.col * CELL_SIZE;
    float cy = MARGIN + a.row * CELL_SIZE;
    sf::Color color(220, 50, 50, (uint8_t)(a.timer * 255));

    sf::RectangleShape bar1(sf::Vector2f(arm * 2.0f, 4.0f));
    bar1.setOrigin(sf::Vector2f(arm, 2.0f));
    bar1.setPosition({cx, cy});
    bar1.setFillColor(color);
    bar1.setRotation(sf::degrees(45));
    window.draw(bar1);

    sf::RectangleShape bar2(sf::Vector2f(arm * 2.0f, 4.0f));
    bar2.setOrigin(sf::Vector2f(arm, 2.0f));
    bar2.setPosition({cx, cy});
    bar2.setFillColor(color);
    bar2.setRotation(sf::degrees(-45));
    window.draw(bar2);
  }
}

void GameUI::draw_best_move(const GameState &state) {
  if (!suggestion_shown)
    return;
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
