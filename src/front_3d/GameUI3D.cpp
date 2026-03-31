// Engine headers MUST be included before raylib (BLACK/WHITE macro conflict)
#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "../engine/Rules.hpp"

// Save Cell enum values before raylib overwrites them
static constexpr Cell kBlack = BLACK;
static constexpr Cell kWhite = WHITE;
static constexpr Cell kEmpty = EMPTY;

// Now safe to include raylib
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "GameUI3D.hpp"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <string>

// We can't write Player::BLACK in code because raylib's macro expands it.
static inline bool isBlackPlayer(Player p) { return (int)p == 1; }

static Cell playerToCell(Player p) {
    return isBlackPlayer(p) ? kBlack : kWhite;
}

static inline Player makePlayerBlack() { return static_cast<Player>(1); }
static inline Player makePlayerWhite() { return static_cast<Player>(2); }

// ─── Weapon color helper ─────────────────────────────────────────
static Color getWeaponColor(Player p) {
    return isBlackPlayer(p) ? Color{30, 30, 30, 255} : Color{230, 230, 230, 255};
}

GameUI3D::GameUI3D()
    : running(true), menu_selection(0), current_state(UI3DState::MAIN_MENU),
      cam_yaw(0.0f), cam_pitch(0.0f),
      hovered_row(-1), hovered_col(-1), has_hover(false),
      vm_bob_time(0.0f), vm_recoil(0.0f),
      bolt_anim_timer_(0.0f),
      ai_pending_(false),
      ai_highlight_row_(-1), ai_highlight_col_(-1), ai_highlight_timer_(0.0f) {
    InitWindow(1280, 720, "Gomoku FPS");
    ClearWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    init_camera();
}

GameUI3D::~GameUI3D() {
    CloseWindow();
}

void GameUI3D::init_camera() {
    camera = {};
    camera.position = {9.0f, 9.0f, 22.0f};
    camera.target = {9.0f, 9.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    cam_yaw = 0.0f;
    cam_pitch = 0.0f;
}

// ─── Manual camera rotation (clamped to keep board in view) ─────
void GameUI3D::update_camera_rotation() {
    Vector2 delta = GetMouseDelta();
    cam_yaw   += delta.x * MOUSE_SENSITIVITY;
    cam_pitch -= delta.y * MOUSE_SENSITIVITY;

    // Clamp to prevent looking away from the board
    if (cam_yaw > CAM_YAW_LIMIT)    cam_yaw = CAM_YAW_LIMIT;
    if (cam_yaw < -CAM_YAW_LIMIT)   cam_yaw = -CAM_YAW_LIMIT;
    if (cam_pitch > CAM_PITCH_LIMIT) cam_pitch = CAM_PITCH_LIMIT;
    if (cam_pitch < -CAM_PITCH_LIMIT) cam_pitch = -CAM_PITCH_LIMIT;

    // Compute look direction from yaw/pitch
    // Base direction: from camera (Z=22) toward board center (Z=0) => (0, 0, -1)
    Vector3 forward;
    forward.x = sinf(cam_yaw) * cosf(cam_pitch);
    forward.y = sinf(cam_pitch);
    forward.z = -cosf(cam_yaw) * cosf(cam_pitch);

    camera.position = {9.0f, 9.0f, 22.0f};
    camera.target = {
        camera.position.x + forward.x,
        camera.position.y + forward.y,
        camera.position.z + forward.z
    };
    camera.up = {0.0f, 1.0f, 0.0f};
}

// ─── Main loop ───────────────────────────────────────────────────

void GameUI3D::run(GameState &state, Gomoku &gomoku) {
    DisableCursor();
    HideCursor();

    while (!WindowShouldClose() && running) {
        if (current_state == UI3DState::MAIN_MENU) {
            handle_menu_input();
            render_menu();
        } else {
            update_camera_rotation();

            has_hover = raycast_to_board(hovered_row, hovered_col);

            // Snapshot captures and board before processing input
            uint8_t cap_b_before = state.board.blackCaptures;
            uint8_t cap_w_before = state.board.whiteCaptures;
            BitBoard board_snap = state.board;

            handle_game_input(state, gomoku);

            // AI cooldown: fire after 0.5s (solo mode only)
            if (current_state == UI3DState::PLAYING_SOLO && ai_pending_ && !state.game_over) {
                auto now = std::chrono::high_resolution_clock::now();
                double elapsed_ms = std::chrono::duration<double, std::milli>(now - ai_pending_since_).count();
                if (elapsed_ms >= 500.0) {
                    ai_pending_ = false;
                    handle_ai_turn(state, gomoku);
                }
            }

            // Detect newly captured stones and register animations
            if (state.board.blackCaptures != cap_b_before || state.board.whiteCaptures != cap_w_before) {
                for (int r = 0; r < 19; r++)
                    for (int c = 0; c < 19; c++)
                        if (board_snap.get(r, c) != kEmpty && state.board.get(r, c) == kEmpty)
                            capture_anims.push_back({r, c, 1.0f});
            }

            // Viewmodel animation timers
            float dt = GetFrameTime();
            vm_bob_time += dt;
            if (vm_recoil > 0.0f) vm_recoil -= dt * 6.0f;
            if (vm_recoil < 0.0f) vm_recoil = 0.0f;

            // Bolt-action timer
            if (bolt_anim_timer_ > 0.0f) bolt_anim_timer_ -= dt;
            if (bolt_anim_timer_ < 0.0f) bolt_anim_timer_ = 0.0f;

            // Shell casing physics (gravity in viewmodel up-axis)
            for (auto &s : shell_casings_) {
                s.vu  -= 4.0f * dt;
                s.r   += s.vr * dt;
                s.u   += s.vu * dt;
                s.f   += s.vf * dt;
                s.rot += s.rot_spd * dt;
                s.timer -= dt;
            }
            shell_casings_.erase(
                std::remove_if(shell_casings_.begin(), shell_casings_.end(),
                    [](const ShellCasing &s) { return s.timer <= 0.0f; }),
                shell_casings_.end());

            // Update capture animation timers
            for (auto &a : capture_anims)
                a.timer -= dt;
            capture_anims.erase(
                std::remove_if(capture_anims.begin(), capture_anims.end(),
                    [](const CaptureAnim3D &a) { return a.timer <= 0.0f; }),
                capture_anims.end());

            // Update AI highlight timer
            if (ai_highlight_timer_ > 0.0f) ai_highlight_timer_ -= dt;

            render_scene(state);
        }
    }
}

// ─── Menu ────────────────────────────────────────────────────────

void GameUI3D::render_menu() {
    BeginDrawing();
    ClearBackground({40, 40, 40, 255});

    int sw = GetScreenWidth();

    DrawText("GOMOKU FPS", sw / 2 - MeasureText("GOMOKU FPS", 60) / 2, 100, 60, WHITE);

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

// ─── Input ───────────────────────────────────────────────────────

void GameUI3D::handle_game_input(GameState &state, Gomoku &gomoku) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        current_state = UI3DState::MAIN_MENU;
        state.reset();
        init_camera();
        ai_pending_ = false;
        bolt_anim_timer_ = 0.0f;
        shell_casings_.clear();
        return;
    }

    if (state.game_over) {
        if (IsKeyPressed(KEY_R)) {
            state.reset();
            ai_pending_ = false;
            bolt_anim_timer_ = 0.0f;
            shell_casings_.clear();
        }
        return;
    }

    if (current_state == UI3DState::PLAYING_SOLO &&
        !isBlackPlayer(state.current_player)) {
        return;
    }

    // Bolt animation blocks any new shot
    if (bolt_anim_timer_ > 0.0f)
        return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && has_hover) {
        if (Rules::is_valid_move(state, hovered_col, hovered_row)) {
            if (state.place_stone(hovered_col, hovered_row)) {
                vm_recoil = 1.0f;
                bolt_anim_timer_ = BOLT_ANIM_DURATION;

                // Eject a shell casing (left side of receiver, slight upward kick)
                ShellCasing s;
                s.r = -0.02f; s.u = 0.03f; s.f = 0.05f;
                s.vr = -0.40f; s.vu = 0.35f; s.vf = -0.10f;
                s.timer = 0.70f;
                s.rot = 0.0f; s.rot_spd = 600.0f;
                shell_casings_.push_back(s);

                check_game_over(state);

                if (!state.game_over) {
                    if (current_state == UI3DState::PLAYING_SOLO) {
                        // 0.5s cooldown before AI responds
                        ai_pending_ = true;
                        ai_pending_since_ = std::chrono::high_resolution_clock::now();
                    } else {
                        Cell c = playerToCell(state.current_player);
                        state.best_move_suggestion = gomoku.getBestMove(state.board, c);
                    }
                }
            }
        }
    }
}

void GameUI3D::handle_ai_turn(GameState &state, Gomoku &gomoku) {
    Cell ai_cell = playerToCell(state.current_player);

    auto t_start = std::chrono::high_resolution_clock::now();
    Move ai_move = gomoku.getBestMove(state.board, ai_cell);
    auto t_end = std::chrono::high_resolution_clock::now();

    double elapsed_ms =
        std::chrono::duration<double, std::milli>(t_end - t_start).count();
    state.last_ai_move_time_ms = elapsed_ms;

    state.place_stone(ai_move.col, ai_move.row);
    ai_highlight_row_   = ai_move.row;
    ai_highlight_col_   = ai_move.col;
    ai_highlight_timer_ = 0.5f;
    check_game_over(state);
}

void GameUI3D::check_game_over(GameState &state) {
    if (Rules::check_win_condition(state, makePlayerBlack()) ||
        Rules::check_win_by_capture(state, makePlayerBlack()) ||
        Rules::check_win_condition(state, makePlayerWhite()) ||
        Rules::check_win_by_capture(state, makePlayerWhite())) {
        state.game_over = true;
        state.best_move_suggestion = {-1, -1, 0, 0};
    }
}

// ─── Raycast ─────────────────────────────────────────────────────

bool GameUI3D::raycast_to_board(int &out_row, int &out_col) {
    Ray ray = GetScreenToWorldRay(
        {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f}, camera);

    if (fabsf(ray.direction.z) < 0.0001f)
        return false;

    float t = -ray.position.z / ray.direction.z;
    if (t < 0.0f)
        return false;

    float hit_x = ray.position.x + t * ray.direction.x;
    float hit_y = ray.position.y + t * ray.direction.y;

    int col = (int)roundf(hit_x);
    int row = (int)roundf(18.0f - hit_y);

    if (col < 0 || col >= 19 || row < 0 || row >= 19)
        return false;

    out_row = row;
    out_col = col;
    return true;
}

// ─── Board coordinate helpers ────────────────────────────────────
static inline Vector3 boardToWorld(int row, int col, float z_offset = 0.0f) {
    return {(float)col, 18.0f - (float)row, z_offset};
}

// ─── 3D Rendering ────────────────────────────────────────────────

void GameUI3D::render_scene(const GameState &state) {
    BeginDrawing();
    ClearBackground({30, 30, 30, 255});

    // ── Pass 1: World scene ──
    BeginMode3D(camera);
    draw_board_3d();
    draw_stones_3d(state);
    draw_ai_highlight_3d();
    draw_hover_indicator(state);
    draw_best_move_3d(state);
    EndMode3D();

    // ── Pass 2: Viewmodel (weapon + hands + arms) ──
    draw_viewmodel_3d(state);

    // ── 2D HUD overlay ──
    draw_crosshair();
    draw_hud(state);
    draw_capture_anims_3d();
    draw_history(state);

    if (state.game_over)
        draw_game_over_overlay(state);

    EndDrawing();
}

void GameUI3D::draw_board_3d() {
    // Board base — flat quad (no DrawCube shading artifacts)
    Color base_col = {180, 140, 70, 255};
    Vector3 b0 = {-1.0f, -1.0f, -0.2f};
    Vector3 b1 = {19.0f, -1.0f, -0.2f};
    Vector3 b2 = {19.0f, 19.0f, -0.2f};
    Vector3 b3 = {-1.0f, 19.0f, -0.2f};
    DrawTriangle3D(b0, b1, b2, base_col);
    DrawTriangle3D(b0, b2, b3, base_col);

    // Playing surface — flat quad, uniform color
    Color surface_col = {220, 179, 92, 255};
    float m = 0.25f;
    Vector3 s0 = {-m,        -m,        0.0f};
    Vector3 s1 = {18.0f + m, -m,        0.0f};
    Vector3 s2 = {18.0f + m, 18.0f + m, 0.0f};
    Vector3 s3 = {-m,        18.0f + m, 0.0f};
    DrawTriangle3D(s0, s1, s2, surface_col);
    DrawTriangle3D(s0, s2, s3, surface_col);

    // Grid lines
    Color lineColor = {0, 0, 0, 255};
    for (int i = 0; i < 19; i++) {
        float y = (float)i;
        DrawLine3D({0.0f, y, 0.01f}, {18.0f, y, 0.01f}, lineColor);
        float x = (float)i;
        DrawLine3D({x, 0.0f, 0.01f}, {x, 18.0f, 0.01f}, lineColor);
    }

    // Hoshi points
    int hoshi[][2] = {{3,3},{3,9},{3,15},{9,3},{9,9},{9,15},{15,3},{15,9},{15,15}};
    for (auto &h : hoshi) {
        Vector3 pos = boardToWorld(h[0], h[1], 0.02f);
        DrawSphere(pos, 0.12f, DARKGRAY);
    }
}

void GameUI3D::draw_stones_3d(const GameState &state) {
    for (int row = 0; row < 19; row++) {
        for (int col = 0; col < 19; col++) {
            Cell cell = state.board.get(row, col);
            if (cell == kEmpty)
                continue;

            Vector3 pos = boardToWorld(row, col, STONE_RADIUS * 0.6f);
            Color color = (cell == kBlack)
                              ? Color{20, 20, 20, 255}
                              : Color{240, 240, 240, 255};
            DrawSphere(pos, STONE_RADIUS, color);
        }
    }
}

void GameUI3D::draw_ai_highlight_3d() {
    if (current_state != UI3DState::PLAYING_SOLO) return;
    if (ai_highlight_timer_ <= 0.0f || ai_highlight_row_ < 0) return;

    float alpha = ai_highlight_timer_ / 0.5f; // 1.0 -> 0.0
    unsigned char a = (unsigned char)(alpha * 255);
    Color ring_col = {220, 50, 50, a};

    Vector3 pos = boardToWorld(ai_highlight_row_, ai_highlight_col_, 0.08f);
    DrawCircle3D(pos, 0.48f, {0.0f, 0.0f, 1.0f}, 0.0f, ring_col);
    // Second ring slightly larger for visibility
    DrawCircle3D(pos, 0.52f, {0.0f, 0.0f, 1.0f}, 0.0f,
                 Color{220, 50, 50, (unsigned char)(a / 2)});
}

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
                      ? Color{20, 20, 20, 100}
                      : Color{240, 240, 240, 100};
    DrawSphere(pos, STONE_RADIUS, ghost);

    Vector3 ring_pos = boardToWorld(hovered_row, hovered_col, 0.05f);
    DrawCircle3D(ring_pos, 0.45f, {0.0f, 0.0f, 1.0f}, 0.0f, GREEN);
}

void GameUI3D::draw_best_move_3d(const GameState &state) {
    if (state.game_over)
        return;
    if (state.best_move_suggestion.row < 0 || state.best_move_suggestion.col < 0)
        return;

    Vector3 pos = boardToWorld(
        state.best_move_suggestion.row,
        state.best_move_suggestion.col,
        0.05f);

    float pulse = (sinf((float)GetTime() * 4.0f) + 1.0f) * 0.5f;
    unsigned char alpha = (unsigned char)(80 + pulse * 80);
    Color suggest_color = {0, 200, 255, alpha};

    DrawCircle3D(pos, 0.5f, {0.0f, 0.0f, 1.0f}, 0.0f, suggest_color);
}

// ─── 3D Viewmodel ────────────────────────────────────────────────
// Renders weapon + hands + forearms in a separate 3D pass.
// Depth buffer is cleared so the viewmodel always renders on top.

void GameUI3D::draw_viewmodel_3d(const GameState &state) {
    rlDrawRenderBatchActive();

    // Camera basis vectors
    Vector3 cam_dir   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam_dir, camera.up));
    Vector3 cam_up    = Vector3Normalize(Vector3CrossProduct(cam_right, cam_dir));

    // Dedicated viewmodel camera (origin at 0,0,0, FOV slightly wider)
    Camera3D vm_cam = {};
    vm_cam.position   = {0.0f, 0.0f, 0.0f};
    vm_cam.target     = cam_dir;
    vm_cam.up         = cam_up;
    vm_cam.fovy       = 65.0f;
    vm_cam.projection = CAMERA_PERSPECTIVE;

    // Idle bob
    float bob_x = sinf(vm_bob_time * 1.8f) * 0.015f;
    float bob_y = sinf(vm_bob_time * 3.6f) * 0.008f;

    // Recoil (sniper kick: stronger back push)
    float recoil_back = vm_recoil * 0.18f;
    float recoil_up   = vm_recoil * 0.07f;

    // Grip position (pistol grip of the AWP, bottom-right of screen)
    float base_right   =  0.30f + bob_x;
    float base_down    = -0.22f + bob_y;
    float base_forward =  0.55f - recoil_back;

    Vector3 grip = Vector3Scale(cam_right, base_right);
    grip = Vector3Add(grip, Vector3Scale(cam_up, base_down));
    grip = Vector3Add(grip, Vector3Scale(cam_dir, base_forward));
    grip = Vector3Add(grip, Vector3Scale(cam_up, recoil_up));

    // Colors
    Color weapon_col = getWeaponColor(state.current_player);
    Color weapon_dark = {
        (unsigned char)(weapon_col.r * 0.45f),
        (unsigned char)(weapon_col.g * 0.45f),
        (unsigned char)(weapon_col.b * 0.45f), 255};
    Color weapon_light = {
        (unsigned char)((int)weapon_col.r + (255 - (int)weapon_col.r) / 3),
        (unsigned char)((int)weapon_col.g + (255 - (int)weapon_col.g) / 3),
        (unsigned char)((int)weapon_col.b + (255 - (int)weapon_col.b) / 3), 255};
    Color skin_col    = {210, 170, 130, 255};
    Color skin_dark   = {180, 140, 100, 255};
    Color sleeve_col  = {60,  60,  60,  255};
    Color scope_col   = {35,  35,  35,  255};
    Color scope_lens  = {20,  80,  110, 220};

    rlDisableDepthTest();
    BeginMode3D(vm_cam);

    // Rotation matrix aligning DrawCube faces to camera axes
    float rot[16] = {
        cam_right.x, cam_right.y, cam_right.z, 0,
        cam_up.x,    cam_up.y,    cam_up.z,    0,
        cam_dir.x,   cam_dir.y,   cam_dir.z,   0,
        0,           0,           0,           1
    };

    auto drawBox = [&](Vector3 c, float w, float h, float l, Color col) {
        rlPushMatrix();
        rlTranslatef(c.x, c.y, c.z);
        rlMultMatrixf(rot);
        DrawCube({0,0,0}, w, h, l, col);
        rlPopMatrix();
    };
    auto drawBoxWires = [&](Vector3 c, float w, float h, float l, Color col) {
        rlPushMatrix();
        rlTranslatef(c.x, c.y, c.z);
        rlMultMatrixf(rot);
        DrawCubeWires({0,0,0}, w, h, l, col);
        rlPopMatrix();
    };

    // ── Barrel (very long, thin — AWP signature) ──
    Vector3 barrel_up = Vector3Add(grip, Vector3Scale(cam_up, 0.028f));
    Vector3 barrel_start = barrel_up;
    Vector3 barrel_end   = Vector3Add(barrel_up, Vector3Scale(cam_dir, 0.82f));
    // Thin outer barrel
    DrawCylinderEx(barrel_start, barrel_end, 0.013f, 0.011f, 8, weapon_col);
    // Thicker shroud near receiver (first third)
    Vector3 shroud_end = Vector3Add(barrel_start, Vector3Scale(cam_dir, 0.28f));
    DrawCylinderEx(barrel_start, shroud_end, 0.020f, 0.016f, 8, weapon_dark);

    // ── Receiver body ──
    Vector3 recv = Vector3Add(grip, Vector3Scale(cam_dir, 0.06f));
    recv = Vector3Add(recv, Vector3Scale(cam_up, 0.026f));
    drawBox(recv, 0.062f, 0.052f, 0.22f, weapon_col);
    drawBoxWires(recv, 0.063f, 0.053f, 0.221f, weapon_dark);

    // ── Ejection port cover (right side of receiver, decorative) ──
    {
        Vector3 port = Vector3Add(recv, Vector3Scale(cam_right, -0.031f));
        port = Vector3Add(port, Vector3Scale(cam_dir, 0.02f));
        drawBox(port, 0.004f, 0.030f, 0.06f, weapon_dark);
    }

    // ── Scope mounts (two rings connecting scope to receiver) ──
    {
        Vector3 mnt_fwd  = Vector3Add(recv, Vector3Scale(cam_dir,  0.06f));
        mnt_fwd  = Vector3Add(mnt_fwd,  Vector3Scale(cam_up, 0.034f));
        Vector3 mnt_back = Vector3Add(recv, Vector3Scale(cam_dir, -0.06f));
        mnt_back = Vector3Add(mnt_back, Vector3Scale(cam_up, 0.034f));
        drawBox(mnt_fwd,  0.064f, 0.020f, 0.022f, weapon_dark);
        drawBox(mnt_back, 0.064f, 0.020f, 0.022f, weapon_dark);
    }

    // ── Scope (prominent — AWP's defining feature) ──
    {
        Vector3 scope_center = Vector3Add(recv, Vector3Scale(cam_up, 0.065f));
        // Main tube
        Vector3 scope_f = Vector3Add(scope_center, Vector3Scale(cam_dir,  0.115f));
        Vector3 scope_b = Vector3Add(scope_center, Vector3Scale(cam_dir, -0.115f));
        DrawCylinderEx(scope_b, scope_f, 0.026f, 0.024f, 10, scope_col);
        // Objective bell (front)
        Vector3 obj_tip = Vector3Add(scope_f, Vector3Scale(cam_dir, 0.028f));
        DrawCylinderEx(scope_f, obj_tip, 0.024f, 0.040f, 10, scope_col);
        DrawSphere(obj_tip, 0.041f, scope_col);
        // Lens at objective
        DrawSphere(Vector3Add(obj_tip, Vector3Scale(cam_dir, 0.006f)), 0.032f, scope_lens);
        // Eyepiece bell (back)
        Vector3 eye_tip = Vector3Add(scope_b, Vector3Scale(cam_dir, -0.028f));
        DrawCylinderEx(scope_b, eye_tip, 0.026f, 0.038f, 10, scope_col);
        // Elevation turret (top knob on scope body)
        Vector3 turret = Vector3Add(scope_center, Vector3Scale(cam_up, 0.030f));
        DrawCylinderEx(turret, Vector3Add(turret, Vector3Scale(cam_up, 0.018f)), 0.012f, 0.010f, 6, weapon_dark);
    }

    // ── Bolt handle (slides back when firing, returns forward) ──
    {
        // Compute bolt travel (0 = home, 1 = fully open)
        float bolt_t = bolt_anim_timer_ / BOLT_ANIM_DURATION;  // 1→0
        float bolt_phase;
        if (bolt_t > 0.5f)
            bolt_phase = (1.0f - bolt_t) * 2.0f;   // opening: 0→1
        else
            bolt_phase = bolt_t * 2.0f;              // closing: 1→0
        float bolt_back = bolt_phase * 0.075f;

        Vector3 bolt_root = Vector3Add(recv, Vector3Scale(cam_dir, -0.02f - bolt_back));
        bolt_root = Vector3Add(bolt_root, Vector3Scale(cam_up, -0.000f));
        // Bolt knob sticks left (visible from player's viewpoint)
        Vector3 bolt_tip = Vector3Add(bolt_root, Vector3Scale(cam_right, -0.065f));
        DrawCylinderEx(bolt_root, bolt_tip, 0.010f, 0.009f, 6, weapon_light);
        DrawSphere(bolt_tip, 0.015f, weapon_light);
    }

    // ── Pistol grip ──
    {
        Vector3 grip_top    = Vector3Add(grip, Vector3Scale(cam_up,  -0.006f));
        Vector3 grip_bottom = Vector3Add(grip_top, Vector3Scale(cam_up,  -0.092f));
        grip_bottom = Vector3Add(grip_bottom, Vector3Scale(cam_dir, -0.018f));
        DrawCylinderEx(grip_top, grip_bottom, 0.024f, 0.020f, 8, weapon_dark);
    }

    // ── Trigger guard ──
    {
        Vector3 tg = Vector3Add(grip, Vector3Scale(cam_dir,  0.028f));
        tg = Vector3Add(tg, Vector3Scale(cam_up, -0.026f));
        drawBox(tg, 0.048f, 0.007f, 0.040f, weapon_dark);
    }

    // ── Stock (extends behind grip) ──
    {
        Vector3 stock_front = Vector3Add(grip, Vector3Scale(cam_dir, -0.07f));
        stock_front = Vector3Add(stock_front, Vector3Scale(cam_up, 0.010f));
        Vector3 stock_back  = Vector3Add(stock_front, Vector3Scale(cam_dir, -0.27f));
        stock_back  = Vector3Add(stock_back, Vector3Scale(cam_up, -0.012f));
        Vector3 stock_mid   = Vector3Lerp(stock_front, stock_back, 0.5f);
        drawBox(stock_mid, 0.058f, 0.046f, 0.24f, weapon_col);
        // Cheek rest (raised ridge on top of stock)
        Vector3 cheek = Vector3Lerp(stock_front, stock_back, 0.3f);
        cheek = Vector3Add(cheek, Vector3Scale(cam_up, 0.024f));
        drawBox(cheek, 0.050f, 0.016f, 0.10f, weapon_dark);
        // Butt plate
        Vector3 butt = Vector3Add(stock_back, Vector3Scale(cam_dir, -0.008f));
        drawBox(butt, 0.064f, 0.064f, 0.016f, weapon_dark);
    }

    // ── Muzzle device & flash ──
    {
        DrawSphere(barrel_end, 0.017f, weapon_dark);
        if (vm_recoil > 0.5f) {
            float fa = (vm_recoil - 0.5f) * 2.0f;
            unsigned char a = (unsigned char)(fa * 220);
            Color flash = {255, 210, 60, a};
            DrawSphere(barrel_end, 0.09f * fa, flash);
            Vector3 fl_u = Vector3Add(barrel_end, Vector3Scale(cam_up,    0.12f * fa));
            Vector3 fl_d = Vector3Add(barrel_end, Vector3Scale(cam_up,   -0.12f * fa));
            Vector3 fl_r = Vector3Add(barrel_end, Vector3Scale(cam_right,  0.12f * fa));
            Vector3 fl_l = Vector3Add(barrel_end, Vector3Scale(cam_right, -0.12f * fa));
            DrawLine3D(fl_u, fl_d, flash);
            DrawLine3D(fl_r, fl_l, flash);
        }
    }

    // ── Right hand (grip) ──
    {
        Vector3 rh = Vector3Add(grip, Vector3Scale(cam_up, -0.052f));
        drawBox(rh, 0.060f, 0.040f, 0.050f, skin_col);
        Vector3 rh_f = Vector3Add(rh, Vector3Scale(cam_dir,  0.030f));
        rh_f = Vector3Add(rh_f, Vector3Scale(cam_up, 0.010f));
        drawBox(rh_f, 0.056f, 0.020f, 0.026f, skin_col);
        Vector3 rh_t = Vector3Add(rh, Vector3Scale(cam_right, -0.031f));
        rh_t = Vector3Add(rh_t, Vector3Scale(cam_up, 0.012f));
        drawBox(rh_t, 0.016f, 0.016f, 0.032f, skin_dark);
    }

    // ── Right forearm ──
    {
        Vector3 wrist = Vector3Add(grip, Vector3Scale(cam_dir, -0.04f));
        wrist = Vector3Add(wrist, Vector3Scale(cam_up, -0.072f));
        Vector3 elbow = Vector3Add(wrist, Vector3Scale(cam_right,  0.16f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_up,  -0.11f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_dir, -0.13f));
        DrawCylinderEx(wrist, elbow, 0.034f, 0.040f, 8, sleeve_col);
        DrawCylinderEx(Vector3Add(wrist, Vector3Scale(cam_dir, 0.01f)), wrist, 0.031f, 0.034f, 8, skin_col);
    }

    // ── Left hand (fore-stock, further forward than a pistol) ──
    {
        Vector3 lh = Vector3Add(grip, Vector3Scale(cam_dir,  0.20f));
        lh = Vector3Add(lh, Vector3Scale(cam_up,    0.008f));
        lh = Vector3Add(lh, Vector3Scale(cam_right, -0.012f));
        drawBox(lh, 0.056f, 0.030f, 0.048f, skin_col);
        Vector3 lh_f = Vector3Add(lh, Vector3Scale(cam_up, 0.022f));
        drawBox(lh_f, 0.050f, 0.014f, 0.036f, skin_dark);
    }

    // ── Left forearm ──
    {
        Vector3 lw = Vector3Add(grip, Vector3Scale(cam_dir,  0.18f));
        lw = Vector3Add(lw, Vector3Scale(cam_up,    -0.032f));
        lw = Vector3Add(lw, Vector3Scale(cam_right, -0.012f));
        Vector3 le = Vector3Add(lw, Vector3Scale(cam_right, -0.18f));
        le = Vector3Add(le, Vector3Scale(cam_up,  -0.09f));
        le = Vector3Add(le, Vector3Scale(cam_dir, -0.10f));
        DrawCylinderEx(lw, le, 0.032f, 0.037f, 8, sleeve_col);
        DrawCylinderEx(Vector3Add(lw, Vector3Scale(cam_dir, 0.01f)), lw, 0.029f, 0.032f, 8, skin_col);
    }

    // ── Shell casings (ejected brass, fall with gravity in viewmodel space) ──
    for (const auto &s : shell_casings_) {
        float alpha = std::min(s.timer / 0.3f, 1.0f);
        Color brass = {210, 165, 50, (unsigned char)(alpha * 255)};
        Vector3 sp = grip;
        sp = Vector3Add(sp, Vector3Scale(cam_right, s.r));
        sp = Vector3Add(sp, Vector3Scale(cam_up,    s.u));
        sp = Vector3Add(sp, Vector3Scale(cam_dir,   s.f));
        // Tiny cylinder oriented along cam_right (rotating)
        float rad = s.rot * DEG2RAD;
        Vector3 axis = Vector3Add(Vector3Scale(cam_right, cosf(rad)), Vector3Scale(cam_up, sinf(rad)));
        Vector3 case_tip = Vector3Add(sp, Vector3Scale(axis, 0.026f));
        DrawCylinderEx(sp, case_tip, 0.007f, 0.005f, 6, brass);
    }

    EndMode3D();
    rlEnableDepthTest();
}

// ─── 2D HUD ─────────────────────────────────────────────────────

void GameUI3D::draw_crosshair() {
    int cx = GetScreenWidth() / 2;
    int cy = GetScreenHeight() / 2;
    int size = 12;
    int thick = 2;

    Color color = has_hover ? GREEN : WHITE;

    DrawRectangle(cx - size, cy - thick / 2, size * 2, thick, color);
    DrawRectangle(cx - thick / 2, cy - size, thick, size * 2, color);
}

void GameUI3D::draw_hud(const GameState &state) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, sh - 80, sw, 80, {40, 40, 40, 200});

    const char *turn_label = isBlackPlayer(state.current_player) ? "NOIR" : "BLANC";
    char turn_buf[64];
    snprintf(turn_buf, sizeof(turn_buf), "Tour %d : %s", state.current_turn, turn_label);
    DrawText(turn_buf, sw / 2 - MeasureText(turn_buf, 24) / 2, sh - 70, 24, WHITE);

    const char *mode = (current_state == UI3DState::PLAYING_SOLO)
                           ? "Solo (vs IA) - ESC: Menu"
                           : "Multi (Local 1v1) - ESC: Menu";
    DrawText(mode, 20, sh - 70, 18, LIGHTGRAY);

    if (current_state == UI3DState::PLAYING_SOLO) {
        char timer_buf[64];
        snprintf(timer_buf, sizeof(timer_buf), "AI Time: %.2f ms", state.last_ai_move_time_ms);
        DrawText(timer_buf, 20, sh - 40, 18, {0, 200, 255, 255});
    }

    char cap_buf[64];
    snprintf(cap_buf, sizeof(cap_buf), "Captures  N: %d/10  B: %d/10",
             state.board.blackCaptures, state.board.whiteCaptures);
    DrawText(cap_buf, sw / 2 - MeasureText(cap_buf, 18) / 2, sh - 35, 18, WHITE);

    if (has_hover) {
        char aim_buf[32];
        char col_char = 'A' + hovered_col;
        snprintf(aim_buf, sizeof(aim_buf), "Aim: %c-%d", col_char, hovered_row + 1);
        DrawText(aim_buf, sw - 160, 20, 20, GREEN);
    }
}

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
        snprintf(timer_buf, sizeof(timer_buf), "Dernier temps IA: %.2f ms", state.last_ai_move_time_ms);
        DrawText(timer_buf, sw / 2 - MeasureText(timer_buf, 22) / 2, sh / 2 + 10, 22, {0, 200, 255, 255});
    }

    const char *restart = "Appuyer [R] pour Rejouer";
    DrawText(restart, sw / 2 - MeasureText(restart, 24) / 2, sh / 2 + 50, 24, WHITE);
}

void GameUI3D::draw_capture_anims_3d() {
    for (const auto &a : capture_anims) {
        Vector3 world_pos = boardToWorld(a.row, a.col, 0.6f);
        Vector2 screen_pos = GetWorldToScreen(world_pos, camera);
        float arm = 16.0f;
        Color red = {220, 50, 50, (unsigned char)(a.timer * 255)};
        Vector2 tl = {screen_pos.x - arm, screen_pos.y - arm};
        Vector2 br = {screen_pos.x + arm, screen_pos.y + arm};
        Vector2 tr = {screen_pos.x + arm, screen_pos.y - arm};
        Vector2 bl = {screen_pos.x - arm, screen_pos.y + arm};
        DrawLineEx(tl, br, 3.0f, red);
        DrawLineEx(tr, bl, 3.0f, red);
    }
}

void GameUI3D::draw_history(const GameState &state) {
    int sh = GetScreenHeight();

    int panel_w = 200;
    int panel_h = sh - 80;
    DrawRectangle(0, 0, panel_w, panel_h, {50, 50, 50, 180});

    DrawText("Historique", 10, 10, 22, WHITE);
    DrawLine(10, 35, panel_w - 10, 35, GRAY);

    int max_lines = (panel_h - 50) / 25;
    int start_idx = 0;
    if ((int)state.move_history.size() > max_lines)
        start_idx = (int)state.move_history.size() - max_lines;

    int y = 45;
    for (int i = start_idx; i < (int)state.move_history.size(); i++) {
        const GameMove &mv = state.move_history[i];
        char col_char = 'A' + mv.x;

        char line[64];
        snprintf(line, sizeof(line), "%d. %c %c-%d",
                 mv.turn,
                 isBlackPlayer(mv.player) ? 'N' : 'B',
                 col_char, mv.y + 1);

        Color text_color = isBlackPlayer(mv.player) ? LIGHTGRAY : WHITE;
        DrawText(line, 10, y, 16, text_color);
        y += 25;
    }
}
