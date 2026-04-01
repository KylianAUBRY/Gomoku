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

// AI cooldown state (solo mode only)
static bool s_ai_pending = false;
static std::chrono::high_resolution_clock::time_point s_ai_pending_since;

// Endgame capture rule: track a "pending" alignment win.
// When a player aligns 5 but the opponent can immediately capture from that line,
// game does not end — the opponent gets exactly one turn to attempt the capture.
static bool   s_pending_alignment_win = false;
static Player s_pending_winner        = Player::NONE;

// Returns true if 'opp' can, in one move, capture a pair of 'winner' stones
// where at least one stone in that pair belongs to winner's 5+ alignment.
// The capture can be along any axis (not necessarily the alignment axis).
static bool can_opponent_capture_from_alignment(const GameState &state,
                                                Player winner, Player opp) {
    Cell winner_cell = (winner == Player::BLACK) ? BLACK : WHITE;
    Cell opp_cell    = (opp    == Player::BLACK) ? BLACK : WHITE;

    // Mark all cells that are part of a winning alignment (run of >= 5).
    bool in_alignment[19][19] = {};
    const int adirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};

    for (int y = 0; y < 19; ++y) {
        for (int x = 0; x < 19; ++x) {
            if (state.board.get(y, x) != winner_cell) continue;
            for (auto &d : adirs) {
                int dx = d[0], dy = d[1];
                // Only start from the beginning of a run.
                if (Rules::in_bounds(x - dx, y - dy) &&
                    state.board.get(y - dy, x - dx) == winner_cell) continue;
                int len = 0, cx = x, cy = y;
                while (Rules::in_bounds(cx, cy) &&
                       state.board.get(cy, cx) == winner_cell) {
                    ++len; cx += dx; cy += dy;
                }
                if (len < 5) continue;
                cx = x; cy = y;
                for (int i = 0; i < len; ++i) {
                    in_alignment[cy][cx] = true;
                    cx += dx; cy += dy;
                }
            }
        }
    }

    // For each empty cell P: can opponent play there and capture a pair
    // that includes at least one alignment cell?
    // Capture pattern (opp plays at P): P - winner - winner - opp_existing
    const int cdirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},
                             {1,1},{-1,-1},{1,-1},{-1,1}};
    for (int y = 0; y < 19; ++y) {
        for (int x = 0; x < 19; ++x) {
            if (state.board.get(y, x) != EMPTY) continue;
            for (auto &cd : cdirs) {
                int dx = cd[0], dy = cd[1];
                int x1 = x + dx,    y1 = y + dy;
                int x2 = x + 2*dx,  y2 = y + 2*dy;
                int x3 = x + 3*dx,  y3 = y + 3*dy;
                if (!Rules::in_bounds(x1, y1) || !Rules::in_bounds(x2, y2) ||
                    !Rules::in_bounds(x3, y3)) continue;
                if (state.board.get(y1, x1) == winner_cell &&
                    state.board.get(y2, x2) == winner_cell &&
                    state.board.get(y3, x3) == opp_cell    &&
                    (in_alignment[y1][x1] || in_alignment[y2][x2]))
                    return true;
            }
        }
    }
    return false;
}

// Unified win check called after every stone placement.
// Implements the endgame capture rule:
//   - Alignment win is only immediate if the opponent cannot capture from it.
//   - If they can, set a "pending" flag and give them one turn.
//   - On that response turn, if the alignment is still intact → pending winner wins.
//   - The first validated alignment always takes priority over a later one.
// Returns true if the game is now over (state.game_over has been set).
static bool apply_win_check(GameState &state) {
    // current_player has already been flipped: who just played?
    Player just_played = (state.current_player == Player::BLACK)
                             ? Player::WHITE : Player::BLACK;
    Player next_player = state.current_player;

    if (s_pending_alignment_win) {
        // just_played had one turn to capture from pending_winner's alignment.
        if (Rules::check_win_condition(state, s_pending_winner)) {
            // Still intact — opponent did not break it — pending winner wins.
            state.game_over = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            s_pending_alignment_win = false;
            s_pending_winner = Player::NONE;
            return true;
        }
        // Alignment broken. Clear pending and run normal checks for just_played.
        s_pending_alignment_win = false;
        s_pending_winner = Player::NONE;

        if (Rules::check_win_by_capture(state, just_played)) {
            state.game_over = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            return true;
        }
        if (Rules::check_win_condition(state, just_played)) {
            if (!can_opponent_capture_from_alignment(state, just_played, next_player)) {
                state.game_over = true;
                state.best_move_suggestion = {-1, -1, 0, 0};
                return true;
            }
            s_pending_alignment_win = true;
            s_pending_winner = just_played;
        }
        return false;
    }

    // No pending. Check capture win (always immediate — no endgame exception).
    if (Rules::check_win_by_capture(state, just_played)) {
        state.game_over = true;
        state.best_move_suggestion = {-1, -1, 0, 0};
        return true;
    }

    // Check alignment win with endgame capture rule.
    if (Rules::check_win_condition(state, just_played)) {
        if (!can_opponent_capture_from_alignment(state, just_played, next_player)) {
            state.game_over = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            return true;
        }
        // Opponent can potentially break it — give them exactly one turn.
        s_pending_alignment_win = true;
        s_pending_winner = just_played;
    }

    return false;
}

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

  // Mouse click logic for menu — only act if click is on a visible option rect
  if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouseButton->button == sf::Mouse::Button::Left) {
      float mx = mouseButton->position.x;
      float my = mouseButton->position.y;
      float center_x = window.getSize().x / 2.0f;

      sf::FloatRect solo_rect({center_x - 150, 280}, {300, 50});
      sf::FloatRect multi_rect({center_x - 200, 370}, {400, 50});
      sf::FloatRect bot_rect({center_x - 100, 460}, {200, 50});
      sf::FloatRect bench_rect({center_x - 160, 550}, {320, 50});

      if (solo_rect.contains({mx, my}))
        current_state = UIState::PLAYING_SOLO;
      else if (multi_rect.contains({mx, my}))
        current_state = UIState::PLAYING_MULTI;
      else if (bot_rect.contains({mx, my}))
        current_state = UIState::PLAYING_BOT;
      else if (bench_rect.contains({mx, my}))
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
                       Gomoku &gomoku, bool &suggestion_shown) {
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
      current_state = UIState::MAIN_MENU;
      state.reset();
      suggestion_shown = false;
      s_ai_pending = false;
      s_pending_alignment_win = false;
      s_pending_winner = Player::NONE;
    }
  }

  if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouseButton->button == sf::Mouse::Button::Left) {
      float mx = mouseButton->position.x;
      float my = mouseButton->position.y;

      if (state.game_over) {
        if (current_state == UIState::PLAYING_BENCHMARK)
          return;

        sf::FloatRect replay_btn(
            {window.getSize().x / 2.0f - 100, window.getSize().y / 2.0f + 40},
            {200, 60});
        if (replay_btn.contains({mx, my})) {
          state.reset();
          suggestion_shown = false;
          s_ai_pending = false;
          s_pending_alignment_win = false;
          s_pending_winner = Player::NONE;
        }
        return;
      }

      // In solo mode, only allow the human player (BLACK) to click
      if (current_state == UIState::PLAYING_SOLO &&
          state.current_player != Player::BLACK) {
        return;
      }

      // Ignore clicks in bot modes
      if (current_state == UIState::PLAYING_BOT ||
          current_state == UIState::PLAYING_BENCHMARK)
        return;

      // Check Suggestion button click (history panel, y=50, h=35)
      bool show_btn = (current_state == UIState::PLAYING_MULTI) ||
                      (current_state == UIState::PLAYING_SOLO &&
                       state.current_player == Player::BLACK);
      if (show_btn) {
        float hist_x = (BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN;
        sf::FloatRect btn_rect({hist_x + 10.0f, 50.0f}, {230.0f, 35.0f});
        if (btn_rect.contains({mx, my})) {
          update_best_move_suggestion(state, gomoku);
          suggestion_shown = true;
          return;
        }
      }

      int grid_x = (mx - MARGIN + CELL_SIZE / 2) / CELL_SIZE;
      int grid_y = (my - MARGIN + CELL_SIZE / 2) / CELL_SIZE;

      if (Rules::is_valid_move(state, grid_x, grid_y)) {
        if (state.place_stone(grid_x, grid_y)) {
          suggestion_shown = false;

          // Check win condition after the move (endgame capture rule applied)
          if (apply_win_check(state))
            return;

          // ---- AI TURN (Solo mode only) — 0.5s cooldown ----
          if (current_state == UIState::PLAYING_SOLO && !state.game_over) {
            s_ai_pending = true;
            s_ai_pending_since = std::chrono::high_resolution_clock::now();
          }
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

  if (state.current_player == Player::BLACK)
    bot_move = gomoku.getBestMove2(state.board, current_cell);
  else
    bot_move = gomoku.getBestMove(state.board, current_cell);

  double elapsed_ms = (double)bot_move.computeTimeMs;

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
  apply_win_check(state);
}

// ─── Benchmark ────────────────────────────────────────────────────────────────

static void print_benchmark_summary(double bm1_total, int bm1_moves, int bm1_wins, double bm2_total, int bm2_moves, int bm2_wins, double bm1_game_times[4], double bm2_game_times[4]) {
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
    clearTTv2();
    clearTTv();
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
    s_pending_alignment_win = false;
    s_pending_winner = Player::NONE;
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

  Move bot_move = bm1_plays
                      ? gomoku.getBestMove(state.board, current_cell)
                      : gomoku.getBestMove2(state.board, current_cell);

  double elapsed_ms = (double)bot_move.computeTimeMs;

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
  apply_win_check(state);
}

// ──────────────────────────────────────────────────────────────────────────────

void process_events(sf::RenderWindow &window, UIState &current_state,
                    int &menu_selection, GameState &state, Gomoku &gomoku,
                    bool &suggestion_shown) {
  while (const std::optional<sf::Event> event = window.pollEvent()) {
    if (event->is<sf::Event::Closed>()) {
      window.close();
    }

    if (current_state == UIState::MAIN_MENU) {
      handle_menu_input(*event, current_state, menu_selection, window);
    } else {
      handle_game_input(*event, current_state, state, window, gomoku, suggestion_shown);
    }
  }

  if (current_state == UIState::PLAYING_BOT && !state.game_over) {
    handle_bot_vs_bot(state, gomoku);
  }

  if (current_state == UIState::PLAYING_BENCHMARK && state.benchmark_game != 5) {
    handle_benchmark(state, gomoku);
  }

  // AI cooldown: fire after 0.5s (solo mode only)
  if (current_state == UIState::PLAYING_SOLO && s_ai_pending && !state.game_over) {
    auto now = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(now - s_ai_pending_since).count();
    if (elapsed_ms >= 500.0) {
      s_ai_pending = false;
      Cell ai_cell = playerToCell(state.current_player);

      Move ai_move = gomoku.getBestMove(state.board, ai_cell);

      state.last_ai_move_time_ms = (double)ai_move.computeTimeMs;

      state.place_stone(ai_move.col, ai_move.row);
      apply_win_check(state);
    }
  }
}
} // namespace Input
