#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// GameUI3D.hpp — Orchestrateur de la vue 3D FPS (Raylib)
//
// Activé par : ./Gomoku --fps
//
// ⚠ ORDRE D'INCLUSION CRITIQUE — conflit de macros Raylib / engine ⚠
// Raylib définit BLACK et WHITE comme des Color{} C-style.
// L'engine utilise BLACK et WHITE comme valeurs de l'enum Cell.
// → L'engine DOIT être inclus AVANT raylib.h.
// → Dans GameUI3D.cpp, les valeurs Cell sont sauvegardées dans kBlack/kWhite/kEmpty
//   AVANT l'inclusion de raylib.
// ─────────────────────────────────────────────────────────────────────────────

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "raylib.h"

#include "front3d_types.hpp"

class GameUI3D {
public:
    GameUI3D();   // InitWindow + init_camera
    ~GameUI3D();  // CloseWindow

    void run(GameState& state, Gomoku& gomoku);

private:
    // ── Caméra ────────────────────────────────────────────────────────────────
    Camera3D camera;
    float    cam_yaw;   // Rotation horizontale (autour de Y)
    float    cam_pitch; // Rotation verticale (autour de X local)

    static constexpr float CAM_YAW_LIMIT      = 0.75f;   // ≈ ±43°
    static constexpr float CAM_PITCH_LIMIT     = 0.60f;   // ≈ ±34°
    static constexpr float MOUSE_SENSITIVITY   = 0.003f;  // rad/pixel

    // ── État général ──────────────────────────────────────────────────────────
    bool       running;
    int        menu_selection;
    UI3DState  current_state;

    // ── Raycast (case visée par le réticule) ──────────────────────────────────
    int  hovered_row;
    int  hovered_col;
    bool has_hover;

    // ── Animations ────────────────────────────────────────────────────────────
    float vm_bob_time;    // Timer balancement idle (sinusoïde)
    float vm_recoil;      // Intensité du recul [0..1], décroît à 6× dt
    float bolt_anim_timer_; // Durée restante animation bolt-action

    std::vector<ShellCasing>    shell_casings_;
    std::vector<CaptureAnim3D>  capture_anims;

    // ── Tour IA — cooldown + surbrillance ─────────────────────────────────────
    bool                                         ai_pending_;
    std::chrono::high_resolution_clock::time_point ai_pending_since_;
    int   ai_highlight_row_;
    int   ai_highlight_col_;
    float ai_highlight_timer_;

    // ── Règle endgame capture ─────────────────────────────────────────────────
    // Un alignement de 5 ne gagne que si l'adversaire ne peut pas capturer
    // immédiatement une paire de cet alignement. Si c'est possible, il a un tour.
    bool   pending_alignment_win_;
    Player pending_winner_;

    // ── Thread IA (Solo uniquement) ───────────────────────────────────────────
    std::thread          ai_thread_;
    std::atomic<bool>    ai_thread_active_;
    std::atomic<bool>    ai_result_ready_;
    Move                 ai_result_;

    // ── Méthodes privées ──────────────────────────────────────────────────────
    void init_camera();
    void update_camera_rotation();

    void render_menu();
    void handle_menu_input();

    void handle_game_input(GameState& state, Gomoku& gomoku);
    void handle_ai_turn(GameState& state, Gomoku& gomoku);
    void apply_win_check(GameState& state);

    void render_scene(const GameState& state);
};
