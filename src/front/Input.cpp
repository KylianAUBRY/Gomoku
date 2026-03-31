#include "Input.hpp"
#include "../engine/Rules.hpp"
#include <chrono>
#include <climits>
#include <cstdio>

// Helper: convert Player enum to Cell enum used by the engine
static Cell playerToCell(Player p) {
  return (p == Player::BLACK) ? BLACK : WHITE;
}

namespace Input {

void handle_menu_input(const sf::Event &event, UIState &current_state,
                       int &menu_selection, const sf::RenderWindow &window) {
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Up) {
      menu_selection = (menu_selection + 3) % 4;
    } else if (keyPressed->scancode == sf::Keyboard::Scancode::Down) {
      menu_selection = (menu_selection + 1) % 4;
    } else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
      if (menu_selection == 0)
        current_state = UIState::PLAYING_SOLO;
      else if (menu_selection == 1)
        current_state = UIState::PLAYING_MULTI;
      else if (menu_selection == 2)
        current_state = UIState::PLAYING_BOT;
      else
        current_state = UIState::PLAYING_BENCHMARK;
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
    sf::FloatRect bench_rect({center_x - 160, 550}, {320, 50});

    if (solo_rect.contains({mx, my}))
      menu_selection = 0;
    else if (multi_rect.contains({mx, my}))
      menu_selection = 1;
    else if (bot_rect.contains({mx, my}))
      menu_selection = 2;
    else if (bench_rect.contains({mx, my}))
      menu_selection = 3;
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
      else if (menu_selection == 3)
        current_state = UIState::PLAYING_BENCHMARK;
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
        // No replay button in benchmark mode
        if (current_state == UIState::PLAYING_BENCHMARK)
          return;

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

      // Ignore clicks in bot modes
      if (current_state == UIState::PLAYING_BOT ||
          current_state == UIState::PLAYING_BENCHMARK)
        return;

      int grid_x = (mx - MARGIN + CELL_SIZE / 2) / CELL_SIZE;
      int grid_y = (my - MARGIN + CELL_SIZE / 2) / CELL_SIZE;

      if (Rules::is_valid_move(state, grid_x, grid_y)) {
        if (state.place_stone(grid_x, grid_y)) {
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

            state.place_stone(ai_move.col, ai_move.row);

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
    bot_move = gomoku.getBestMove2(state.board, current_cell);
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

// ─── Benchmark ────────────────────────────────────────────────────────────────

static void print_benchmark_summary(double bm1_total, int bm1_moves,
                                     int bm1_wins, double bm2_total,
                                     int bm2_moves, int bm2_wins,
                                     double bm1_game_times[4],
                                     double bm2_game_times[4]) {
  double bm1_avg_move = bm1_moves > 0 ? bm1_total / bm1_moves : 0.0;
  double bm2_avg_move = bm2_moves > 0 ? bm2_total / bm2_moves : 0.0;

  double bm1_game_sum = 0.0, bm2_game_sum = 0.0;
  for (int i = 0; i < 4; i++) {
    bm1_game_sum += bm1_game_times[i];
    bm2_game_sum += bm2_game_times[i];
  }
  double bm1_avg_game = bm1_game_sum / 4.0;
  double bm2_avg_game = bm2_game_sum / 4.0;

  printf("\n");
  printf("=============================================================\n");
  printf("        BENCHMARK - RESULTATS (4 parties)                    \n");
  printf("  Parties 1-2 : getBestMove=NOIR   | getBestMove2=BLANC      \n");
  printf("  Parties 3-4 : getBestMove2=NOIR  | getBestMove=BLANC       \n");
  printf("=============================================================\n");
  printf("%-28s %13s %13s\n", "", "getBestMove", "getBestMove2");
  printf("-------------------------------------------------------------\n");
  printf("%-28s %13d %13d\n", "Victoires",             bm1_wins,      bm2_wins);
  printf("%-28s %13d %13d\n", "Defaites",              4 - bm1_wins,  4 - bm2_wins);
  printf("%-28s %12.1f  %12.1f\n", "Tps moyen/coup (ms)",  bm1_avg_move,  bm2_avg_move);
  printf("%-28s %12.1f  %12.1f\n", "Tps moyen/partie (ms)", bm1_avg_game, bm2_avg_game);
  printf("%-28s %12.1f  %12.1f\n", "Temps total (ms)",     bm1_total,     bm2_total);
  printf("=============================================================\n");
  printf("\n");
  fflush(stdout);
}

static void handle_benchmark(GameState &state, Gomoku &gomoku) {
  struct BenchStats {
    int    current_game   = 0;  // 1-4 in progress, 0 = not started/reset
    bool   done           = false;

    double bm1_total_time = 0.0;
    int    bm1_total_moves = 0;
    int    bm1_wins        = 0;
    double bm1_game_times[4] = {};

    double bm2_total_time = 0.0;
    int    bm2_total_moves = 0;
    int    bm2_wins        = 0;
    double bm2_game_times[4] = {};

    double cur_bm1_time = 0.0;
    double cur_bm2_time = 0.0;

    void reset() { *this = BenchStats{}; }
  };
  static BenchStats bench;

  // (Re)start when current_game == 0 (first call or after a previous run)
  if (bench.current_game == 0) {
    bench.reset();
    bench.current_game = 1;
    printf("[Benchmark] Démarrage — 4 parties en cours...\n");
    fflush(stdout);
  }

  if (bench.done)
    return;

  // ── Game just ended: record stats and advance ──────────────────────────────
  if (state.game_over) {
    int gi = bench.current_game - 1;
    bench.bm1_game_times[gi] = bench.cur_bm1_time;
    bench.bm2_game_times[gi] = bench.cur_bm2_time;

    // Determine which algo won
    // Games 1-2 : bm1(getBestMove)=BLACK, bm2(getBestMove2)=WHITE
    // Games 3-4 : bm2(getBestMove2)=BLACK, bm1(getBestMove)=WHITE
    bool black_wins = Rules::check_win_condition(state, Player::BLACK) ||
                      Rules::check_win_by_capture(state, Player::BLACK);
    bool bm1_is_black = (bench.current_game <= 2);

    if (black_wins == bm1_is_black)
      bench.bm1_wins++;
    else
      bench.bm2_wins++;

    printf("[Benchmark] Partie %d terminee — %s gagne\n",
           bench.current_game,
           (black_wins == bm1_is_black) ? "getBestMove" : "getBestMove2");
    fflush(stdout);

    bench.cur_bm1_time = 0.0;
    bench.cur_bm2_time = 0.0;

    if (bench.current_game == 4) {
      bench.done = true;
      state.benchmark_game = 5;
      print_benchmark_summary(bench.bm1_total_time, bench.bm1_total_moves,
                               bench.bm1_wins, bench.bm2_total_time,
                               bench.bm2_total_moves, bench.bm2_wins,
                               bench.bm1_game_times, bench.bm2_game_times);
      // Ready for potential next run
      bench.current_game = 0;
      return;
    }

    bench.current_game++;
    state.reset();
    state.benchmark_game = bench.current_game;
    return;
  }

  // ── Play one move ──────────────────────────────────────────────────────────
  // Games 1-2 : getBestMove plays BLACK, getBestMove2 plays WHITE
  // Games 3-4 : getBestMove2 plays BLACK, getBestMove plays WHITE
  bool bm1_is_black = (bench.current_game <= 2);
  bool bm1_plays    = (bm1_is_black)
                          ? (state.current_player == Player::BLACK)
                          : (state.current_player == Player::WHITE);

  Cell current_cell = playerToCell(state.current_player);

  auto t_start = std::chrono::high_resolution_clock::now();
  Move bot_move = bm1_plays
                      ? gomoku.getBestMove(state.board, current_cell)
                      : gomoku.getBestMove2(state.board, current_cell);
  auto t_end = std::chrono::high_resolution_clock::now();

  double elapsed_ms =
      std::chrono::duration<double, std::milli>(t_end - t_start).count();

  if (bm1_plays) {
    bench.cur_bm1_time   += elapsed_ms;
    bench.bm1_total_time += elapsed_ms;
    bench.bm1_total_moves++;
  } else {
    bench.cur_bm2_time   += elapsed_ms;
    bench.bm2_total_time += elapsed_ms;
    bench.bm2_total_moves++;
  }

  // Update HUD averages for live display (bm1→black slot, bm2→white slot)
  state.black_avg_time_ms = bench.bm1_total_moves > 0
                                ? bench.bm1_total_time / bench.bm1_total_moves
                                : 0.0;
  state.white_avg_time_ms = bench.bm2_total_moves > 0
                                ? bench.bm2_total_time / bench.bm2_total_moves
                                : 0.0;

  state.place_stone(bot_move.col, bot_move.row);

  if (Rules::check_win_condition(state, Player::BLACK) ||
      Rules::check_win_by_capture(state, Player::BLACK) ||
      Rules::check_win_condition(state, Player::WHITE) ||
      Rules::check_win_by_capture(state, Player::WHITE)) {
    state.game_over = true;
    state.best_move_suggestion = {-1, -1, 0, 0};
  }
}

// ──────────────────────────────────────────────────────────────────────────────

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

  if (current_state == UIState::PLAYING_BENCHMARK && state.benchmark_game != 5) {
    handle_benchmark(state, gomoku);
  }
}
} // namespace Input
