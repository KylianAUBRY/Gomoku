// ─────────────────────────────────────────────────────────────────────────────
// GameUI3D.cpp — Vue 3D Raylib, mode FPS bonus (./Gomoku --fps)
// ─────────────────────────────────────────────────────────────────────────────

// ⚠ ORDRE OBLIGATOIRE : engine avant raylib
// Raylib définit BLACK={0,0,0,255} et WHITE={255,255,255,255} comme macros C.
// L'engine définit BLACK=1, WHITE=2 comme valeurs de l'enum Cell.
// → inclure l'engine EN PREMIER, puis sauvegarder les valeurs dans des
// constantes
//   avant que raylib les écrase.
#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "../engine/Rules.hpp"

// Sauvegarde des valeurs Cell AVANT que raylib définisse ses macros
static constexpr Cell kBlack = BLACK;
static constexpr Cell kWhite = WHITE;
static constexpr Cell kEmpty = EMPTY;

// Maintenant on peut inclure raylib en sécurité
#include "raylib.h"
#include "raymath.h" // Vector3Add, Vector3Scale, Vector3Normalize, etc.
#include "rlgl.h"    // rlDisableDepthTest, rlDrawRenderBatchActive, etc.

#include "GameUI3D.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers pour contourner les macros BLACK/WHITE de Raylib
//
// On ne peut plus écrire Player::BLACK dans ce fichier car la macro BLACK
// l'expanderait en CLITERAL(Color){0,0,0,255}.
// Solution : wrapper inline qui caste l'int sous-jacent.
// ─────────────────────────────────────────────────────────────────────────────
static inline bool isBlackPlayer(Player p) { return (int)p == 1; }

static Cell playerToCell(Player p) {
  return isBlackPlayer(p) ? kBlack : kWhite;
}

static inline Player makePlayerBlack() { return static_cast<Player>(1); }
static inline Player makePlayerWhite() { return static_cast<Player>(2); }
static inline Player makePlayerNone() { return static_cast<Player>(0); }

// ─────────────────────────────────────────────────────────────────────────────
// can_opponent_capture_from_alignment — identique à la version Input.cpp (2D)
//
// Renvoie true si opp peut, en jouant UN coup, capturer une paire de winner
// dont au moins une pierre appartient à l'alignement de 5+ en cours.
// ─────────────────────────────────────────────────────────────────────────────
static bool can_opponent_capture_from_alignment(const GameState &state,
                                                Player winner, Player opp) {
  Cell winner_cell = isBlackPlayer(winner) ? kBlack : kWhite;
  Cell opp_cell = isBlackPlayer(opp) ? kBlack : kWhite;

  bool in_alignment[19][19] = {};
  const int adirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

  for (int y = 0; y < 19; ++y) {
    for (int x = 0; x < 19; ++x) {
      if (state.board.get(y, x) != winner_cell)
        continue;
      for (auto &d : adirs) {
        int dx = d[0], dy = d[1];
        if (Rules::in_bounds(x - dx, y - dy) &&
            state.board.get(y - dy, x - dx) == winner_cell)
          continue;
        int len = 0, cx = x, cy = y;
        while (Rules::in_bounds(cx, cy) &&
               state.board.get(cy, cx) == winner_cell) {
          ++len;
          cx += dx;
          cy += dy;
        }
        if (len < 5)
          continue;
        cx = x;
        cy = y;
        for (int i = 0; i < len; ++i) {
          in_alignment[cy][cx] = true;
          cx += dx;
          cy += dy;
        }
      }
    }
  }

  const int cdirs[8][2] = {{1, 0}, {-1, 0},  {0, 1},  {0, -1},
                           {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
  for (int y = 0; y < 19; ++y) {
    for (int x = 0; x < 19; ++x) {
      if (state.board.get(y, x) != kEmpty)
        continue;
      for (auto &cd : cdirs) {
        int dx = cd[0], dy = cd[1];
        int x1 = x + dx, y1 = y + dy;
        int x2 = x + 2 * dx, y2 = y + 2 * dy;
        int x3 = x + 3 * dx, y3 = y + 3 * dy;
        if (!Rules::in_bounds(x1, y1) || !Rules::in_bounds(x2, y2) ||
            !Rules::in_bounds(x3, y3))
          continue;
        if (state.board.get(y1, x1) == winner_cell &&
            state.board.get(y2, x2) == winner_cell &&
            state.board.get(y3, x3) == opp_cell &&
            (in_alignment[y1][x1] || in_alignment[y2][x2]))
          return true;
      }
    }
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructeur
//
// InitWindow : crée la fenêtre et le contexte OpenGL (Raylib).
// ClearWindowState(FLAG_WINDOW_RESIZABLE) : fenêtre non redimensionnable,
// layout fixe comme en 2D.
// ─────────────────────────────────────────────────────────────────────────────
GameUI3D::GameUI3D()
    : running(true), menu_selection(0), current_state(UI3DState::MAIN_MENU),
      cam_yaw(0.0f), cam_pitch(0.0f), hovered_row(-1), hovered_col(-1),
      has_hover(false), vm_bob_time(0.0f), vm_recoil(0.0f),
      bolt_anim_timer_(0.0f), ai_pending_(false), ai_highlight_row_(-1),
      ai_highlight_col_(-1), ai_highlight_timer_(0.0f),
      pending_alignment_win_(false), pending_winner_(makePlayerNone()),
      ai_thread_active_(false), ai_result_ready_(false),
      ai_result_({-1, -1, 0, 0}) {
  InitWindow(1280, 720, "Gomoku FPS");
  ClearWindowState(FLAG_WINDOW_RESIZABLE);
  SetExitKey(KEY_NULL); // Désactive le comportement Raylib par défaut (ESC =
                        // WindowShouldClose)
  SetTargetFPS(60);
  init_camera();
}

GameUI3D::~GameUI3D() {
  CloseWindow(); // Libère le contexte OpenGL Raylib
}

// ─────────────────────────────────────────────────────────────────────────────
// init_camera — position fixe, orientée vers le centre du goban
//
// Système de coordonnées monde :
//   - Goban : coins à (0,0,0) et (18,18,0), centre à (9,9,0)
//   - Caméra : position (9, 9, 22) → directement au-dessus du centre, Z=22
//   - camera.up = (0,1,0) : Y est "le haut" de l'image
//   - fovy = 55° : champ de vision vertical confortable pour voir tout le goban
//   - CAMERA_PERSPECTIVE : projection perspective standard (points lointains
//   plus petits)
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::init_camera() {
  camera = {};
  camera.position = {9.0f, 9.0f, 22.0f}; // Surplombe le centre du goban
  camera.target = {9.0f, 9.0f, 0.0f};    // Regarde vers le centre du goban
  camera.up = {0.0f, 1.0f, 0.0f};        // Y est le "haut"
  camera.fovy = 55.0f;
  camera.projection = CAMERA_PERSPECTIVE;
  cam_yaw = 0.0f;
  cam_pitch = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// update_camera_rotation — rotation FPS manuelle, position verrouillée
//
// Principe :
//   GetMouseDelta() → delta pixels souris depuis la frame précédente
//   yaw   += delta.x * sensitivity  (rotation horizontale autour de Y)
//   pitch -= delta.y * sensitivity  (rotation verticale, inversé car Y souris ↓
//   = regard ↓)
//
// Clamp yaw/pitch pour garder le goban visible en permanence.
//
// Direction de regard calculée depuis yaw/pitch (coordonnées sphériques) :
//   forward.x = sin(yaw) * cos(pitch)   ← composante droite/gauche
//   forward.y = sin(pitch)              ← composante haut/bas
//   forward.z = -cos(yaw) * cos(pitch)  ← composante avant (Z négatif = "vers
//   le goban")
//
// camera.position reste FIXE. Seul camera.target = position + forward change.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::update_camera_rotation() {
  Vector2 delta = GetMouseDelta();
  cam_yaw += delta.x * MOUSE_SENSITIVITY;
  cam_pitch -= delta.y * MOUSE_SENSITIVITY; // Inversé : souris bas = regard bas

  // Clamp : empêche de "retourner" la caméra hors du goban
  if (cam_yaw > CAM_YAW_LIMIT)
    cam_yaw = CAM_YAW_LIMIT;
  if (cam_yaw < -CAM_YAW_LIMIT)
    cam_yaw = -CAM_YAW_LIMIT;
  if (cam_pitch > CAM_PITCH_LIMIT)
    cam_pitch = CAM_PITCH_LIMIT;
  if (cam_pitch < -CAM_PITCH_LIMIT)
    cam_pitch = -CAM_PITCH_LIMIT;

  // Coordonnées sphériques → vecteur directionnel cartésien
  Vector3 forward;
  forward.x = sinf(cam_yaw) * cosf(cam_pitch);
  forward.y = sinf(cam_pitch);
  forward.z =
      -cosf(cam_yaw) * cosf(cam_pitch); // Z négatif = vers le goban (à Z=0)

  camera.position = {9.0f, 9.0f, 22.0f}; // Position toujours fixe
  camera.target = {camera.position.x + forward.x, camera.position.y + forward.y,
                   camera.position.z + forward.z};
  camera.up = {0.0f, 1.0f, 0.0f};
}

// ─────────────────────────────────────────────────────────────────────────────
// Boucle principale 3D
//
// Différences vs 2D :
//   - DisableCursor() : capture la souris dans la fenêtre (FPS standard)
//   - Pas de queue d'événements à drainer : Raylib expose un état
//   clavier/souris
//     via IsKeyPressed(), IsMouseButtonPressed(), GetMouseDelta() etc.
//   - render_scene() est appelé chaque frame (pas de séparation menu/jeu dans
//   run)
//
// Snapshot avant/après pour les animations (même logique qu'en 2D).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::run(GameState &state, Gomoku &gomoku) {
  DisableCursor(); // Capture souris dans la fenêtre
  HideCursor();

  while (!WindowShouldClose() && running) {
    if (current_state == UI3DState::MAIN_MENU) {
      handle_menu_input();
      render_menu();
    } else {
      update_camera_rotation();

      // Raycast vers le goban chaque frame (case visée par le réticule)
      has_hover = raycast_to_board(hovered_row, hovered_col);

      // Snapshot avant input pour détecter les captures
      uint8_t cap_b_before = state.board.blackCaptures;
      uint8_t cap_w_before = state.board.whiteCaptures;
      BitBoard board_snap = state.board;

      handle_game_input(state, gomoku);

      // Cooldown IA 500ms → lance le thread après l'expiration
      if (current_state == UI3DState::PLAYING_SOLO && ai_pending_ &&
          !state.game_over && !ai_thread_active_.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed_ms =
            std::chrono::duration<double, std::milli>(now - ai_pending_since_)
                .count();
        if (elapsed_ms >= 500.0) {
          ai_pending_ = false;
          handle_ai_turn(state, gomoku); // non-bloquant : lance le thread
        }
      }

      // Application du résultat IA (thread principal uniquement)
      // La caméra a pu bouger librement pendant que le thread calculait.
      if (ai_result_ready_.load() && !state.game_over) {
        ai_result_ready_.store(false);
        if (current_state == UI3DState::PLAYING_SOLO) {
          state.last_ai_move_time_ms = (double)ai_result_.computeTimeMs;
          state.place_stone(ai_result_.col, ai_result_.row);
          ai_highlight_row_ = ai_result_.row;
          ai_highlight_col_ = ai_result_.col;
          ai_highlight_timer_ = 0.5f;
          apply_win_check(state);
        } else if (current_state == UI3DState::PLAYING_MULTI) {
          state.best_move_suggestion = ai_result_;
        }
      }

      // Détecter captures : cases non-vides avant → vides après
      if (state.board.blackCaptures != cap_b_before ||
          state.board.whiteCaptures != cap_w_before) {
        for (int r = 0; r < 19; r++)
          for (int c = 0; c < 19; c++)
            if (board_snap.get(r, c) != kEmpty &&
                state.board.get(r, c) == kEmpty)
              capture_anims.push_back({r, c, 1.0f});
      }

      // ── Mise à jour des timers d'animation ──────────────────────────
      float dt = GetFrameTime(); // Secondes depuis la frame précédente (Raylib)

      // Bob idle : sinusoïde double fréquence (X lent, Y rapide)
      vm_bob_time += dt;

      // Recoil : décroît à 6× la vitesse du temps (retour à 0 rapide)
      if (vm_recoil > 0.0f)
        vm_recoil -= dt * 6.0f;
      if (vm_recoil < 0.0f)
        vm_recoil = 0.0f;

      // Bolt-action : décompte de la durée d'animation du verrou
      if (bolt_anim_timer_ > 0.0f)
        bolt_anim_timer_ -= dt;
      if (bolt_anim_timer_ < 0.0f)
        bolt_anim_timer_ = 0.0f;

      // Physique des douilles : gravité sur l'axe U (haut viewmodel)
      for (auto &s : shell_casings_) {
        s.vu -= 4.0f * dt; // Gravité
        s.r += s.vr * dt;
        s.u += s.vu * dt;
        s.f += s.vf * dt;
        s.rot += s.rot_spd * dt;
        s.timer -= dt;
      }
      // Idiome erase-remove (même pattern que 2D)
      shell_casings_.erase(
          std::remove_if(shell_casings_.begin(), shell_casings_.end(),
                         [](const ShellCasing &s) { return s.timer <= 0.0f; }),
          shell_casings_.end());

      // Timers des captures
      for (auto &a : capture_anims)
        a.timer -= dt;
      capture_anims.erase(std::remove_if(capture_anims.begin(),
                                         capture_anims.end(),
                                         [](const CaptureAnim3D &a) {
                                           return a.timer <= 0.0f;
                                         }),
                          capture_anims.end());

      // Timer surbrillance IA
      if (ai_highlight_timer_ > 0.0f)
        ai_highlight_timer_ -= dt;

      render_scene(state);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Menu 3D
//
// MeasureText(text, size) : retourne la largeur en pixels du texte avec cette
// taille. Utilisé pour centrer horizontalement (sw/2 - largeur/2).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::render_menu() {
  BeginDrawing();
  ClearBackground({40, 40, 40, 255});

  int sw = GetScreenWidth();
  DrawText("GOMOKU FPS", sw / 2 - MeasureText("GOMOKU FPS", 60) / 2, 100, 60,
           WHITE);

  const char *opt0 = "Mode Solo (vs IA)";
  const char *opt1 = "Mode Multi (Local 1v1)";
  Color c0 = (menu_selection == 0) ? YELLOW : WHITE;
  Color c1 = (menu_selection == 1) ? YELLOW : WHITE;

  DrawText(opt0, sw / 2 - MeasureText(opt0, 40) / 2, 280, 40, c0);
  DrawText(opt1, sw / 2 - MeasureText(opt1, 40) / 2, 370, 40, c1);
  DrawText("[UP/DOWN] Select   [ENTER] Start",
           sw / 2 - MeasureText("[UP/DOWN] Select   [ENTER] Start", 20) / 2,
           500, 20, GRAY);

  EndDrawing();
}

void GameUI3D::handle_menu_input() {
  // IsKeyPressed : retourne true UNE SEULE frame (pas de répétition
  // automatique)
  if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
    menu_selection = (menu_selection + 1) % 2;
  if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
    menu_selection = (menu_selection - 1 + 2) % 2;
  if (IsKeyPressed(KEY_ENTER)) {
    if (menu_selection == 0)
      current_state = UI3DState::PLAYING_SOLO;
    else
      current_state = UI3DState::PLAYING_MULTI;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_game_input — placement de pierre par clic gauche (FPS)
//
// Différence vs 2D : le clic place la pierre à la case VISÉE (réticule),
// pas à la position de la souris. has_hover + hovered_row/col sont calculés
// par raycast_to_board() chaque frame.
//
// bolt_anim_timer_ > 0 bloque tout nouveau tir pendant l'animation du verrou.
// Sur placement réussi :
//   - vm_recoil = 1.0f (recul maximal)
//   - bolt_anim_timer_ = 0.6s
//   - Éjection d'une douille (physique simple)
//   - check_game_over() (version simplifiée, sans endgame capture rule)
//   - En Solo : démarrer cooldown IA 500ms
//   - En Multi : calculer la suggestion de coup
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::handle_game_input(GameState &state, Gomoku &gomoku) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    current_state = UI3DState::MAIN_MENU;
    state.reset();
    init_camera();
    ai_pending_ = false;
    bolt_anim_timer_ = 0.0f;
    pending_alignment_win_ = false;
    pending_winner_ = makePlayerNone();
    ai_result_ready_.store(false); // Ignorer tout résultat en vol
    shell_casings_.clear();
    return;
  }

  if (state.game_over) {
    if (IsKeyPressed(KEY_R)) { // Rejouer en 3D = touche R
      state.reset();
      ai_pending_ = false;
      bolt_anim_timer_ = 0.0f;
      pending_alignment_win_ = false;
      pending_winner_ = makePlayerNone();
      ai_result_ready_.store(false);
      shell_casings_.clear();
    }
    return;
  }

  // En Solo, bloquer les clics quand c'est le tour de l'IA (WHITE)
  if (current_state == UI3DState::PLAYING_SOLO &&
      !isBlackPlayer(state.current_player))
    return;

  // Bloquer pendant l'animation bolt-action (0.6s après chaque tir)
  if (bolt_anim_timer_ > 0.0f)
    return;

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && has_hover) {
    if (Rules::is_valid_move(state, hovered_col, hovered_row)) {
      if (state.place_stone(hovered_col, hovered_row)) {
        // Déclencher les animations de tir
        vm_recoil = 1.0f;
        bolt_anim_timer_ = BOLT_ANIM_DURATION;

        // Éjecter une douille (position initiale près du boîtier de culasse)
        ShellCasing s;
        s.r = -0.02f;
        s.u = 0.03f;
        s.f = 0.05f; // Position initiale
        s.vr = -0.40f;
        s.vu = 0.35f;
        s.vf = -0.10f; // Vitesse initiale
        s.timer = 0.70f;
        s.rot = 0.0f;
        s.rot_spd = 600.0f; // Rotation rapide (degrés/s)
        shell_casings_.push_back(s);

        apply_win_check(state);

        if (!state.game_over) {
          if (current_state == UI3DState::PLAYING_SOLO) {
            // Démarrer cooldown IA 500ms
            ai_pending_ = true;
            ai_pending_since_ = std::chrono::high_resolution_clock::now();
          } else {
            // Multi : suggestion dans un thread (non-bloquant)
            if (!ai_thread_active_.load()) {
              ai_thread_active_.store(true);
              ai_result_ready_.store(false);
              BitBoard board_copy = state.board;
              Cell c = playerToCell(state.current_player);
              if (ai_thread_.joinable())
                ai_thread_.join();
              ai_thread_ =
                  std::thread([this, board_copy, c, &gomoku]() mutable {
                    Move result = gomoku.getBestMove(board_copy, c);
                    ai_result_ = result;
                    ai_result_ready_.store(true);
                    ai_thread_active_.store(false);
                  });
              ai_thread_.detach();
            }
          }
        }
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_ai_turn — lance getBestMove dans un thread séparé (non-bloquant)
//
// Pourquoi un thread ?
//   getBestMove() peut prendre jusqu'à ~500ms. Sans thread, la boucle
//   principale est gelée : la caméra ne répond plus, le rendu s'arrête. Avec un
//   thread détaché, la boucle continue à tourner à 60fps. Le résultat est
//   appliqué dans run() dès que ai_result_ready_ passe à true.
//
// Sécurité thread :
//   - board_copy : copie par valeur du plateau au moment du lancement → le
//     thread travaille sur ses propres données, aucune race avec le rendu.
//   - gomoku : non modifié par le thread (getBestMove est const sur gomoku).
//   - ai_result_ / ai_result_ready_ / ai_thread_active_ : atomiques.
//
// Note : on ne lance pas de second thread si un est déjà en cours.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::handle_ai_turn(GameState &state, Gomoku &gomoku) {
  if (ai_thread_active_.load())
    return;

  ai_thread_active_.store(true);
  ai_result_ready_.store(false);

  // Copie du plateau : le thread travaille sur sa propre instance
  BitBoard board_copy = state.board;
  Cell ai_cell = playerToCell(state.current_player);

  if (ai_thread_.joinable())
    ai_thread_.join();

  ai_thread_ = std::thread([this, board_copy, ai_cell, &gomoku]() mutable {
    auto t_start = std::chrono::high_resolution_clock::now();
    Move result = gomoku.getBestMove(board_copy, ai_cell);
    auto t_end = std::chrono::high_resolution_clock::now();

    result.computeTimeMs =
        (long)std::chrono::duration<double, std::milli>(t_end - t_start)
            .count();
    ai_result_ = result;
    ai_result_ready_.store(true);
    ai_thread_active_.store(false);
  });
  ai_thread_.detach();
}

// ─────────────────────────────────────────────────────────────────────────────
// apply_win_check — vérification de victoire avec règle endgame capture (3D)
//
// Portage fidèle de la logique 2D (Input.cpp::apply_win_check).
// Appelée après chaque placement (humain ou IA). place_stone() a déjà switché
// current_player, donc "qui vient de jouer" = l'opposé de current_player.
//
// Cas 1 — pending_alignment_win_ actif (l'adversaire avait une chance) :
//   Si l'alignement est encore intact → le pending_winner_ gagne.
//   Sinon → annuler le pending et réévaluer le flux normal.
//
// Cas 2 — pas de pending :
//   a) Victoire par capture : immédiate.
//   b) Victoire par alignement : immédiate SAUF si l'adversaire peut capturer
//      une paire de l'alignement → alors pending (l'adversaire a un tour).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::apply_win_check(GameState &state) {
  Player just_played = isBlackPlayer(state.current_player) ? makePlayerWhite()
                                                           : makePlayerBlack();
  Player next_player = state.current_player;

  if (pending_alignment_win_) {
    if (Rules::check_win_condition(state, pending_winner_)) {
      state.game_over = true;
      state.best_move_suggestion = {-1, -1, 0, 0};
      pending_alignment_win_ = false;
      pending_winner_ = makePlayerNone();
      return;
    }
    pending_alignment_win_ = false;
    pending_winner_ = makePlayerNone();

    if (Rules::check_win_by_capture(state, just_played)) {
      state.game_over = true;
      state.best_move_suggestion = {-1, -1, 0, 0};
      return;
    }
    if (Rules::check_win_condition(state, just_played)) {
      if (!can_opponent_capture_from_alignment(state, just_played,
                                               next_player)) {
        state.game_over = true;
        state.best_move_suggestion = {-1, -1, 0, 0};
        return;
      }
      pending_alignment_win_ = true;
      pending_winner_ = just_played;
    }
    return;
  }

  if (Rules::check_win_by_capture(state, just_played)) {
    state.game_over = true;
    state.best_move_suggestion = {-1, -1, 0, 0};
    return;
  }

  if (Rules::check_win_condition(state, just_played)) {
    if (!can_opponent_capture_from_alignment(state, just_played, next_player)) {
      state.game_over = true;
      state.best_move_suggestion = {-1, -1, 0, 0};
      return;
    }
    pending_alignment_win_ = true;
    pending_winner_ = just_played;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// raycast_to_board — centre écran → case du goban
//
// Étape 1 : GetScreenToWorldRay(centre_écran, camera)
//   Raylib déproject un pixel 2D en rayon 3D via la matrice de projection/view.
//   Retourne un Ray {position: origine, direction: direction unitaire}.
//
// Étape 2 : Intersection rayon / plan Z=0 (le goban est dans le plan Z=0)
//   Le rayon : P(t) = ray.position + t * ray.direction
//   On cherche t tel que P(t).z = 0 :
//     ray.position.z + t * ray.direction.z = 0
//     t = -ray.position.z / ray.direction.z
//   Condition : t > 0 (devant la caméra) et ray.direction.z != 0.
//
// Étape 3 : Convertir le point d'impact en indices (col, row)
//   hit_x → col = round(hit_x)
//   hit_y → row = round(18.0f - hit_y)  (Y monde inversé par rapport au row)
// ─────────────────────────────────────────────────────────────────────────────
bool GameUI3D::raycast_to_board(int &out_row, int &out_col) {
  Ray ray = GetScreenToWorldRay(
      {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f}, camera);

  // Éviter la division par zéro si le rayon est parallèle au plan Z=0
  if (fabsf(ray.direction.z) < 0.0001f)
    return false;

  // t = paramètre d'intersection avec Z=0
  float t = -ray.position.z / ray.direction.z;
  if (t < 0.0f)
    return false; // Le goban est derrière la caméra

  float hit_x = ray.position.x + t * ray.direction.x;
  float hit_y = ray.position.y + t * ray.direction.y;

  // Arrondi à l'entier le plus proche = intersection la plus proche
  int col = (int)roundf(hit_x);
  int row = (int)roundf(18.0f - hit_y); // Inversion Y : row=0 → Y monde=18

  if (col < 0 || col >= 19 || row < 0 || row >= 19)
    return false;

  out_row = row;
  out_col = col;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// boardToWorld — conversion case grille → position monde 3D
//
// Système de coordonnées :
//   col (0-18) → X monde (0.0 - 18.0) : direct
//   row (0-18) → Y monde (18.0 - 0.0) : inversé car row=0 est en "haut" du
//   goban
//                                        et Y monde augmente vers le haut
//   z_offset   → Z (léger offset pour éviter le Z-fighting avec le plan du
//   goban)
// ─────────────────────────────────────────────────────────────────────────────
static inline Vector3 boardToWorld(int row, int col, float z_offset = 0.0f) {
  return {(float)col, 18.0f - (float)row, z_offset};
}

// ─────────────────────────────────────────────────────────────────────────────
// render_scene — pipeline de rendu 3 passes
//
// Pass 1 — Scène monde (BeginMode3D / EndMode3D)
//   Raylib configure la matrice de projection perspective + vue de la caméra.
//   Toute la géométrie 3D est rendue ici avec le Z-buffer actif.
//
// Pass 2 — Viewmodel (draw_viewmodel_3d)
//   L'arme est rendue dans un espace caméra virtuel séparé (vm_cam).
//   rlDrawRenderBatchActive() force le flush du batch précédent avant de
//   changer de matrice. rlDisableDepthTest() garantit que l'arme s'affiche
//   toujours devant la scène (même si géométriquement derrière un plan).
//
// Pass 3 — HUD 2D (après rlDisableDepthTest)
//   DrawText, DrawRectangle etc. utilisent des coordonnées écran (pixels).
//   rlDisableDepthTest() empêche les pixels 2D d'être occultés par le Z-buffer
//   de la scène 3D (problème classique de rendu mixte 2D/3D).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::render_scene(const GameState &state) {
  BeginDrawing();
  ClearBackground({30, 30, 30, 255});

  // ── Pass 1 : scène monde ──────────────────────────────────────────────────
  BeginMode3D(camera);
  draw_board_3d();
  draw_stones_3d(state);
  draw_ai_highlight_3d();
  draw_hover_indicator(state);
  draw_best_move_3d(state);
  EndMode3D();

  // ── Pass 2 : viewmodel (arme + mains + avant-bras) ───────────────────────
  draw_viewmodel_3d();

  // ── Pass 3 : HUD 2D — désactiver le depth test pour que le 2D soit toujours
  // visible
  rlDisableDepthTest();
  draw_crosshair();
  draw_hud(state);
  draw_capture_anims_3d();
  draw_history(state);

  if (state.game_over)
    draw_game_over_overlay(state);

  EndDrawing();
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_board_3d — plateau et grille en 3D
//
// Pas de DrawCube pour la surface : DrawCube utilise des normales et produit
// des artefacts de shading plat. On utilise DrawTriangle3D (pas de shading)
// pour une couleur uniforme.
//
// Deux quads (= 4 triangles) :
//   base_col  : bordure légèrement plus sombre sous la surface jouable
//   surface_col : couleur bois du goban (identique au 2D)
//
// Grille : DrawLine3D à Z=0.01f (léger offset au-dessus de la surface)
// Hoshi (points étoiles) : 9 positions standard, petites sphères DrawSphere
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_board_3d() {
  Color base_col = {180, 140, 70, 255};
  Color surface_col = {220, 179, 92, 255}; // Même couleur que le 2D

  // Bordure (légèrement plus grande que la surface)
  Vector3 b0 = {-1.0f, -1.0f, -0.2f}, b1 = {19.0f, -1.0f, -0.2f};
  Vector3 b2 = {19.0f, 19.0f, -0.2f}, b3 = {-1.0f, 19.0f, -0.2f};
  DrawTriangle3D(b0, b1, b2, base_col);
  DrawTriangle3D(b0, b2, b3, base_col);

  // Surface jouable
  float m = 0.25f; // Marge pour que la surface dépasse légèrement les lignes
  Vector3 s0 = {-m, -m, 0.0f}, s1 = {18.0f + m, -m, 0.0f};
  Vector3 s2 = {18.0f + m, 18.0f + m, 0.0f}, s3 = {-m, 18.0f + m, 0.0f};
  DrawTriangle3D(s0, s1, s2, surface_col);
  DrawTriangle3D(s0, s2, s3, surface_col);

  // Grille (Z=0.01f pour éviter le Z-fighting avec la surface à Z=0)
  Color lineColor = {0, 0, 0, 255};
  for (int i = 0; i < 19; i++) {
    DrawLine3D({0.0f, (float)i, 0.01f}, {18.0f, (float)i, 0.01f}, lineColor);
    DrawLine3D({(float)i, 0.0f, 0.01f}, {(float)i, 18.0f, 0.01f}, lineColor);
  }

  // Hoshi : 9 points étoiles standard (positions en row, col)
  int hoshi[][2] = {{3, 3},  {3, 9},  {3, 15}, {9, 3},  {9, 9},
                    {9, 15}, {15, 3}, {15, 9}, {15, 15}};
  for (auto &h : hoshi) {
    Vector3 pos = boardToWorld(h[0], h[1], 0.02f);
    DrawSphere(pos, 0.12f, DARKGRAY);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_stones_3d — pierres comme sphères 3D
//
// DrawSphere(position, radius, color) : sphère Raylib.
// kBlack/kWhite utilisés à la place de BLACK/WHITE (voir note macros en début).
// STONE_RADIUS = 0.35f → légèrement plus petit que 0.5 (demi-cellule) pour
// laisser la grille visible, comme en 2D (CELL_SIZE * 0.4f vs 0.5f).
// z_offset = STONE_RADIUS * 0.6f : la sphère est légèrement au-dessus du plan.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_stones_3d(const GameState &state) {
  for (int row = 0; row < 19; row++) {
    for (int col = 0; col < 19; col++) {
      Cell cell = state.board.get(row, col);
      if (cell == kEmpty)
        continue;

      Vector3 pos = boardToWorld(row, col, STONE_RADIUS * 0.6f);
      Color color = (cell == kBlack)
                        ? Color{20, 20, 20, 255}     // Noir très sombre
                        : Color{240, 240, 240, 255}; // Blanc légèrement grisé
      DrawSphere(pos, STONE_RADIUS, color);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_ai_highlight_3d — anneau rouge fadeout sur le dernier coup IA
//
// DrawCircle3D : cercle dans un plan 3D défini par position, rayon et normal.
// Normal = {0,0,1} → cercle dans le plan XY (= plan du goban, Z=const).
// Deux anneaux de rayon légèrement différent pour plus de lisibilité.
// Alpha décroît de 255 à 0 sur 0.5s (même logique que le 2D).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_ai_highlight_3d() {
  if (current_state != UI3DState::PLAYING_SOLO)
    return;
  if (ai_highlight_timer_ <= 0.0f || ai_highlight_row_ < 0)
    return;

  float alpha = ai_highlight_timer_ / 0.5f; // Normalisation 0→1
  unsigned char a = (unsigned char)(alpha * 255);

  Vector3 pos = boardToWorld(ai_highlight_row_, ai_highlight_col_, 0.08f);
  DrawCircle3D(pos, 0.48f, {0.0f, 0.0f, 1.0f}, 0.0f, {220, 50, 50, a});
  DrawCircle3D(pos, 0.52f, {0.0f, 0.0f, 1.0f}, 0.0f,
               {220, 50, 50, (unsigned char)(a / 2)});
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_hover_indicator — indicateur de la case visée par le réticule
//
// Sphère fantôme semi-transparente + cercle vert au sol.
// N'est affiché que si :
//   - Le raycast touche le plateau (has_hover)
//   - La partie n'est pas terminée
//   - En Solo : c'est le tour du joueur humain (BLACK)
//   - La case est vide
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_hover_indicator(const GameState &state) {
  if (!has_hover || state.game_over)
    return;
  if (current_state == UI3DState::PLAYING_SOLO &&
      !isBlackPlayer(state.current_player))
    return;
  if (!state.board.isEmpty(hovered_row, hovered_col))
    return;

  Vector3 pos = boardToWorld(hovered_row, hovered_col, STONE_RADIUS * 0.6f);
  Color ghost = isBlackPlayer(state.current_player)
                    ? Color{20, 20, 20, 100}     // Fantôme noir
                    : Color{240, 240, 240, 100}; // Fantôme blanc
  DrawSphere(pos, STONE_RADIUS, ghost);

  // Cercle au sol pour indiquer la case précisément
  Vector3 ring_pos = boardToWorld(hovered_row, hovered_col, 0.05f);
  DrawCircle3D(ring_pos, 0.45f, {0.0f, 0.0f, 1.0f}, 0.0f, GREEN);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_best_move_3d — suggestion de coup (cercle pulsant)
//
// Utilisé en mode Multi (suggestion calculée après chaque coup humain).
// pulse = (sin(time*4) + 1) / 2 → valeur entre 0 et 1, oscillation à 4 rad/s.
// L'alpha du cercle oscille entre 80 et 160 → effet "battement" visuel.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_best_move_3d(const GameState &state) {
  if (state.game_over)
    return;
  if (state.best_move_suggestion.row < 0 || state.best_move_suggestion.col < 0)
    return;

  Vector3 pos = boardToWorld(state.best_move_suggestion.row,
                             state.best_move_suggestion.col, 0.05f);

  // Effet de pulsation : alpha oscille en sinusoïde
  float pulse = (sinf((float)GetTime() * 4.0f) + 1.0f) * 0.5f;
  unsigned char alpha = (unsigned char)(80 + pulse * 80);

  DrawCircle3D(pos, 0.5f, {0.0f, 0.0f, 1.0f}, 0.0f, {0, 200, 255, alpha});
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_viewmodel_3d — arme + mains + avant-bras en première personne
//
// Technique : rendu dans un espace caméra virtuel séparé (vm_cam)
//
// Pourquoi une caméra séparée ?
//   Si on dessinait l'arme avec la caméra de la scène, sa position dépendrait
//   du monde 3D et elle pourrait s'enfoncer dans le goban. Avec vm_cam dont
//   l'origine est (0,0,0), l'arme est toujours à la même position relative
//   à l'œil du joueur.
//   FOV 65° (vs 55° pour la scène) : champ plus large évite la distorsion
//   des objets proches.
//
// rlDrawRenderBatchActive() : force le flush du batch Raylib AVANT de changer
//   de matrice de projection. Sans ça, les géométries des deux passes se
//   mélangeraient.
//
// drawBox (lambda) : construit une boîte 3D avec 6 faces (12 triangles) en
//   coordonnées camera (cam_right, cam_up, cam_dir). Évite DrawCube qui
//   applique un shading automatique non désiré.
//
// Animations :
//   bob_x/bob_y : balancement idle sinusoïdal (position de repos vivante)
//   recoil_back/recoil_up : recul lors du tir, décroît à 6× dt
//   pump_back : animation bolt-action, suit pump_phase (aller-retour 0→0.5→0)
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_viewmodel_3d() {
  rlDrawRenderBatchActive(); // Flush avant changement de matrice

  // Vecteurs de base de la caméra (extraits de la direction de regard)
  Vector3 cam_dir =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam_dir, camera.up));
  Vector3 cam_up = Vector3Normalize(Vector3CrossProduct(cam_right, cam_dir));

  // Caméra viewmodel : origine à (0,0,0), même orientation que la caméra
  // principale
  Camera3D vm_cam = {};
  vm_cam.position = {0.0f, 0.0f, 0.0f};
  vm_cam.target = cam_dir;
  vm_cam.up = cam_up;
  vm_cam.fovy = 65.0f; // FOV plus large pour les objets proches
  vm_cam.projection = CAMERA_PERSPECTIVE;

  // Bob idle : double sinusoïde (X à 1.8 rad/s, Y à 3.6 rad/s = 2× plus rapide)
  float bob_x = sinf(vm_bob_time * 1.8f) * 0.015f;
  float bob_y = sinf(vm_bob_time * 3.6f) * 0.008f;

  // Recul : fort kick arrière + léger kick vers le haut
  float recoil_back = vm_recoil * 0.18f;
  float recoil_up = vm_recoil * 0.07f;

  // Position de base du grip (bas-droite de l'écran, en coordonnées caméra)
  float base_right = 0.30f + bob_x;
  float base_down = -0.22f + bob_y;
  float base_forward = 0.55f - recoil_back;

  Vector3 grip = Vector3Scale(cam_right, base_right);
  grip = Vector3Add(grip, Vector3Scale(cam_up, base_down));
  grip = Vector3Add(grip, Vector3Scale(cam_dir, base_forward));
  grip = Vector3Add(grip, Vector3Scale(cam_up, recoil_up));

  // Palette de couleurs Tron
  Color skin_col = {210, 170, 130, 255};
  Color skin_dark = {180, 140, 100, 255};
  Color sleeve_col = {60, 60, 60, 255};
  Color body_col = {12, 14, 20, 255};    // Noir absolu
  Color accent_col = {0, 200, 255, 255}; // Cyan primaire
  Color accent_dim = {0, 55, 90, 255};   // Cyan sombre

  rlDisableDepthTest();
  BeginMode3D(vm_cam);

  // drawBox : boîte centrée sur `c`, dimensions w×h×l, en coordonnées caméra.
  // Construit 8 sommets via les vecteurs cam_right/cam_up/cam_dir,
  // puis les 12 triangles (6 faces × 2 triangles).
  auto drawBox = [&](Vector3 c, float w, float h, float l, Color col) {
    float hw = w * 0.5f, hh = h * 0.5f, hl = l * 0.5f;
    // Fonction lambda imbriquée : construit un sommet à offset (rx,ry,rz)
    auto p = [&](float rx, float ry, float rz) -> Vector3 {
      return Vector3Add(c, Vector3Add(Vector3Scale(cam_right, rx),
                                      Vector3Add(Vector3Scale(cam_up, ry),
                                                 Vector3Scale(cam_dir, rz))));
    };
    // 8 sommets d'une boîte (-hw/-hh/-hl à +hw/+hh/+hl)
    Vector3 v[8] = {p(-hw, -hh, -hl), p(hw, -hh, -hl), p(-hw, hh, -hl),
                    p(hw, hh, -hl),   p(-hw, -hh, hl), p(hw, -hh, hl),
                    p(-hw, hh, hl),   p(hw, hh, hl)};
    // 6 faces = 12 triangles (winding order cohérent)
    DrawTriangle3D(v[4], v[5], v[7], col);
    DrawTriangle3D(v[4], v[7], v[6], col); // Avant
    DrawTriangle3D(v[1], v[0], v[2], col);
    DrawTriangle3D(v[1], v[2], v[3], col); // Arrière
    DrawTriangle3D(v[2], v[3], v[7], col);
    DrawTriangle3D(v[2], v[7], v[6], col); // Haut
    DrawTriangle3D(v[0], v[1], v[5], col);
    DrawTriangle3D(v[0], v[5], v[4], col); // Bas
    DrawTriangle3D(v[1], v[3], v[7], col);
    DrawTriangle3D(v[1], v[7], v[5], col); // Droite
    DrawTriangle3D(v[4], v[6], v[2], col);
    DrawTriangle3D(v[4], v[2], v[0], col); // Gauche
  };

  // ── Animation bolt-action ─────────────────────────────────────────────────
  // pump_t : 0.0 (fin) → 1.0 (début de l'animation)
  // pump_phase : aller-retour 0→1→0 via (t > 0.5) ? (1-t)*2 : t*2
  // pump_back : déplacement maximal du bloc coulissant (0.10 unités)
  float pump_t = bolt_anim_timer_ / BOLT_ANIM_DURATION;
  float pump_phase = (pump_t > 0.5f) ? (1.0f - pump_t) * 2.0f : pump_t * 2.0f;
  float pump_back = pump_phase * 0.10f;

  // ── Canon ─────────────────────────────────────────────────────────────────
  const float BRL_L = 0.50f, BRL_S = 0.044f;
  Vector3 brl_org = Vector3Add(grip, Vector3Scale(cam_up, 0.038f));
  brl_org = Vector3Add(brl_org, Vector3Scale(cam_dir, 0.08f));
  Vector3 brl_ctr = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L * 0.5f));
  Vector3 barrel_tip =
      Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L + 0.006f));
  drawBox(brl_ctr, BRL_S, BRL_S, BRL_L, body_col);
  // Stripes lumineuses cyan sur les arêtes du canon
  drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_up, BRL_S * 0.5f + 0.002f)),
          BRL_S - 0.008f, 0.003f, BRL_L, accent_col);
  drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_right, BRL_S * 0.5f + 0.001f)),
          0.003f, BRL_S - 0.008f, BRL_L, accent_col);
  drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_right, -BRL_S * 0.5f - 0.001f)),
          0.003f, BRL_S - 0.008f, BRL_L, accent_col);
  // Bouche du canon (rebord cyan + âme creuse)
  drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_dir, BRL_L * 0.5f + 0.002f)),
          BRL_S + 0.004f, BRL_S + 0.004f, 0.004f, accent_col);
  drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_dir, BRL_L * 0.5f + 0.005f)),
          BRL_S - 0.012f, BRL_S - 0.012f, 0.007f, body_col);
  // 3 anneaux de segmentation
  for (int ri = 0; ri < 3; ri++) {
    float t = 0.22f + ri * 0.22f;
    drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_dir, (t - 0.5f) * BRL_L)),
            BRL_S + 0.004f, BRL_S + 0.004f, 0.004f, accent_dim);
  }

  // ── Boîtier de culasse ────────────────────────────────────────────────────
  Vector3 recv = Vector3Add(grip, Vector3Scale(cam_dir, 0.02f));
  recv = Vector3Add(recv, Vector3Scale(cam_up, 0.018f));
  const float RCV_W = 0.076f, RCV_H = 0.086f, RCV_L = 0.22f;
  drawBox(recv, RCV_W, RCV_H, RCV_L, body_col);
  drawBox(Vector3Add(recv, Vector3Scale(cam_up, RCV_H * 0.5f + 0.002f)), 0.014f,
          0.003f, RCV_L, accent_col);
  for (int s = -1; s <= 1; s += 2) {
    Vector3 sp =
        Vector3Add(recv, Vector3Scale(cam_right, s * (RCV_W * 0.5f + 0.001f)));
    drawBox(Vector3Add(sp, Vector3Scale(cam_up, 0.020f)), 0.003f, 0.007f,
            RCV_L * 0.88f, accent_col);
    drawBox(Vector3Add(sp, Vector3Scale(cam_up, -0.020f)), 0.003f, 0.007f,
            RCV_L * 0.88f, accent_dim);
  }
  {
    Vector3 cell =
        Vector3Add(recv, Vector3Scale(cam_right, RCV_W * 0.5f + 0.001f));
    cell = Vector3Add(cell, Vector3Scale(cam_dir, 0.008f));
    drawBox(cell, 0.004f, 0.038f, 0.060f, accent_col);
    drawBox(Vector3Add(cell, Vector3Scale(cam_right, 0.003f)), 0.003f, 0.030f,
            0.052f, accent_dim);
  }
  drawBox(Vector3Add(recv, Vector3Scale(cam_dir, RCV_L * 0.5f + 0.001f)),
          RCV_W + 0.004f, RCV_H + 0.004f, 0.003f, accent_dim);
  drawBox(Vector3Add(recv, Vector3Scale(cam_dir, -RCV_L * 0.5f - 0.001f)),
          RCV_W + 0.004f, RCV_H + 0.004f, 0.003f, accent_dim);

  // ── Garde (sliding, suit pump_back) ──────────────────────────────────────
  {
    const float PM_W = 0.070f, PM_H = 0.032f, PM_L = 0.082f;
    Vector3 pump_ctr =
        Vector3Add(grip, Vector3Scale(cam_dir, 0.24f - pump_back));
    pump_ctr = Vector3Add(pump_ctr, Vector3Scale(cam_up, -0.004f));
    drawBox(pump_ctr, PM_W, PM_H, PM_L, body_col);
    drawBox(Vector3Add(pump_ctr, Vector3Scale(cam_up, PM_H * 0.5f + 0.002f)),
            PM_W - 0.010f, 0.003f, PM_L, accent_col);
    drawBox(Vector3Add(pump_ctr, Vector3Scale(cam_dir, PM_L * 0.5f + 0.001f)),
            PM_W + 0.004f, PM_H + 0.004f, 0.003f, accent_col);
    drawBox(Vector3Add(pump_ctr, Vector3Scale(cam_dir, -PM_L * 0.5f - 0.001f)),
            PM_W + 0.004f, PM_H + 0.004f, 0.003f, accent_col);
    Vector3 recv_front = Vector3Add(recv, Vector3Scale(cam_dir, RCV_L * 0.5f));
    float bar_off = PM_W * 0.5f - 0.008f;
    for (int s = -1; s <= 1; s += 2) {
      Vector3 pf = Vector3Add(pump_ctr, Vector3Scale(cam_right, s * bar_off));
      pf = Vector3Add(pf, Vector3Scale(cam_dir, PM_L * 0.5f));
      Vector3 rb = Vector3Add(recv_front, Vector3Scale(cam_right, s * bar_off));
      DrawLine3D(pf, rb, accent_dim);
    }
  }

  // ── Poignée ───────────────────────────────────────────────────────────────
  {
    Vector3 gc = Vector3Add(grip, Vector3Scale(cam_up, -0.052f));
    gc = Vector3Add(gc, Vector3Scale(cam_dir, -0.006f));
    drawBox(gc, 0.042f, 0.090f, 0.036f, body_col);
    drawBox(Vector3Add(gc, Vector3Scale(cam_dir, 0.019f)), 0.044f, 0.092f,
            0.003f, accent_dim);
    drawBox(Vector3Add(gc, Vector3Scale(cam_dir, -0.019f)), 0.044f, 0.092f,
            0.003f, accent_dim);
  }

  // ── Pontet de détente ─────────────────────────────────────────────────────
  {
    Vector3 tg = Vector3Add(grip, Vector3Scale(cam_dir, 0.034f));
    tg = Vector3Add(tg, Vector3Scale(cam_up, -0.028f));
    drawBox(tg, 0.054f, 0.007f, 0.044f, body_col);
    drawBox(Vector3Add(tg, Vector3Scale(cam_up, -0.012f)), 0.013f, 0.020f,
            0.006f, body_col);
  }

  // ── Crosse ────────────────────────────────────────────────────────────────
  {
    Vector3 bridge =
        Vector3Add(recv, Vector3Scale(cam_dir, -(RCV_L * 0.5f + 0.012f)));
    drawBox(bridge, RCV_W, RCV_H - 0.010f, 0.024f, body_col);
    Vector3 stk = Vector3Add(bridge, Vector3Scale(cam_dir, -0.136f));
    stk = Vector3Add(stk, Vector3Scale(cam_up, -0.008f));
    drawBox(stk, 0.056f, 0.060f, 0.248f, body_col);
    drawBox(Vector3Add(stk, Vector3Scale(cam_up, 0.032f)), 0.010f, 0.003f,
            0.240f, accent_col);
    for (int s = -1; s <= 1; s += 2) {
      Vector3 sp = Vector3Add(stk, Vector3Scale(cam_right, s * 0.029f));
      drawBox(Vector3Add(sp, Vector3Scale(cam_up, -0.010f)), 0.003f, 0.007f,
              0.240f, accent_dim);
    }
    drawBox(Vector3Add(stk, Vector3Scale(cam_dir, -0.126f)), 0.058f, 0.062f,
            0.003f, accent_dim);
  }

  // ── Guidon ────────────────────────────────────────────────────────────────
  {
    Vector3 sb = Vector3Add(recv, Vector3Scale(cam_up, RCV_H * 0.5f + 0.006f));
    sb = Vector3Add(sb, Vector3Scale(cam_dir, RCV_L * 0.5f - 0.028f));
    drawBox(sb, 0.020f, 0.004f, 0.012f, body_col);
    Vector3 sp = Vector3Add(sb, Vector3Scale(cam_up, 0.010f));
    drawBox(sp, 0.004f, 0.014f, 0.004f, body_col);
    DrawSphere(Vector3Add(sp, Vector3Scale(cam_up, 0.009f)), 0.005f,
               accent_col);
  }

  // ── Flash de bouche (burst cyan au moment du tir) ─────────────────────────
  // Visible uniquement quand vm_recoil > 0.5 (première moitié du recul).
  // fa (flash alpha) : 0→1 en fonction de la position dans le recul.
  if (vm_recoil > 0.5f) {
    float fa = (vm_recoil - 0.5f) * 2.0f;
    unsigned char a = (unsigned char)(fa * 255);
    Color energy = {0, 200, 255, a};
    Color energy_core = {200, 240, 255, a};
    // Cube d'énergie + croix de lumière rayonnante
    drawBox(Vector3Add(barrel_tip, Vector3Scale(cam_dir, 0.04f * fa)),
            0.08f * fa, 0.08f * fa, 0.08f * fa, energy);
    drawBox(Vector3Add(barrel_tip, Vector3Scale(cam_dir, 0.02f * fa)),
            0.04f * fa, 0.04f * fa, 0.04f * fa, energy_core);
    DrawLine3D(barrel_tip,
               Vector3Add(barrel_tip, Vector3Scale(cam_up, 0.24f * fa)),
               energy);
    DrawLine3D(barrel_tip,
               Vector3Add(barrel_tip, Vector3Scale(cam_up, -0.24f * fa)),
               energy);
    DrawLine3D(barrel_tip,
               Vector3Add(barrel_tip, Vector3Scale(cam_right, 0.24f * fa)),
               energy);
    DrawLine3D(barrel_tip,
               Vector3Add(barrel_tip, Vector3Scale(cam_right, -0.24f * fa)),
               energy);
  }

  // ── Main droite (poignée) ─────────────────────────────────────────────────
  {
    Vector3 rh = Vector3Add(grip, Vector3Scale(cam_up, -0.052f));
    drawBox(rh, 0.060f, 0.040f, 0.050f, skin_col);
    Vector3 rh_f = Vector3Add(rh, Vector3Scale(cam_dir, 0.030f));
    rh_f = Vector3Add(rh_f, Vector3Scale(cam_up, 0.010f));
    drawBox(rh_f, 0.056f, 0.020f, 0.026f, skin_col);
    Vector3 rh_t = Vector3Add(rh, Vector3Scale(cam_right, -0.031f));
    rh_t = Vector3Add(rh_t, Vector3Scale(cam_up, 0.012f));
    drawBox(rh_t, 0.016f, 0.016f, 0.032f, skin_dark);
  }

  // ── Avant-bras droit ──────────────────────────────────────────────────────
  // DrawCylinderEx : cylindre entre deux points 3D (wrist et elbow).
  // Rayons différents pour un aspect conique naturel.
  {
    Vector3 wrist = Vector3Add(grip, Vector3Scale(cam_dir, -0.04f));
    wrist = Vector3Add(wrist, Vector3Scale(cam_up, -0.072f));
    Vector3 elbow = Vector3Add(wrist, Vector3Scale(cam_right, 0.16f));
    elbow = Vector3Add(elbow, Vector3Scale(cam_up, -0.11f));
    elbow = Vector3Add(elbow, Vector3Scale(cam_dir, -0.13f));
    DrawCylinderEx(wrist, elbow, 0.034f, 0.040f, 8, sleeve_col);
    DrawCylinderEx(Vector3Add(wrist, Vector3Scale(cam_dir, 0.01f)), wrist,
                   0.031f, 0.034f, 8, skin_col);
  }

  // ── Main gauche (suit le déplacement de la garde) ─────────────────────────
  {
    Vector3 lh = Vector3Add(grip, Vector3Scale(cam_dir, 0.24f - pump_back));
    lh = Vector3Add(lh, Vector3Scale(cam_up, -0.006f));
    lh = Vector3Add(lh, Vector3Scale(cam_right, -0.012f));
    drawBox(lh, 0.056f, 0.032f, 0.050f, skin_col);
    drawBox(Vector3Add(lh, Vector3Scale(cam_up, 0.020f)), 0.050f, 0.014f,
            0.038f, skin_dark);
  }

  // ── Avant-bras gauche (suit la garde) ────────────────────────────────────
  {
    Vector3 lw = Vector3Add(grip, Vector3Scale(cam_dir, 0.22f - pump_back));
    lw = Vector3Add(lw, Vector3Scale(cam_up, -0.030f));
    lw = Vector3Add(lw, Vector3Scale(cam_right, -0.012f));
    Vector3 le = Vector3Add(lw, Vector3Scale(cam_right, -0.18f));
    le = Vector3Add(le, Vector3Scale(cam_up, -0.09f));
    le = Vector3Add(le, Vector3Scale(cam_dir, -0.10f));
    DrawCylinderEx(lw, le, 0.032f, 0.037f, 8, sleeve_col);
    DrawCylinderEx(Vector3Add(lw, Vector3Scale(cam_dir, 0.01f)), lw, 0.029f,
                   0.032f, 8, skin_col);
  }

  // ── Douilles éjectées ─────────────────────────────────────────────────────
  // Rendues dans l'espace viewmodel (coordonnées relatives au grip).
  // DrawCylinderEx(sp, case_tip, ...) : petit cylindre orienté selon l'axe de
  // rotation. L'axe tourne dans le plan cam_right/cam_up selon s.rot.
  for (const auto &s : shell_casings_) {
    float alpha = std::min(s.timer / 0.3f, 1.0f); // Fade sur les 0.3s finales
    Color brass = {210, 165, 50, (unsigned char)(alpha * 255)};
    Vector3 sp = grip;
    sp = Vector3Add(sp, Vector3Scale(cam_right, s.r));
    sp = Vector3Add(sp, Vector3Scale(cam_up, s.u));
    sp = Vector3Add(sp, Vector3Scale(cam_dir, s.f));
    // Axe de la douille : tourne dans le plan cam_right/cam_up
    float rad = s.rot * DEG2RAD;
    Vector3 axis = Vector3Add(Vector3Scale(cam_right, cosf(rad)),
                              Vector3Scale(cam_up, sinf(rad)));
    Vector3 case_tip = Vector3Add(sp, Vector3Scale(axis, 0.026f));
    DrawCylinderEx(sp, case_tip, 0.007f, 0.005f, 6, brass);
  }

  EndMode3D();
  rlEnableDepthTest(); // Réactiver pour la prochaine frame
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_crosshair — réticule en croix au centre de l'écran
//
// Couleur verte (has_hover=true) = le réticule pointe sur une case valide.
// Couleur blanche (has_hover=false) = hors du plateau.
// Deux DrawRectangle : une barre horizontale et une verticale.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_crosshair() {
  int cx = GetScreenWidth() / 2;
  int cy = GetScreenHeight() / 2;
  int size = 12;
  int thick = 2;
  Color color = has_hover ? GREEN : WHITE;
  DrawRectangle(cx - size, cy - thick / 2, size * 2, thick,
                color); // Horizontal
  DrawRectangle(cx - thick / 2, cy - size, thick, size * 2, color); // Vertical
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_hud — HUD 2D en bas de l'écran
//
// Identique au 2D en termes d'informations :
//   - Mode + commandes
//   - Timer IA en cyan (exigence sujet)
//   - Tour courant
//   - Compteurs de captures
//   - En haut à droite : coordonnée visée (Aim: L-N) si has_hover
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_hud(const GameState &state) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();

  // Barre de fond semi-transparente (alpha=200)
  DrawRectangle(0, sh - 80, sw, 80, {40, 40, 40, 200});

  // Tour courant (centré)
  const char *turn_label =
      isBlackPlayer(state.current_player) ? "NOIR" : "BLANC";
  char turn_buf[64];
  snprintf(turn_buf, sizeof(turn_buf), "Tour %d : %s", state.current_turn,
           turn_label);
  DrawText(turn_buf, sw / 2 - MeasureText(turn_buf, 24) / 2, sh - 70, 24,
           WHITE);

  // Mode à gauche
  const char *mode = (current_state == UI3DState::PLAYING_SOLO)
                         ? "Solo (vs IA) - ESC: Menu"
                         : "Multi (Local 1v1) - ESC: Menu";
  DrawText(mode, 20, sh - 70, 18, LIGHTGRAY);

  // Timer IA (cyan, exigé par le sujet p.6)
  if (current_state == UI3DState::PLAYING_SOLO) {
    char timer_buf[64];
    snprintf(timer_buf, sizeof(timer_buf), "AI Time: %.2f ms",
             state.last_ai_move_time_ms);
    DrawText(timer_buf, 20, sh - 40, 18, {0, 200, 255, 255});
  }

  // Captures (centré)
  char cap_buf[64];
  snprintf(cap_buf, sizeof(cap_buf), "Captures  N: %d/10  B: %d/10",
           state.board.blackCaptures, state.board.whiteCaptures);
  DrawText(cap_buf, sw / 2 - MeasureText(cap_buf, 18) / 2, sh - 35, 18, WHITE);

  // Coordonnée visée (en haut à droite, en vert si valide)
  if (has_hover) {
    char aim_buf[32];
    char col_char = 'A' + hovered_col;
    snprintf(aim_buf, sizeof(aim_buf), "Aim: %c-%d", col_char, hovered_row + 1);
    DrawText(aim_buf, sw - 160, 20, 20, GREEN);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_game_over_overlay — écran de fin en 3D
//
// Overlay noir semi-transparent (alpha=150) sur toute la fenêtre.
// En Solo : affiche le dernier temps IA.
// Touche R pour rejouer (vs bouton cliquable en 2D).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_game_over_overlay(const GameState &state) {
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  DrawRectangle(0, 0, sw, sh, {0, 0, 0, 150});

  bool black_wins = Rules::check_win_condition(state, makePlayerBlack()) ||
                    Rules::check_win_by_capture(state, makePlayerBlack());
  bool white_wins = Rules::check_win_condition(state, makePlayerWhite()) ||
                    Rules::check_win_by_capture(state, makePlayerWhite());

  const char *msg = "GAME OVER";
  if (black_wins)
    msg = "NOIR GAGNE!";
  else if (white_wins)
    msg = "BLANC GAGNE!";

  DrawText(msg, sw / 2 - MeasureText(msg, 60) / 2, sh / 2 - 60, 60, RED);

  if (current_state == UI3DState::PLAYING_SOLO) {
    char timer_buf[64];
    snprintf(timer_buf, sizeof(timer_buf), "Dernier temps IA: %.2f ms",
             state.last_ai_move_time_ms);
    DrawText(timer_buf, sw / 2 - MeasureText(timer_buf, 22) / 2, sh / 2 + 10,
             22, {0, 200, 255, 255});
  }

  const char *restart = "Appuyer [R] pour Rejouer";
  DrawText(restart, sw / 2 - MeasureText(restart, 24) / 2, sh / 2 + 50, 24,
           WHITE);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_capture_anims_3d — croix X en coordonnées écran aux positions capturées
//
// GetWorldToScreen(world_pos, camera) : Raylib projette un point 3D en 2D
// écran. Permet d'afficher des effets 2D ancrés à une position 3D du monde.
// DrawLineEx : ligne 2D avec épaisseur (DrawLine n'a pas de paramètre
// épaisseur). Alpha décroît comme en 2D : timer * 255.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_capture_anims_3d() {
  for (const auto &a : capture_anims) {
    // Projeter la position 3D de la capture en 2D écran
    Vector3 world_pos = boardToWorld(a.row, a.col, 0.6f);
    Vector2 screen_pos = GetWorldToScreen(world_pos, camera);
    float arm = 16.0f;
    Color red = {220, 50, 50, (unsigned char)(a.timer * 255)};

    // Croix X : deux diagonales
    Vector2 tl = {screen_pos.x - arm, screen_pos.y - arm};
    Vector2 br = {screen_pos.x + arm, screen_pos.y + arm};
    Vector2 tr = {screen_pos.x + arm, screen_pos.y - arm};
    Vector2 bl = {screen_pos.x - arm, screen_pos.y + arm};
    DrawLineEx(tl, br, 3.0f, red);
    DrawLineEx(tr, bl, 3.0f, red);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_history — panneau d'historique des coups (gauche de l'écran)
//
// Semi-transparent (alpha=180) pour ne pas masquer complètement le goban.
// max_lines calculé dynamiquement selon la hauteur disponible.
// Format : "N. C L-R" avec C=N/B (couleur), L=lettre colonne, R=numéro ligne.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::draw_history(const GameState &state) {
  int sh = GetScreenHeight();
  int panel_w = 200;
  int panel_h = sh - 80; // S'arrête avant la barre HUD

  DrawRectangle(0, 0, panel_w, panel_h, {50, 50, 50, 180});
  DrawText("Historique", 10, 10, 22, WHITE);
  DrawLine(10, 35, panel_w - 10, 35, GRAY);

  // Calcul dynamique du nombre de lignes affichables
  int max_lines = (panel_h - 50) / 25;
  int start_idx = 0;
  if ((int)state.move_history.size() > max_lines)
    start_idx = (int)state.move_history.size() - max_lines;

  int y = 45;
  for (int i = start_idx; i < (int)state.move_history.size(); i++) {
    const GameMove &mv = state.move_history[i];
    char col_char = 'A' + mv.x;
    char line[64];
    snprintf(line, sizeof(line), "%d. %c %c-%d", mv.turn,
             isBlackPlayer(mv.player) ? 'N' : 'B', col_char, mv.y + 1);
    // Gris clair pour Noir, blanc pour Blanc (distinction sans couleur de fond)
    Color text_color = isBlackPlayer(mv.player) ? LIGHTGRAY : WHITE;
    DrawText(line, 10, y, 16, text_color);
    y += 25;
  }
}
