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
      ai_pending_(false) {
    InitWindow(1280, 720, "Gomoku FPS");
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

            // Update capture animation timers
            for (auto &a : capture_anims)
                a.timer -= dt;
            capture_anims.erase(
                std::remove_if(capture_anims.begin(), capture_anims.end(),
                    [](const CaptureAnim3D &a) { return a.timer <= 0.0f; }),
                capture_anims.end());

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
        return;
    }

    if (state.game_over) {
        if (IsKeyPressed(KEY_R)) {
            state.reset();
            ai_pending_ = false;
        }
        return;
    }

    if (current_state == UI3DState::PLAYING_SOLO &&
        !isBlackPlayer(state.current_player)) {
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && has_hover) {
        if (Rules::is_valid_move(state, hovered_col, hovered_row)) {
            if (state.place_stone(hovered_col, hovered_row)) {
                vm_recoil = 1.0f; // trigger recoil animation
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
    // Flush pending draws, then disable depth test so viewmodel renders on top
    rlDrawRenderBatchActive();

    // Compute camera basis vectors for viewmodel positioning
    Vector3 cam_dir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam_dir, camera.up));
    Vector3 cam_up_actual = Vector3Normalize(Vector3CrossProduct(cam_right, cam_dir));

    // Create a viewmodel camera very close to the origin
    // The viewmodel lives in its own mini-space
    Camera3D vm_cam = {};
    vm_cam.position = {0.0f, 0.0f, 0.0f};
    vm_cam.target = cam_dir;
    vm_cam.up = cam_up_actual;
    vm_cam.fovy = 65.0f;  // slightly wider FOV for viewmodel (standard FPS feel)
    vm_cam.projection = CAMERA_PERSPECTIVE;

    // Idle bobbing
    float bob_x = sinf(vm_bob_time * 1.8f) * 0.015f;
    float bob_y = sinf(vm_bob_time * 3.6f) * 0.008f;

    // Recoil: pushes weapon back (along -cam_dir) and rotates up slightly
    float recoil_back = vm_recoil * 0.12f;
    float recoil_up   = vm_recoil * 0.04f;

    // Weapon base offset: bottom-right of view
    // In viewmodel space: right = +cam_right, down = -cam_up, forward = +cam_dir
    float base_right  =  0.35f + bob_x;
    float base_down   = -0.28f + bob_y;
    float base_forward =  0.6f - recoil_back;

    Vector3 vm_origin = {0, 0, 0};
    vm_origin = Vector3Add(vm_origin, Vector3Scale(cam_right, base_right));
    vm_origin = Vector3Add(vm_origin, Vector3Scale(cam_up_actual, base_down));
    vm_origin = Vector3Add(vm_origin, Vector3Scale(cam_dir, base_forward));
    vm_origin = Vector3Add(vm_origin, Vector3Scale(cam_up_actual, recoil_up));

    Color weapon_col = getWeaponColor(state.current_player);
    Color weapon_dark = {
        (unsigned char)(weapon_col.r * 0.5f),
        (unsigned char)(weapon_col.g * 0.5f),
        (unsigned char)(weapon_col.b * 0.5f),
        255};
    Color weapon_light = {
        (unsigned char)((int)weapon_col.r + ((255 - (int)weapon_col.r) / 3)),
        (unsigned char)((int)weapon_col.g + ((255 - (int)weapon_col.g) / 3)),
        (unsigned char)((int)weapon_col.b + ((255 - (int)weapon_col.b) / 3)),
        255};
    Color skin_col = {210, 170, 130, 255};
    Color skin_dark = {180, 140, 100, 255};
    Color sleeve_col = {60, 60, 60, 255};

    // Disable depth test so viewmodel is always on top of the scene
    rlDisableDepthTest();

    BeginMode3D(vm_cam);

    // Rotation matrix: maps local X→cam_right, Y→cam_up, Z→cam_dir
    // so DrawCube faces align with the camera instead of world axes.
    float rot[16] = {
        cam_right.x,    cam_right.y,    cam_right.z,    0,
        cam_up_actual.x,cam_up_actual.y,cam_up_actual.z,0,
        cam_dir.x,      cam_dir.y,      cam_dir.z,      0,
        0,              0,              0,              1
    };

    // Draw an oriented cube aligned to camera axes
    auto drawOrientedCube = [&](Vector3 center, float w, float h, float l, Color c) {
        rlPushMatrix();
        rlTranslatef(center.x, center.y, center.z);
        rlMultMatrixf(rot);
        DrawCube({0, 0, 0}, w, h, l, c);
        rlPopMatrix();
    };

    auto drawOrientedCubeWires = [&](Vector3 center, float w, float h, float l, Color c) {
        rlPushMatrix();
        rlTranslatef(center.x, center.y, center.z);
        rlMultMatrixf(rot);
        DrawCubeWires({0, 0, 0}, w, h, l, c);
        rlPopMatrix();
    };

    // ── Barrel ──
    // Long cylinder-like shape (we use a stretched cube) pointing forward
    {
        Vector3 barrel_center = Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.18f));
        barrel_center = Vector3Add(barrel_center, Vector3Scale(cam_up_actual, 0.015f));
        // Barrel: elongated along cam_dir
        // Use DrawCube with rotation — simpler: draw a cylinder
        DrawCylinderEx(
            Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.0f)),
            Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.35f)),
            0.02f, 0.018f, 8, weapon_col);
        // Barrel outer casing
        DrawCylinderEx(
            Vector3Add(vm_origin, Vector3Scale(cam_dir, -0.02f)),
            Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.15f)),
            0.032f, 0.028f, 8, weapon_dark);
    }

    // ── Receiver / body ──
    {
        Vector3 body_center = Vector3Add(vm_origin, Vector3Scale(cam_dir, -0.02f));
        body_center = Vector3Add(body_center, Vector3Scale(cam_up_actual, -0.01f));

        // Main body block
        drawOrientedCube(body_center, 0.07f, 0.055f, 0.14f, weapon_col);
        drawOrientedCubeWires(body_center, 0.071f, 0.056f, 0.141f, weapon_dark);

        // Top rail / slide
        Vector3 slide = Vector3Add(body_center, Vector3Scale(cam_up_actual, 0.032f));
        drawOrientedCube(slide, 0.05f, 0.015f, 0.16f, weapon_light);
    }

    // ── Grip ──
    {
        Vector3 grip_top = Vector3Add(vm_origin, Vector3Scale(cam_dir, -0.03f));
        grip_top = Vector3Add(grip_top, Vector3Scale(cam_up_actual, -0.04f));
        Vector3 grip_bottom = Vector3Add(grip_top, Vector3Scale(cam_up_actual, -0.10f));
        // Slight angle: grip tilts back
        grip_bottom = Vector3Add(grip_bottom, Vector3Scale(cam_dir, -0.025f));

        DrawCylinderEx(grip_top, grip_bottom, 0.025f, 0.022f, 8, weapon_dark);
    }

    // ── Trigger guard ──
    {
        Vector3 tg = Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.01f));
        tg = Vector3Add(tg, Vector3Scale(cam_up_actual, -0.055f));
        drawOrientedCube(tg, 0.05f, 0.008f, 0.04f, weapon_dark);
    }

    // ── Muzzle tip ──
    {
        Vector3 muzzle = Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.35f));
        DrawSphere(muzzle, 0.022f, weapon_dark);

        // Muzzle flash on recoil
        if (vm_recoil > 0.5f) {
            float flash_alpha = (vm_recoil - 0.5f) * 2.0f; // 0..1
            unsigned char fa = (unsigned char)(flash_alpha * 200);
            Color flash = {255, 200, 50, fa};
            DrawSphere(muzzle, 0.06f * flash_alpha, flash);
        }
    }

    // ── Right hand (gripping the weapon) ──
    {
        Vector3 hand_pos = Vector3Add(vm_origin, Vector3Scale(cam_dir, -0.03f));
        hand_pos = Vector3Add(hand_pos, Vector3Scale(cam_up_actual, -0.06f));
        hand_pos = Vector3Add(hand_pos, Vector3Scale(cam_right, 0.0f));

        // Palm wrapping around grip
        drawOrientedCube(hand_pos, 0.065f, 0.045f, 0.055f, skin_col);

        // Fingers wrapping forward
        Vector3 fingers = Vector3Add(hand_pos, Vector3Scale(cam_dir, 0.035f));
        fingers = Vector3Add(fingers, Vector3Scale(cam_up_actual, 0.01f));
        drawOrientedCube(fingers, 0.06f, 0.025f, 0.03f, skin_col);

        // Thumb on the side
        Vector3 thumb = Vector3Add(hand_pos, Vector3Scale(cam_right, -0.035f));
        thumb = Vector3Add(thumb, Vector3Scale(cam_up_actual, 0.015f));
        drawOrientedCube(thumb, 0.02f, 0.02f, 0.04f, skin_dark);
    }

    // ── Right forearm ──
    {
        Vector3 wrist = Vector3Add(vm_origin, Vector3Scale(cam_dir, -0.06f));
        wrist = Vector3Add(wrist, Vector3Scale(cam_up_actual, -0.08f));

        Vector3 elbow = Vector3Add(wrist, Vector3Scale(cam_right, 0.18f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_up_actual, -0.12f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_dir, -0.15f));

        // Sleeve (forearm)
        DrawCylinderEx(wrist, elbow, 0.035f, 0.04f, 8, sleeve_col);

        // Exposed skin at wrist
        Vector3 wrist_skin = Vector3Add(wrist, Vector3Scale(cam_dir, 0.01f));
        DrawCylinderEx(wrist_skin, wrist, 0.032f, 0.035f, 8, skin_col);
    }

    // ── Left hand (supporting the barrel, optional) ──
    {
        Vector3 lh_pos = Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.10f));
        lh_pos = Vector3Add(lh_pos, Vector3Scale(cam_up_actual, -0.03f));
        lh_pos = Vector3Add(lh_pos, Vector3Scale(cam_right, -0.02f));

        // Palm under barrel
        drawOrientedCube(lh_pos, 0.06f, 0.035f, 0.05f, skin_col);

        // Fingers curled over
        Vector3 lfingers = Vector3Add(lh_pos, Vector3Scale(cam_up_actual, 0.025f));
        drawOrientedCube(lfingers, 0.055f, 0.018f, 0.04f, skin_dark);
    }

    // ── Left forearm ──
    {
        Vector3 lh_wrist = Vector3Add(vm_origin, Vector3Scale(cam_dir, 0.08f));
        lh_wrist = Vector3Add(lh_wrist, Vector3Scale(cam_up_actual, -0.04f));
        lh_wrist = Vector3Add(lh_wrist, Vector3Scale(cam_right, -0.02f));

        Vector3 lh_elbow = Vector3Add(lh_wrist, Vector3Scale(cam_right, -0.20f));
        lh_elbow = Vector3Add(lh_elbow, Vector3Scale(cam_up_actual, -0.10f));
        lh_elbow = Vector3Add(lh_elbow, Vector3Scale(cam_dir, -0.12f));

        DrawCylinderEx(lh_wrist, lh_elbow, 0.033f, 0.038f, 8, sleeve_col);

        Vector3 lw_skin = Vector3Add(lh_wrist, Vector3Scale(cam_dir, 0.01f));
        DrawCylinderEx(lw_skin, lh_wrist, 0.030f, 0.033f, 8, skin_col);
    }

    EndMode3D();

    // Re-enable depth test for next frame
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
