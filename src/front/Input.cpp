#include "Input.hpp"
#include "../engine/Rules.hpp"
#include <chrono>
#include <climits>

// Helper: convert Player enum to Cell enum used by the engine
static Cell playerToCell(Player p) {
  return (p == Player::BLACK) ? BLACK : WHITE;
}

namespace Input {

void handle_menu_input(const sf::Event &event, UIState &current_state,
                       int &menu_selection, const sf::RenderWindow &window) {
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Up) {
      menu_selection = (menu_selection + 2) % 3;
    } else if (keyPressed->scancode == sf::Keyboard::Scancode::Down) {
      menu_selection = (menu_selection + 1) % 3;
    } else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
      if (menu_selection == 0)
        current_state = UIState::PLAYING_SOLO;
      else if (menu_selection == 1)
        current_state = UIState::PLAYING_MULTI;
      else
        current_state = UIState::PLAYING_BOT;
    }
  }

  // Mouse hover logic for menu
  if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
    float mx = mouseMoved->position.x;
    float my = mouseMoved->position.y;
    float center_x = window.getSize().x / 2.0f;

    sf::FloatRect solo_rect({center_x - 150, 280}, {300, 50});
    sf::FloatRect multi_rect({center_x - 200, 370}, {400, 50});
    sf::FloatRect bot_rect({center_x - 100, 460}, {200, 50});

    if (solo_rect.contains({mx, my}))
      menu_selection = 0;
    else if (multi_rect.contains({mx, my}))
      menu_selection = 1;
    else if (bot_rect.contains({mx, my}))
      menu_selection = 2;
  }

  // Mouse click logic for menu
  if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouseButton->button == sf::Mouse::Button::Left) {
      if (menu_selection == 0)
        current_state = UIState::PLAYING_SOLO;
      else if (menu_selection == 1)
        current_state = UIState::PLAYING_MULTI;
      else if (menu_selection == 2)
        current_state = UIState::PLAYING_BOT;
    }
  }
}

// Compute and store the best move suggestion for the current player
static void update_best_move_suggestion(GameState &state, Gomoku &gomoku) {
  if (state.game_over) {
    state.best_move_suggestion = {-1, -1, 0, 0};
    return;
  }
  Cell current_cell = playerToCell(state.current_player);
  state.best_move_suggestion = gomoku.getBestMove(state.board, current_cell);
}

void handle_game_input(const sf::Event &event, UIState &current_state,
                       GameState &state, const sf::RenderWindow &window,
                       Gomoku &gomoku) {
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
      current_state = UIState::MAIN_MENU; // Back to Menu
      state.reset();                      // Always reset on exit
    }
  }

  if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouseButton->button == sf::Mouse::Button::Left) {
      float mx = mouseButton->position.x;
      float my = mouseButton->position.y;

      if (state.game_over) {
        // Check replay button click
        sf::FloatRect replay_btn(
            {window.getSize().x / 2.0f - 100, window.getSize().y / 2.0f + 40},
            {200, 60});
        if (replay_btn.contains({mx, my})) {
          state.reset();
        }
        return; // Prevent further interactions if game over
      }

      // In solo mode, only allow the human player (BLACK) to click
      if (current_state == UIState::PLAYING_SOLO &&
          state.current_player != Player::BLACK) {
        return; // It's the AI's turn, ignore human clicks
      }

      int grid_x = (mx - MARGIN + CELL_SIZE / 2) / CELL_SIZE;
      int grid_y = (my - MARGIN + CELL_SIZE / 2) / CELL_SIZE;

      if (Rules::is_valid_move(state, grid_x, grid_y)) {
        // Player current = state.current_player; // Before placement modifies it
        if (state.place_stone(grid_x, grid_y)) {
        //   Rules::process_captures(state, grid_x, grid_y, current);

          // Check win condition after the move
          if (Rules::check_win_condition(state, Player::BLACK) ||
              Rules::check_win_by_capture(state, Player::BLACK) ||
              Rules::check_win_condition(state, Player::WHITE) ||
              Rules::check_win_by_capture(state, Player::WHITE)) {
            state.game_over = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            return;
          }

          // ---- AI TURN (Solo mode only) ----
          if (current_state == UIState::PLAYING_SOLO && !state.game_over) {
            Cell ai_cell = playerToCell(state.current_player);

            auto t_start = std::chrono::high_resolution_clock::now();
            Move ai_move = gomoku.getBestMove(state.board, ai_cell);
            auto t_end = std::chrono::high_resolution_clock::now();

            double elapsed_ms =
                std::chrono::duration<double, std::milli>(t_end - t_start)
                    .count();
            state.last_ai_move_time_ms = elapsed_ms;

            // Place the AI stone
            // Player ai_player = state.current_player;
            state.place_stone(ai_move.col, ai_move.row);
            // Rules::process_captures(state, ai_move.col, ai_move.row, ai_player);

            // Check win after AI move
            if (Rules::check_win_condition(state, Player::BLACK) ||
                Rules::check_win_by_capture(state, Player::BLACK) ||
                Rules::check_win_condition(state, Player::WHITE) ||
                Rules::check_win_by_capture(state, Player::WHITE)) {
              state.game_over = true;
              state.best_move_suggestion = {-1, -1, 0, 0};
              return;
            }
          }

          // Update best move suggestion for the current player
          update_best_move_suggestion(state, gomoku);
        }
      }
    }
  }
}

static void handle_bot_vs_bot(GameState &state, Gomoku &gomoku) {
  if (state.game_over)
    return;

  Cell current_cell = playerToCell(state.current_player);
  Move bot_move;

  auto t_start = std::chrono::high_resolution_clock::now();
  if (state.current_player == Player::BLACK)
    bot_move = gomoku.getBestMove3(state.board, current_cell);
  else
    bot_move = gomoku.getBestMove(state.board, current_cell);
  auto t_end = std::chrono::high_resolution_clock::now();

  double elapsed_ms =
      std::chrono::duration<double, std::milli>(t_end - t_start).count();

  if (state.current_player == Player::BLACK) {
    state.black_move_count++;
    state.black_avg_time_ms +=
        (elapsed_ms - state.black_avg_time_ms) / state.black_move_count;
  } else {
    state.white_move_count++;
    state.white_avg_time_ms +=
        (elapsed_ms - state.white_avg_time_ms) / state.white_move_count;
  }

  state.place_stone(bot_move.col, bot_move.row);

  if (Rules::check_win_condition(state, Player::BLACK) ||
      Rules::check_win_by_capture(state, Player::BLACK) ||
      Rules::check_win_condition(state, Player::WHITE) ||
      Rules::check_win_by_capture(state, Player::WHITE)) {
    state.game_over = true;
    state.best_move_suggestion = {-1, -1, 0, 0};
  }
}

void process_events(sf::RenderWindow &window, UIState &current_state,
                    int &menu_selection, GameState &state, Gomoku &gomoku) {
  while (const std::optional<sf::Event> event = window.pollEvent()) {
    if (event->is<sf::Event::Closed>()) {
      window.close();
    }

    if (current_state == UIState::MAIN_MENU) {
      handle_menu_input(*event, current_state, menu_selection, window);
    } else {
      handle_game_input(*event, current_state, state, window, gomoku);
    }
  }

  if (current_state == UIState::PLAYING_BOT && !state.game_over) {
    handle_bot_vs_bot(state, gomoku);
  }
}
} // namespace Input
