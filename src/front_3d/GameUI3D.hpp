#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameUI3D.hpp — Vue 3D (Raylib), mode FPS bonus
//
// Activé par : ./Gomoku --fps
//
// ⚠ ORDRE D'INCLUSION CRITIQUE — conflit de macros Raylib / engine ⚠
// Raylib définit BLACK et WHITE comme des Color{} C-style :
//   #define BLACK  CLITERAL(Color){ 0, 0, 0, 255 }
//   #define WHITE  CLITERAL(Color){ 255, 255, 255, 255 }
// L'engine utilise BLACK et WHITE comme valeurs de l'enum Cell.
// → L'engine DOIT être inclus AVANT raylib.h.
// → Les valeurs Cell sont sauvegardées dans kBlack/kWhite/kEmpty dans le .cpp
//   AVANT l'inclusion de raylib, puis utilisées partout à leur place.
// ─────────────────────────────────────────────────────────────────────────────

#include <chrono>

// Engine avant raylib (voir note ci-dessus)
#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "../engine/Rules.hpp"

#include "raylib.h"

// Machine à états de l'UI 3D (plus simple que le 2D : pas de Bot vs Bot)
enum class UI3DState { MAIN_MENU, PLAYING_SOLO, PLAYING_MULTI };

class GameUI3D {
public:
    GameUI3D();   // InitWindow + init_camera
    ~GameUI3D();  // CloseWindow

    void run(GameState &state, Gomoku &gomoku);

private:
    Camera3D camera;        // Caméra Raylib : position, target, up, fovy, projection
    bool     running;
    int      menu_selection;
    UI3DState current_state;

    // ── Géométrie du goban en espace monde ───────────────────────────────────
    // Le goban 18×18 est mappé sur un carré de BOARD_WORLD_SIZE unités monde.
    // CELL_WORLD_SIZE = 1.0f (18/18), ce qui simplifie les conversions :
    //   col → x_world = (float)col
    //   row → y_world = 18.0f - (float)row  (Y inversé : row=0 en haut → Y=18)
    static constexpr float BOARD_WORLD_SIZE = 18.0f;
    static constexpr float CELL_WORLD_SIZE  = BOARD_WORLD_SIZE / 18.0f; // = 1.0f
    static constexpr float STONE_RADIUS     = 0.35f; // Rayon des sphères (unités monde)

    // ── Rotation caméra FPS manuelle ──────────────────────────────────────────
    // La caméra a une POSITION FIXE {9, 9, 22} (centre goban, hauteur 22u).
    // Seul le TARGET change selon yaw/pitch.
    // cam_yaw   : rotation horizontale (autour de Y), 0 = face au centre
    // cam_pitch : rotation verticale (autour de X local), 0 = horizontal
    float cam_yaw;
    float cam_pitch;
    // Limites de clamp pour garder le goban toujours visible
    static constexpr float CAM_YAW_LIMIT     = 0.75f;  // ≈ ±43°
    static constexpr float CAM_PITCH_LIMIT   = 0.60f;  // ≈ ±34°
    static constexpr float MOUSE_SENSITIVITY = 0.003f; // rad/pixel

    // ── Raycast (détection de la case visée) ──────────────────────────────────
    // Chaque frame : GetScreenToWorldRay depuis le centre de l'écran →
    // intersection avec le plan Z=0 → (col, row).
    int  hovered_row;
    int  hovered_col;
    bool has_hover; // false si le rayon ne touche pas le plateau

    // ── Animation du viewmodel (arme à la première personne) ─────────────────
    // vm_bob_time   : timer croissant pour le balancement idle (sinusoïde)
    // vm_recoil     : intensité du recul après un tir (décroît à 6x dt)
    float vm_bob_time;
    float vm_recoil;

    // Animation bolt-action (verrou coulissant post-tir)
    float bolt_anim_timer_;
    static constexpr float BOLT_ANIM_DURATION = 0.60f; // secondes

    // ── Douilles éjectées (physique simple) ───────────────────────────────────
    // Position/vitesse en coordonnées viewmodel (r=droite, u=haut, f=avant).
    // Gravité simulée : vu -= 4.0f * dt chaque frame.
    struct ShellCasing {
        float r, u, f;      // Position offset depuis le grip
        float vr, vu, vf;   // Vitesse initiale
        float timer;        // Durée de vie (0.7s)
        float rot;          // Angle de rotation visuel (degrés)
        float rot_spd;      // Vitesse de rotation (degrés/s)
    };
    std::vector<ShellCasing> shell_casings_;

    // ── Animations de capture ─────────────────────────────────────────────────
    // Identique au 2D : croix X en coordonnées écran (via GetWorldToScreen).
    struct CaptureAnim3D { int row, col; float timer; };
    std::vector<CaptureAnim3D> capture_anims;

    // ── Cooldown IA (Solo uniquement) ─────────────────────────────────────────
    // Même principe qu'en 2D : 500ms après le coup humain avant que l'IA réponde.
    bool ai_pending_;
    std::chrono::high_resolution_clock::time_point ai_pending_since_;

    // Surbrillance du dernier coup IA (anneau rouge, 0.5s)
    int   ai_highlight_row_;
    int   ai_highlight_col_;
    float ai_highlight_timer_;

    // ── Méthodes privées ──────────────────────────────────────────────────────

    // Caméra
    void init_camera();             // Position fixe {9,9,22}, target vers le centre
    void update_camera_rotation();  // Lit GetMouseDelta(), met à jour yaw/pitch/target

    // Menu
    void render_menu();
    void handle_menu_input();

    // Gameplay
    void handle_game_input(GameState &state, Gomoku &gomoku); // Input + placement
    void handle_ai_turn(GameState &state, Gomoku &gomoku);    // Appel getBestMove + timer
    void check_game_over(GameState &state); // Simplifié : sans règle endgame capture

    // Raycast : centre écran → case du goban
    bool raycast_to_board(int &out_row, int &out_col);

    // ── Pipeline de rendu (3 passes) ─────────────────────────────────────────
    // Pass 1 : BeginMode3D → scène monde (plateau, pierres, indicateurs)
    // Pass 2 : draw_viewmodel_3d() → arme en espace caméra (depth buffer vidé)
    // Pass 3 : rlDisableDepthTest() → HUD 2D overlay (jamais occulté)
    void render_scene(const GameState &state);

    // Pass 1 — scène monde
    void draw_board_3d();
    void draw_stones_3d(const GameState &state);
    void draw_ai_highlight_3d();
    void draw_hover_indicator(const GameState &state);
    void draw_best_move_3d(const GameState &state);

    // Pass 2 — viewmodel
    void draw_viewmodel_3d(); // Rendu dans un vm_cam séparé, FOV 65°

    // Pass 3 — HUD 2D
    void draw_crosshair();
    void draw_hud(const GameState &state);
    void draw_game_over_overlay(const GameState &state);
    void draw_history(const GameState &state);
    void draw_capture_anims_3d();
};
