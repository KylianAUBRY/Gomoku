// ─────────────────────────────────────────────────────────────────────────────
// GameUI3D.cpp — Orchestrateur de la vue 3D FPS (Raylib)
//
// Responsabilités :
//   - Boucle principale run() : menu + gameplay
//   - Gestion caméra FPS (position fixe, rotation souris)
//   - Input joueur (raycast → placement de pierre)
//   - Thread IA (getBestMove non-bloquant)
//   - Règle endgame capture (apply_win_check)
//   - Pipeline de rendu (3 passes : scène → viewmodel → HUD)
// ─────────────────────────────────────────────────────────────────────────────

// ⚠ ORDRE OBLIGATOIRE : engine avant sauvegardes, sauvegardes avant raylib
#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "../engine/Rules.hpp"

// Sauvegardes Cell AVANT que raylib écrase les macros BLACK/WHITE
static constexpr Cell kBlack = BLACK;
static constexpr Cell kWhite = WHITE;
static constexpr Cell kEmpty = EMPTY;

// Raylib (écrase les macros — kBlack/kWhite/kEmpty sont déjà sauvegardés)
#include "raylib.h"
#include "rlgl.h"    // rlDisableDepthTest

// Headers du module (incluent engine/raylib en no-op via pragma once)
#include "GameUI3D.hpp"
#include "Board3D.hpp"
#include "Viewmodel3D.hpp"
#include "HUD3D.hpp"
#include "front3d_compat.hpp" // isBlackPlayer, playerToCell, makePlayer*

#include <algorithm>
#include <chrono>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// can_opponent_capture_from_alignment
//
// Renvoie true si opp peut, en jouant UN coup, capturer une paire de winner
// dont au moins une pierre appartient à l'alignement de 5+ pierres en cours.
// Logique identique à la version 2D (Input.cpp).
// ─────────────────────────────────────────────────────────────────────────────
static bool can_opponent_capture_from_alignment(const GameState& state,
                                                Player winner, Player opp) {
    Cell winner_cell = isBlackPlayer(winner) ? kBlack : kWhite;
    Cell opp_cell    = isBlackPlayer(opp)    ? kBlack : kWhite;

    bool in_alignment[19][19] = {};
    const int adirs[4][2] = {{1,0},{0,1},{1,1},{1,-1}};

    // Marquer toutes les pierres faisant partie d'un alignement de 5+
    for (int y = 0; y < 19; ++y) {
        for (int x = 0; x < 19; ++x) {
            if (state.board.get(y, x) != winner_cell) continue;
            for (auto& d : adirs) {
                int dx = d[0], dy = d[1];
                if (Rules::in_bounds(x - dx, y - dy) &&
                    state.board.get(y - dy, x - dx) == winner_cell) continue;
                int len = 0, cx = x, cy = y;
                while (Rules::in_bounds(cx, cy) && state.board.get(cy, cx) == winner_cell) {
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

    // Chercher si opp peut capturer une paire incluant une pierre de l'alignement
    const int cdirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,-1},{1,-1},{-1,1}};
    for (int y = 0; y < 19; ++y) {
        for (int x = 0; x < 19; ++x) {
            if (state.board.get(y, x) != kEmpty) continue;
            for (auto& cd : cdirs) {
                int dx = cd[0], dy = cd[1];
                int x1 = x + dx,     y1 = y + dy;
                int x2 = x + 2 * dx, y2 = y + 2 * dy;
                int x3 = x + 3 * dx, y3 = y + 3 * dy;
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
// Constructeur / Destructeur
// ─────────────────────────────────────────────────────────────────────────────
GameUI3D::GameUI3D()
    : cam_yaw(0.0f), cam_pitch(0.0f),
      running(true), menu_selection(0), current_state(UI3DState::MAIN_MENU),
      hovered_row(-1), hovered_col(-1), has_hover(false),
      vm_bob_time(0.0f), vm_recoil(0.0f), bolt_anim_timer_(0.0f),
      ai_pending_(false), ai_highlight_row_(-1), ai_highlight_col_(-1),
      ai_highlight_timer_(0.0f),
      pending_alignment_win_(false), pending_winner_(makePlayerNone()),
      ai_thread_active_(false), ai_result_ready_(false),
      ai_result_({-1, -1, 0, 0}) {
    InitWindow(1280, 720, "Gomoku FPS");
    ClearWindowState(FLAG_WINDOW_RESIZABLE);
    SetExitKey(KEY_NULL); // Désactive ESC = WindowShouldClose par défaut Raylib
    SetTargetFPS(60);
    init_camera();
}

GameUI3D::~GameUI3D() {
    CloseWindow();
}

// ─────────────────────────────────────────────────────────────────────────────
// init_camera — position fixe au-dessus du centre du goban
//
// Système de coordonnées monde :
//   Goban : coins à (0,0,0) et (18,18,0), centre à (9,9,0)
//   Caméra : position (9, 9, 22) → directement au-dessus du centre
//   up = (0,1,0) : Y est "le haut" de l'image
//   fovy = 55° : champ de vision vertical pour voir tout le goban
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::init_camera() {
    camera           = {};
    camera.position  = {9.0f, 9.0f, 22.0f};
    camera.target    = {9.0f, 9.0f,  0.0f};
    camera.up        = {0.0f, 1.0f,  0.0f};
    camera.fovy      = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    cam_yaw   = 0.0f;
    cam_pitch = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// update_camera_rotation — rotation FPS manuelle, position verrouillée
//
// GetMouseDelta() → delta pixels depuis la frame précédente.
// Yaw/pitch clampés pour garder le goban toujours visible.
// Direction de regard en coordonnées sphériques :
//   forward.x = sin(yaw) * cos(pitch)
//   forward.y = sin(pitch)
//   forward.z = -cos(yaw) * cos(pitch)   (Z négatif = vers le goban)
// camera.position reste FIXE. Seul camera.target change.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::update_camera_rotation() {
    Vector2 delta  = GetMouseDelta();
    cam_yaw   += delta.x * MOUSE_SENSITIVITY;
    cam_pitch -= delta.y * MOUSE_SENSITIVITY; // Inversé : souris bas = regard bas

    if (cam_yaw   >  CAM_YAW_LIMIT)   cam_yaw   =  CAM_YAW_LIMIT;
    if (cam_yaw   < -CAM_YAW_LIMIT)   cam_yaw   = -CAM_YAW_LIMIT;
    if (cam_pitch >  CAM_PITCH_LIMIT)  cam_pitch =  CAM_PITCH_LIMIT;
    if (cam_pitch < -CAM_PITCH_LIMIT)  cam_pitch = -CAM_PITCH_LIMIT;

    Vector3 forward;
    forward.x = sinf(cam_yaw)  * cosf(cam_pitch);
    forward.y = sinf(cam_pitch);
    forward.z = -cosf(cam_yaw) * cosf(cam_pitch);

    camera.position = {9.0f, 9.0f, 22.0f};
    camera.target   = {camera.position.x + forward.x,
                       camera.position.y + forward.y,
                       camera.position.z + forward.z};
    camera.up       = {0.0f, 1.0f, 0.0f};
}

// ─────────────────────────────────────────────────────────────────────────────
// run — boucle principale 3D
//
// DisableCursor() : capture la souris dans la fenêtre (FPS standard).
// Snapshot avant/après le coup pour détecter les captures animées.
// Cooldown IA 500ms : évite que l'IA réponde instantanément après le joueur.
// Application du résultat IA dans le thread principal (ai_result_ready_ atomique).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::run(GameState& state, Gomoku& gomoku) {
    DisableCursor();
    HideCursor();

    while (!WindowShouldClose() && running) {
        if (current_state == UI3DState::MAIN_MENU) {
            handle_menu_input();
            render_menu();
        } else {
            update_camera_rotation();
            has_hover = board3d_raycast(camera, hovered_row, hovered_col);

            // Snapshot avant input pour détecter les captures
            uint8_t  cap_b_before = state.board.blackCaptures;
            uint8_t  cap_w_before = state.board.whiteCaptures;
            BitBoard board_snap   = state.board;

            handle_game_input(state, gomoku);

            // Cooldown IA 500ms → lance le thread après expiration
            if (current_state == UI3DState::PLAYING_SOLO &&
                ai_pending_ && !state.game_over && !ai_thread_active_.load()) {
                auto   now        = std::chrono::high_resolution_clock::now();
                double elapsed_ms = std::chrono::duration<double, std::milli>(
                                        now - ai_pending_since_).count();
                if (elapsed_ms >= 500.0) {
                    ai_pending_ = false;
                    handle_ai_turn(state, gomoku);
                }
            }

            // Application du résultat IA (thread principal uniquement)
            if (ai_result_ready_.load() && !state.game_over) {
                ai_result_ready_.store(false);
                if (current_state == UI3DState::PLAYING_SOLO) {
                    state.last_ai_move_time_ms = (double)ai_result_.computeTimeMs;
                    state.place_stone(ai_result_.col, ai_result_.row);
                    ai_highlight_row_   = ai_result_.row;
                    ai_highlight_col_   = ai_result_.col;
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
            float dt = GetFrameTime();

            vm_bob_time += dt;

            if (vm_recoil > 0.0f) vm_recoil -= dt * 6.0f;
            if (vm_recoil < 0.0f) vm_recoil  = 0.0f;

            if (bolt_anim_timer_ > 0.0f) bolt_anim_timer_ -= dt;
            if (bolt_anim_timer_ < 0.0f) bolt_anim_timer_  = 0.0f;

            // Physique des douilles : gravité sur l'axe U
            for (auto& s : shell_casings_) {
                s.vu -= 4.0f * dt;
                s.r  += s.vr * dt;
                s.u  += s.vu * dt;
                s.f  += s.vf * dt;
                s.rot += s.rot_spd * dt;
                s.timer -= dt;
            }
            shell_casings_.erase(
                std::remove_if(shell_casings_.begin(), shell_casings_.end(),
                               [](const ShellCasing& s) { return s.timer <= 0.0f; }),
                shell_casings_.end());

            for (auto& a : capture_anims) a.timer -= dt;
            capture_anims.erase(
                std::remove_if(capture_anims.begin(), capture_anims.end(),
                               [](const CaptureAnim3D& a) { return a.timer <= 0.0f; }),
                capture_anims.end());

            if (ai_highlight_timer_ > 0.0f) ai_highlight_timer_ -= dt;

            render_scene(state);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// render_menu — menu principal 3D
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::render_menu() {
    BeginDrawing();
    ClearBackground({40, 40, 40, 255});

    int sw = GetScreenWidth();
    DrawText("GOMOKU FPS", sw / 2 - MeasureText("GOMOKU FPS", 60) / 2, 100, 60, WHITE);

    const char* opt0 = "Mode Solo (vs IA)";
    const char* opt1 = "Mode Multi (Local 1v1)";
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
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) menu_selection = (menu_selection + 1) % 2;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) menu_selection = (menu_selection - 1 + 2) % 2;
    if (IsKeyPressed(KEY_ENTER)) {
        current_state = (menu_selection == 0) ? UI3DState::PLAYING_SOLO
                                              : UI3DState::PLAYING_MULTI;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// handle_game_input — placement de pierre par clic gauche (FPS)
//
// Le clic place la pierre à la case VISÉE (réticule), pas à la position souris.
// bolt_anim_timer_ > 0 bloque tout nouveau tir pendant l'animation du verrou.
// En Solo : bloque pendant le tour de l'IA (WHITE) et pendant le thread IA.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::handle_game_input(GameState& state, Gomoku& gomoku) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        current_state = UI3DState::MAIN_MENU;
        state.reset();
        init_camera();
        ai_pending_ = false;
        bolt_anim_timer_ = 0.0f;
        pending_alignment_win_ = false;
        pending_winner_ = makePlayerNone();
        ai_result_ready_.store(false);
        shell_casings_.clear();
        return;
    }

    if (state.game_over) {
        if (IsKeyPressed(KEY_R)) {
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

    // En Solo : bloquer les clics pendant le tour de l'IA (WHITE)
    if (current_state == UI3DState::PLAYING_SOLO && !isBlackPlayer(state.current_player))
        return;

    // Bloquer pendant l'animation bolt-action
    if (bolt_anim_timer_ > 0.0f)
        return;

    // Bloquer pendant que le thread IA réfléchit
    if (ai_thread_active_.load())
        return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && has_hover) {
        if (Rules::is_valid_move(state, hovered_col, hovered_row)) {
            if (state.place_stone(hovered_col, hovered_row)) {
                // Animations de tir
                vm_recoil        = 1.0f;
                bolt_anim_timer_ = BOLT_ANIM_DURATION;

                // Éjecter une douille
                ShellCasing s;
                s.r = -0.02f; s.u = 0.03f;  s.f = 0.05f;
                s.vr = -0.40f; s.vu = 0.35f; s.vf = -0.10f;
                s.timer = 0.70f; s.rot = 0.0f; s.rot_spd = 600.0f;
                shell_casings_.push_back(s);

                apply_win_check(state);

                if (!state.game_over) {
                    if (current_state == UI3DState::PLAYING_SOLO) {
                        ai_pending_       = true;
                        ai_pending_since_ = std::chrono::high_resolution_clock::now();
                    } else {
                        // Multi : suggestion IA dans un thread
                        if (!ai_thread_active_.load()) {
                            ai_thread_active_.store(true);
                            ai_result_ready_.store(false);
                            BitBoard board_copy = state.board;
                            Cell     c          = playerToCell(state.current_player);
                            if (ai_thread_.joinable()) ai_thread_.join();
                            ai_thread_ = std::thread([this, board_copy, c, &gomoku]() mutable {
                                Move result = gomoku.getBestMove(board_copy, c);
                                ai_result_  = result;
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
// getBestMove() peut prendre jusqu'à ~500ms. Sans thread, la boucle principale
// est gelée : la caméra ne répond plus. Avec un thread détaché, la boucle
// continue à 60fps. Le résultat est appliqué dans run() dès que
// ai_result_ready_ passe à true.
//
// Sécurité thread :
//   board_copy : copie par valeur → le thread travaille sur ses propres données.
//   ai_result_ / ai_result_ready_ / ai_thread_active_ : atomiques.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::handle_ai_turn(GameState& state, Gomoku& gomoku) {
    if (ai_thread_active_.load()) return;

    ai_thread_active_.store(true);
    ai_result_ready_.store(false);

    BitBoard board_copy = state.board;
    Cell     ai_cell    = playerToCell(state.current_player);

    if (ai_thread_.joinable()) ai_thread_.join();

    ai_thread_ = std::thread([this, board_copy, ai_cell, &gomoku]() mutable {
        auto t_start = std::chrono::high_resolution_clock::now();
        Move result  = gomoku.getBestMove(board_copy, ai_cell);
        auto t_end   = std::chrono::high_resolution_clock::now();

        result.computeTimeMs = (long)std::chrono::duration<double, std::milli>(
                                   t_end - t_start).count();
        ai_result_ = result;
        ai_result_ready_.store(true);
        ai_thread_active_.store(false);
    });
    ai_thread_.detach();
}

// ─────────────────────────────────────────────────────────────────────────────
// apply_win_check — vérification de victoire avec règle endgame capture
//
// Portage fidèle de la logique 2D (Input.cpp::apply_win_check).
// place_stone() a déjà switché current_player, donc "qui vient de jouer"
// = l'opposé de current_player.
//
// Cas 1 — pending_alignment_win_ actif (l'adversaire avait une chance) :
//   Si l'alignement est encore intact → le pending_winner_ gagne.
//   Sinon → annuler le pending et réévaluer.
//
// Cas 2 — pas de pending :
//   a) Victoire par capture : immédiate.
//   b) Victoire par alignement : immédiate SAUF si l'adversaire peut capturer
//      une paire de l'alignement → alors pending (l'adversaire a un tour).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::apply_win_check(GameState& state) {
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
            if (!can_opponent_capture_from_alignment(state, just_played, next_player)) {
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
// render_scene — pipeline de rendu 3 passes
//
// Pass 1 — Scène monde (BeginMode3D / EndMode3D)
//   Z-buffer actif. Toute la géométrie 3D du goban.
//
// Pass 2 — Viewmodel (draw_viewmodel_3d)
//   Espace caméra virtuel séparé. rlDisableDepthTest() intégré dans la fonction.
//
// Pass 3 — HUD 2D (rlDisableDepthTest)
//   Coordonnées écran. Jamais occulté par la scène 3D.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI3D::render_scene(const GameState& state) {
    BeginDrawing();
    ClearBackground({30, 30, 30, 255});

    // ── Pass 1 : scène monde ──────────────────────────────────────────────────
    BeginMode3D(camera);
    draw_board_3d();
    draw_stones_3d(state);
    draw_ai_highlight_3d(ai_highlight_row_, ai_highlight_col_, ai_highlight_timer_,
                         current_state == UI3DState::PLAYING_SOLO);
    draw_hover_indicator(state, has_hover, hovered_row, hovered_col,
                         current_state == UI3DState::PLAYING_SOLO);
    draw_best_move_3d(state);
    EndMode3D();

    // ── Pass 2 : viewmodel ───────────────────────────────────────────────────
    draw_viewmodel_3d(camera, vm_bob_time, vm_recoil, bolt_anim_timer_,
                      shell_casings_);

    // ── Pass 3 : HUD 2D ───────────────────────────────────────────────────────
    rlDisableDepthTest();
    draw_crosshair(has_hover);
    draw_hud(state, current_state, has_hover, hovered_row, hovered_col);
    draw_capture_anims_3d(capture_anims, camera);
    draw_history(state);
    if (state.game_over)
        draw_game_over_overlay(state, current_state);

    EndDrawing();
}
