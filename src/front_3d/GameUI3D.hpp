#pragma once

#include <chrono>

// Engine headers MUST come before raylib to avoid BLACK/WHITE macro conflicts
#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "../engine/Rules.hpp"

#include "raylib.h"

enum class UI3DState { MAIN_MENU, PLAYING_SOLO, PLAYING_MULTI };

class GameUI3D {
public:
    GameUI3D();
    ~GameUI3D();

    void run(GameState &state, Gomoku &gomoku);

private:
    Camera3D camera;
    bool running;
    int menu_selection;
    UI3DState current_state;

    // Board geometry
    static constexpr float BOARD_WORLD_SIZE = 18.0f;
    static constexpr float CELL_WORLD_SIZE = BOARD_WORLD_SIZE / 18.0f;
    static constexpr float STONE_RADIUS = 0.35f;

    // Camera rotation (manual FPS, clamped to keep board visible)
    float cam_yaw;    // horizontal angle (radians), 0 = looking at board center
    float cam_pitch;  // vertical angle (radians), 0 = looking straight ahead
    static constexpr float CAM_YAW_LIMIT   = 0.75f;  // ~43 degrees
    static constexpr float CAM_PITCH_LIMIT = 0.60f;   // ~34 degrees
    static constexpr float MOUSE_SENSITIVITY = 0.003f;

    // Raycast
    int hovered_row;
    int hovered_col;
    bool has_hover;

    // Viewmodel animation
    float vm_bob_time;      // running timer for idle bob
    float vm_recoil;        // recoil amount (decays over time)

    // Capture animations
    struct CaptureAnim3D { int row, col; float timer; };
    std::vector<CaptureAnim3D> capture_anims;

    // AI cooldown (solo mode only)
    bool ai_pending_;
    std::chrono::high_resolution_clock::time_point ai_pending_since_;

    // AI last move highlight (solo mode, 0.5s)
    int   ai_highlight_row_;
    int   ai_highlight_col_;
    float ai_highlight_timer_;

    // Camera setup
    void init_camera();
    void update_camera_rotation();

    // Menu
    void render_menu();
    void handle_menu_input();

    // Gameplay
    void handle_game_input(GameState &state, Gomoku &gomoku);
    void handle_ai_turn(GameState &state, Gomoku &gomoku);
    void check_game_over(GameState &state);

    // Raycast: screen center -> board intersection
    bool raycast_to_board(int &out_row, int &out_col);

    // 3D rendering
    void render_scene(const GameState &state);
    void draw_board_3d();
    void draw_stones_3d(const GameState &state);
    void draw_ai_highlight_3d();
    void draw_hover_indicator(const GameState &state);
    void draw_best_move_3d(const GameState &state);

    // Viewmodel (3D weapon + hands + arms, rendered in separate pass)
    void draw_viewmodel_3d(const GameState &state);

    // HUD overlay (2D on top of 3D)
    void draw_crosshair();
    void draw_hud(const GameState &state);
    void draw_game_over_overlay(const GameState &state);
    void draw_history(const GameState &state);
    void draw_capture_anims_3d();
};
