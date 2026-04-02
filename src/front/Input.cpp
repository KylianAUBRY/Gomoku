// ─────────────────────────────────────────────────────────────────────────────
// Input.cpp — Contrôleur 2D : logique d'entrée et règles de fin de partie
//
// Ce fichier contient tout ce qui modifie GameState en mode 2D :
//   - Navigation dans le menu
//   - Placement de pierres (humain)
//   - Tour IA avec cooldown
//   - Bot vs Bot
//   - Benchmark 4 parties
//   - Règle endgame capture (apply_win_check)
// ─────────────────────────────────────────────────────────────────────────────

#include "Input.hpp"
#include "../engine/Rules.hpp"
#include <chrono>
#include <climits>
#include <cstdio>

// Conversion Player → Cell (enum du moteur)
static Cell playerToCell(Player p) {
  return (p == Player::BLACK) ? BLACK : WHITE;
}

namespace Input {

// ─────────────────────────────────────────────────────────────────────────────
// État interne du module (static = invisible hors de ce .cpp)
//
// s_ai_pending : true = le joueur humain vient de poser une pierre, le timer
//   de 500ms avant le coup IA est en cours.
// s_ai_pending_since : point de départ du timer (high_resolution_clock).
// ─────────────────────────────────────────────────────────────────────────────
static bool s_ai_pending = false;
static std::chrono::high_resolution_clock::time_point s_ai_pending_since;

// ─────────────────────────────────────────────────────────────────────────────
// État pour la règle endgame capture (sujet p.3)
//
// Principe : un alignement de 5 ne gagne que si l'adversaire ne peut pas
// immédiatement capturer une paire appartenant à cet alignement.
// Si c'est possible, on donne UNE seule chance à l'adversaire.
//
// s_pending_alignment_win : true = un joueur a aligné 5, mais l'adversaire
//   peut tenter une capture. On attend son prochain coup.
// s_pending_winner : qui a fait l'alignement en attente.
// s_pending_count  : nombre de pendigs successifs dans la séquence courante.
//   = 1 → premier pending (normal).
//   ≥ 2 → double-pending : just_played a répondu et a lui aussi 5 → il gagne.
// ─────────────────────────────────────────────────────────────────────────────
static bool   s_pending_alignment_win = false;
static Player s_pending_winner        = Player::NONE;
static int    s_pending_count         = 0;
// Gagnant réel : positionné par apply_win_check dès que state.game_over = true.
// Évite la re-dérivation ambiguë dans draw_game_over quand les deux joueurs
// ont 5 pierres simultanément sur le plateau (cas pending + réponse alignement).
static Player s_game_winner           = Player::NONE;

// ─────────────────────────────────────────────────────────────────────────────
// can_opponent_capture_from_alignment
//
// Question : l'adversaire (opp) peut-il, en jouant UN coup, capturer une paire
// de pierres du gagnant (winner) où au moins une pierre appartient à son
// alignement de 5+ ?
//
// Algorithme :
//   1. Marquer toutes les cases appartenant à un run ≥ 5 de "winner".
//   2. Pour chaque case vide du plateau, dans les 8 directions :
//      Chercher le pattern [vide](case testée) - [winner] - [winner] - [opp existant]
//      Si au moins une des deux pierres winner est dans l'alignement → return true.
//
// Complexité : O(19×19 × 8) ≈ 2888 opérations — négligeable.
// ─────────────────────────────────────────────────────────────────────────────
static bool can_opponent_capture_from_alignment(const GameState &state,
                                                Player winner, Player opp) {
    Cell winner_cell = (winner == Player::BLACK) ? BLACK : WHITE;
    Cell opp_cell    = (opp    == Player::BLACK) ? BLACK : WHITE;

    // Étape 1 : marquer toutes les cellules dans un alignement de ≥ 5
    bool in_alignment[19][19] = {};
    const int adirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};

    for (int y = 0; y < 19; ++y) {
        for (int x = 0; x < 19; ++x) {
            if (state.board.get(y, x) != winner_cell) continue;
            for (auto &d : adirs) {
                int dx = d[0], dy = d[1];
                // On ne démarre un run que depuis son premier élément
                // (évite de compter le même run plusieurs fois)
                if (Rules::in_bounds(x - dx, y - dy) &&
                    state.board.get(y - dy, x - dx) == winner_cell) continue;
                int len = 0, cx = x, cy = y;
                while (Rules::in_bounds(cx, cy) &&
                       state.board.get(cy, cx) == winner_cell) {
                    ++len; cx += dx; cy += dy;
                }
                if (len < 5) continue;
                // Marquer les len cases du run
                cx = x; cy = y;
                for (int i = 0; i < len; ++i) {
                    in_alignment[cy][cx] = true;
                    cx += dx; cy += dy;
                }
            }
        }
    }

    // Étape 2 : chercher le pattern de capture pour opp
    // Pattern (opp joue à P) : P - winner - winner - opp_existant
    // Les deux pierres winner doivent être adjacentes et encadrées.
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

// ─────────────────────────────────────────────────────────────────────────────
// apply_win_check — vérification de victoire avec règle endgame capture
//
// Appelée après CHAQUE placement de pierre (humain ou IA).
// Note : à ce point, current_player a DÉJÀ été inversé par place_stone().
//   → "qui vient de jouer" = NOT current_player = l'autre.
//
// Cas 1 — Un pending existe (l'adversaire a eu sa chance) :
//   1a. Victoire par capture de just_played → toujours immédiate, prioritaire.
//   1b. L'alignement du pending_winner est intact :
//       - Double-pending (s_pending_count ≥ 2) ET just_played a aussi 5
//         → just_played gagne ("en position de victoire à son tour").
//       - Sinon → pending_winner gagne normalement.
//   1c. L'alignement a été brisé → flux normal pour just_played.
//
// Cas 2 — Pas de pending :
//   a) Victoire par capture : toujours immédiate (pas de règle endgame).
//   b) Victoire par alignement : seulement si l'adversaire ne peut pas capturer.
//      Sinon → pending, l'adversaire a une chance.
//
// Retourne true si la partie est terminée (state.game_over positionné).
// ─────────────────────────────────────────────────────────────────────────────
static bool apply_win_check(GameState &state) {
    // Qui vient de jouer ? place_stone() a déjà switché current_player.
    Player just_played = (state.current_player == Player::BLACK)
                             ? Player::WHITE : Player::BLACK;
    Player next_player = state.current_player; // Qui joue maintenant

    if (s_pending_alignment_win) {
        // 1a. Victoire par capture : toujours immédiate, même en pending.
        if (Rules::check_win_by_capture(state, just_played)) {
            state.game_over            = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            s_game_winner              = just_played;
            s_pending_alignment_win    = false;
            s_pending_winner           = Player::NONE;
            s_pending_count            = 0;
            return true;
        }

        // 1b. just_played avait une chance de briser l'alignement de s_pending_winner.
        if (Rules::check_win_condition(state, s_pending_winner)) {
            // Double-pending : just_played a répondu en alignant 5 à son tour → il gagne.
            // Règle : "si le joueur se retrouve en position de victoire à son tour, il a gagné."
            if (s_pending_count >= 2 && Rules::check_win_condition(state, just_played)) {
                state.game_over            = true;
                state.best_move_suggestion = {-1, -1, 0, 0};
                s_game_winner              = just_played;
                s_pending_alignment_win    = false;
                s_pending_winner           = Player::NONE;
                s_pending_count            = 0;
                return true;
            }
            // L'alignement est intact → le pending_winner gagne normalement.
            state.game_over            = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            s_game_winner              = s_pending_winner;
            s_pending_alignment_win    = false;
            s_pending_winner           = Player::NONE;
            s_pending_count            = 0;
            return true;
        }

        // 1c. L'alignement a été brisé. Annuler le pending et vérifier
        // si just_played a lui-même une victoire (capture ou alignment).
        s_pending_alignment_win = false;
        s_pending_winner        = Player::NONE;
        s_pending_count         = 0;

        if (Rules::check_win_by_capture(state, just_played)) {
            state.game_over            = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            s_game_winner              = just_played;
            return true;
        }
        if (Rules::check_win_condition(state, just_played)) {
            if (!can_opponent_capture_from_alignment(state, just_played, next_player)) {
                state.game_over            = true;
                state.best_move_suggestion = {-1, -1, 0, 0};
                s_game_winner              = just_played;
                return true;
            }
            // L'adversaire peut encore tenter de briser ce nouvel alignement.
            // s_pending_count ≥ 2 : marque un double-pending (ou au-delà).
            s_pending_alignment_win = true;
            s_pending_winner        = just_played;
            s_pending_count         = 2;
        }
        return false;
    }

    // Cas 2 — Pas de pending — flux normal.
    // Victoire par capture : immédiate, sans exception endgame.
    if (Rules::check_win_by_capture(state, just_played)) {
        state.game_over            = true;
        state.best_move_suggestion = {-1, -1, 0, 0};
        s_game_winner              = just_played;
        return true;
    }

    // Victoire par alignement avec règle endgame.
    if (Rules::check_win_condition(state, just_played)) {
        if (!can_opponent_capture_from_alignment(state, just_played, next_player)) {
            // Personne ne peut briser l'alignement → victoire immédiate.
            state.game_over            = true;
            state.best_move_suggestion = {-1, -1, 0, 0};
            s_game_winner              = just_played;
            return true;
        }
        // L'adversaire peut tenter de briser → il a exactement un tour.
        s_pending_alignment_win = true;
        s_pending_winner        = just_played;
        s_pending_count         = 1;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_menu_input — navigation dans le menu principal
//
// SFML 3 utilise event.getIf<T>() (variant) au lieu de event.type == T.
// Flèches haut/bas : navigation cyclique modulo 4.
// Entrée : transition vers l'UIState correspondant.
// Souris : survol → highlight, clic → transition directe.
// ─────────────────────────────────────────────────────────────────────────────
void handle_menu_input(const sf::Event &event, UIState &current_state,
                       int &menu_selection, const sf::RenderWindow &window) {
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Up) {
      menu_selection = (menu_selection + 3) % 4; // +3 mod 4 = -1 mod 4 (montée)
    } else if (keyPressed->scancode == sf::Keyboard::Scancode::Down) {
      menu_selection = (menu_selection + 1) % 4;
    } else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
      if (menu_selection == 0)      current_state = UIState::PLAYING_SOLO;
      else if (menu_selection == 1) current_state = UIState::PLAYING_MULTI;
      else if (menu_selection == 2) current_state = UIState::PLAYING_BOT;
      else                          current_state = UIState::PLAYING_BENCHMARK;
    }
  }

  // Survol souris → met à jour la sélection visuelle
  if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
    float mx = mouseMoved->position.x;
    float my = mouseMoved->position.y;
    float center_x = window.getSize().x / 2.0f;

    // Zones de hit-test pour chaque option du menu
    sf::FloatRect solo_rect ({center_x - 150, 280}, {300, 50});
    sf::FloatRect multi_rect({center_x - 200, 370}, {400, 50});
    sf::FloatRect bot_rect  ({center_x - 100, 460}, {200, 50});
    sf::FloatRect bench_rect({center_x - 160, 550}, {320, 50});

    if (solo_rect.contains ({mx, my})) menu_selection = 0;
    else if (multi_rect.contains({mx, my})) menu_selection = 1;
    else if (bot_rect.contains  ({mx, my})) menu_selection = 2;
    else if (bench_rect.contains({mx, my})) menu_selection = 3;
  }

  // Clic gauche → transition directe
  if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouseButton->button == sf::Mouse::Button::Left) {
      float mx = mouseButton->position.x;
      float my = mouseButton->position.y;
      float center_x = window.getSize().x / 2.0f;

      sf::FloatRect solo_rect ({center_x - 150, 280}, {300, 50});
      sf::FloatRect multi_rect({center_x - 200, 370}, {400, 50});
      sf::FloatRect bot_rect  ({center_x - 100, 460}, {200, 50});
      sf::FloatRect bench_rect({center_x - 160, 550}, {320, 50});

      if (solo_rect.contains ({mx, my})) current_state = UIState::PLAYING_SOLO;
      else if (multi_rect.contains({mx, my})) current_state = UIState::PLAYING_MULTI;
      else if (bot_rect.contains  ({mx, my})) current_state = UIState::PLAYING_BOT;
      else if (bench_rect.contains({mx, my})) current_state = UIState::PLAYING_BENCHMARK;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// update_best_move_suggestion — calcule et stocke le meilleur coup pour l'UI
//
// Appelée quand le joueur clique sur "Suggestion".
// getBestMove() appelle l'IA (minimax) et retourne le meilleur coup.
// Le résultat est stocké dans state.best_move_suggestion (lu par draw_best_move).
// ─────────────────────────────────────────────────────────────────────────────
static void update_best_move_suggestion(GameState &state, Gomoku &gomoku) {
  if (state.game_over) {
    state.best_move_suggestion = {-1, -1, 0, 0};
    return;
  }
  Cell current_cell = playerToCell(state.current_player);
  state.best_move_suggestion = gomoku.getBestMove(state.board, current_cell);
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_game_input — gestion des événements en mode jeu
//
// Echap : retour au menu + reset complet (état, pending, suggestion, AI).
//
// Clic gauche :
//   - Sur l'overlay game_over → bouton "Rejouer" (reset).
//   - Sur le bouton suggestion → calcule getBestMove(), active suggestion_shown.
//   - Sur le plateau :
//       Conversion pixel → grille : grid_x = (mx - MARGIN + CELL_SIZE/2) / CELL_SIZE
//       Rules::is_valid_move() → place_stone() → apply_win_check()
//       Si pas game_over en Solo → démarrer le cooldown IA 500ms.
// ─────────────────────────────────────────────────────────────────────────────
void handle_game_input(const sf::Event &event, UIState &current_state,
                       GameState &state, const sf::RenderWindow &window,
                       Gomoku &gomoku, bool &suggestion_shown) {
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
      current_state = UIState::MAIN_MENU;
      state.reset();
      suggestion_shown = false;
      s_ai_pending            = false;
      s_pending_alignment_win = false;
      s_pending_winner        = Player::NONE;
      s_pending_count         = 0;
      s_game_winner           = Player::NONE;
    }
  }

  if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mouseButton->button == sf::Mouse::Button::Left) {
      float mx = mouseButton->position.x;
      float my = mouseButton->position.y;

      if (state.game_over) {
        // En Benchmark, pas de bouton Rejouer
        if (current_state == UIState::PLAYING_BENCHMARK) return;

        // Zone du bouton "Rejouer" (identique à draw_game_over dans GameUI.cpp)
        sf::FloatRect replay_btn(
            {window.getSize().x / 2.0f - 100, window.getSize().y / 2.0f + 40},
            {200, 60});
        if (replay_btn.contains({mx, my})) {
          state.reset();
          suggestion_shown = false;
          s_ai_pending            = false;
          s_pending_alignment_win = false;
          s_pending_winner        = Player::NONE;
          s_pending_count         = 0;
          s_game_winner           = Player::NONE;
        }
        return;
      }

      // En Solo, seul le joueur humain (BLACK) peut cliquer
      if (current_state == UIState::PLAYING_SOLO &&
          state.current_player != Player::BLACK) return;

      // Bot vs Bot et Benchmark : clics humains ignorés
      if (current_state == UIState::PLAYING_BOT ||
          current_state == UIState::PLAYING_BENCHMARK) return;

      // Vérifier si le clic est sur le bouton Suggestion (panneau historique)
      bool show_btn = (current_state == UIState::PLAYING_MULTI) ||
                      (current_state == UIState::PLAYING_SOLO &&
                       state.current_player == Player::BLACK);
      if (show_btn) {
        // Zone identique à celle dessinée dans draw_history (GameUI.cpp)
        float hist_x = (BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN;
        sf::FloatRect btn_rect({hist_x + 10.0f, 50.0f}, {230.0f, 35.0f});
        if (btn_rect.contains({mx, my})) {
          update_best_move_suggestion(state, gomoku);
          suggestion_shown = true;
          return;
        }
      }

      // Conversion pixel → case de grille
      // Formule : diviser par CELL_SIZE après avoir retiré la marge et ajouté
      // un demi-CELL_SIZE pour que le clic soit au centre, pas en coin haut-gauche.
      int grid_x = (mx - MARGIN + CELL_SIZE / 2) / CELL_SIZE;
      int grid_y = (my - MARGIN + CELL_SIZE / 2) / CELL_SIZE;

      if (Rules::is_valid_move(state, grid_x, grid_y)) {
        if (state.place_stone(grid_x, grid_y)) {
          suggestion_shown = false;

          // Vérification de victoire avec règle endgame capture
          if (apply_win_check(state)) return;

          // Démarrer le timer de cooldown IA (500ms avant que l'IA réponde)
          if (current_state == UIState::PLAYING_SOLO && !state.game_over) {
            s_ai_pending = true;
            s_ai_pending_since = std::chrono::high_resolution_clock::now();
          }
        }
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_bot_vs_bot — un coup par frame en mode Bot vs Bot
//
// Appelé à chaque frame dans process_events (pas de cooldown → vitesse max).
// Bot 1 (Noir) utilise getBestMove2, Bot 2 (Blanc) utilise getBestMove.
// Les temps moyens sont mis à jour avec une moyenne mobile incrémentale :
//   moy_n = moy_{n-1} + (val - moy_{n-1}) / n
// Cette formule évite l'overflow et ne nécessite pas de stocker tous les temps.
// ─────────────────────────────────────────────────────────────────────────────
static void handle_bot_vs_bot(GameState &state, Gomoku &gomoku) {
  if (state.game_over) return;

  Cell current_cell = playerToCell(state.current_player);
  Move bot_move;

  // Noir → getBestMove2, Blanc → getBestMove
  if (state.current_player == Player::BLACK)
    bot_move = gomoku.getBestMove2(state.board, current_cell);
  else
    bot_move = gomoku.getBestMove(state.board, current_cell);

  double elapsed_ms = (double)bot_move.computeTimeMs;

  // Mise à jour de la moyenne mobile
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

// ─────────────────────────────────────────────────────────────────────────────
// handle_benchmark — 4 parties croisées getBestMove vs getBestMove2
//
// Structure :
//   Parties 1-2 : getBestMove = Noir, getBestMove2 = Blanc
//   Parties 3-4 : getBestMove2 = Noir, getBestMove = Blanc
// → Chaque algo joue 2 fois Noir et 2 fois Blanc → résultats équilibrés.
//
// BenchStats est une struct locale static (persist entre appels, reset entre runs).
// On joue UN coup par appel (appelé à chaque frame) → pas de freeze de l'UI.
// ─────────────────────────────────────────────────────────────────────────────
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
    int    current_game   = 0;
    bool   done           = false;

    double bm1_total_time  = 0.0;
    int    bm1_total_moves = 0;
    int    bm1_wins        = 0;
    double bm1_game_times[4] = {};

    double bm2_total_time  = 0.0;
    int    bm2_total_moves = 0;
    int    bm2_wins        = 0;
    double bm2_game_times[4] = {};

    double cur_bm1_time = 0.0;
    double cur_bm2_time = 0.0;

    void reset() { *this = BenchStats{}; }
  };
  // static → persist entre frames, conserve l'état entre les appels
  static BenchStats bench;

  if (bench.current_game == 0) {
    bench.reset();
    bench.current_game = 1;
    clearTTv2();
    clearTTv();
    printf("[Benchmark] Démarrage — 4 parties en cours...\n");
    fflush(stdout);
  }

  if (bench.done) return;

  // ── Fin de partie : enregistrer stats et passer à la suivante ──────────
  if (state.game_over) {
    int gi = bench.current_game - 1;
    bench.bm1_game_times[gi] = bench.cur_bm1_time;
    bench.bm2_game_times[gi] = bench.cur_bm2_time;

    // Parties 1-2 : bm1=Noir → black_wins == bm1 gagne
    // Parties 3-4 : bm2=Noir → black_wins == bm2 gagne
    bool black_wins   = Rules::check_win_condition(state, Player::BLACK) ||
                        Rules::check_win_by_capture(state, Player::BLACK);
    bool bm1_is_black = (bench.current_game <= 2);

    if (black_wins == bm1_is_black) bench.bm1_wins++;
    else                            bench.bm2_wins++;

    printf("[Benchmark] Partie %d terminee — %s gagne\n",
           bench.current_game,
           (black_wins == bm1_is_black) ? "getBestMove" : "getBestMove2");
    fflush(stdout);

    bench.cur_bm1_time = 0.0;
    bench.cur_bm2_time = 0.0;

    if (bench.current_game == 4) {
      bench.done = true;
      state.benchmark_game = 5; // Signal "terminé" pour le HUD
      print_benchmark_summary(bench.bm1_total_time, bench.bm1_total_moves,
                               bench.bm1_wins, bench.bm2_total_time,
                               bench.bm2_total_moves, bench.bm2_wins,
                               bench.bm1_game_times, bench.bm2_game_times);
      bench.current_game = 0; // Prêt pour un prochain run éventuel
      return;
    }

    bench.current_game++;
    state.reset();
    s_pending_alignment_win = false;
    s_pending_winner        = Player::NONE;
    s_pending_count         = 0;
    s_game_winner           = Player::NONE;
    state.benchmark_game = bench.current_game;
    return;
  }

  // ── Jouer UN coup ──────────────────────────────────────────────────────
  bool bm1_is_black = (bench.current_game <= 2);
  // bm1_plays = true si c'est le tour de getBestMove
  bool bm1_plays    = bm1_is_black
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

  // Mise à jour HUD (slot noir = bm1, slot blanc = bm2, peu importe qui joue)
  state.black_avg_time_ms = bench.bm1_total_moves > 0
                                ? bench.bm1_total_time / bench.bm1_total_moves
                                : 0.0;
  state.white_avg_time_ms = bench.bm2_total_moves > 0
                                ? bench.bm2_total_time / bench.bm2_total_moves
                                : 0.0;

  state.place_stone(bot_move.col, bot_move.row);
  apply_win_check(state);
}

// ─────────────────────────────────────────────────────────────────────────────
// process_events — point d'entrée principal, appelé chaque frame
//
// Ordre des traitements :
//   1. Drain de la queue d'événements SFML (pollEvent) → dispatch menu/jeu
//   2. Bot vs Bot si actif (un coup/frame, pas de cooldown)
//   3. Benchmark si actif
//   4. Cooldown IA 500ms (Solo) : fire si le délai est écoulé
//
// Le cooldown IA 500ms est intentionnel : laisser le joueur voir son coup
// avant que l'IA réponde, améliore l'expérience de jeu.
// ─────────────────────────────────────────────────────────────────────────────
void process_events(sf::RenderWindow &window, UIState &current_state,
                    int &menu_selection, GameState &state, Gomoku &gomoku,
                    bool &suggestion_shown) {
  // Drain de la queue d'événements (événements accumulés depuis la frame précédente)
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

  // Traitements non-événementiels (exécutés chaque frame même sans événement)
  if (current_state == UIState::PLAYING_BOT && !state.game_over) {
    handle_bot_vs_bot(state, gomoku);
  }

  if (current_state == UIState::PLAYING_BENCHMARK && state.benchmark_game != 5) {
    handle_benchmark(state, gomoku);
  }

  // Cooldown IA : si 500ms se sont écoulées depuis le coup humain, l'IA joue
  if (current_state == UIState::PLAYING_SOLO && s_ai_pending && !state.game_over) {
    auto now = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(now - s_ai_pending_since).count();
    if (elapsed_ms >= 500.0) {
      s_ai_pending = false;
      Cell ai_cell = playerToCell(state.current_player);

      Move ai_move = gomoku.getBestMove(state.board, ai_cell);

      // Stocker le temps pour l'affichage HUD (exigence sujet p.6)
      state.last_ai_move_time_ms = (double)ai_move.computeTimeMs;
      // Moyenne mobile incrémentale du temps IA (slot white réutilisé)
      state.white_move_count++;
      state.white_avg_time_ms +=
          (state.last_ai_move_time_ms - state.white_avg_time_ms) / state.white_move_count;

      state.place_stone(ai_move.col, ai_move.row);
      apply_win_check(state);
    }
  }
}
Player get_winner() {
    return s_game_winner;
}

} // namespace Input
