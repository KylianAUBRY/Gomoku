// ─────────────────────────────────────────────────────────────────────────────
// Viewmodel3D.cpp — Rendu de l'arme FPS en première personne (Raylib)
// ─────────────────────────────────────────────────────────────────────────────

#include "Viewmodel3D.hpp"
#include "raymath.h" // Vector3Add, Vector3Scale, Vector3Normalize, Vector3CrossProduct
#include "rlgl.h"    // rlDisableDepthTest, rlEnableDepthTest, rlDrawRenderBatchActive

#include <algorithm> // std::min
#include <cmath>     // sinf, cosf

// ─────────────────────────────────────────────────────────────────────────────
// draw_viewmodel_3d — arme + mains + avant-bras en première personne
//
// Technique : rendu dans un espace caméra virtuel séparé (vm_cam).
//
// Pourquoi une caméra séparée ?
//   L'arme dessinée avec la caméra scène s'enfoncerait dans le goban.
//   vm_cam a son origine à (0,0,0) : l'arme est toujours à la même position
//   relative à l'œil du joueur.
//   FOV 65° (vs 55° scène) : champ plus large pour les objets proches.
//
// rlDrawRenderBatchActive() : force le flush du batch Raylib AVANT de changer
//   de matrice de projection (évite de mélanger les géométries des deux passes).
//
// drawBox (lambda) : boîte 3D avec 6 faces (12 triangles) en coordonnées caméra
//   (cam_right, cam_up, cam_dir). Évite DrawCube qui applique un shading non désiré.
//
// Animations :
//   bob_x/bob_y : balancement idle sinusoïdal
//   recoil_back/recoil_up : recul au tir, décroît à 6× dt
//   pump_back : animation bolt-action (aller-retour 0→0.5→0)
// ─────────────────────────────────────────────────────────────────────────────
void draw_viewmodel_3d(const Camera3D&                  camera,
                       float                            bob_time,
                       float                            recoil,
                       float                            bolt_timer,
                       const std::vector<ShellCasing>&  shell_casings) {
    rlDrawRenderBatchActive(); // Flush avant changement de matrice

    // Vecteurs de base de la caméra (extraits de la direction de regard)
    Vector3 cam_dir   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam_dir, camera.up));
    Vector3 cam_up    = Vector3Normalize(Vector3CrossProduct(cam_right, cam_dir));

    // Caméra viewmodel : origine à (0,0,0), même orientation que la caméra principale
    Camera3D vm_cam   = {};
    vm_cam.position   = {0.0f, 0.0f, 0.0f};
    vm_cam.target     = cam_dir;
    vm_cam.up         = cam_up;
    vm_cam.fovy       = 65.0f; // FOV plus large pour les objets proches
    vm_cam.projection = CAMERA_PERSPECTIVE;

    // Bob idle : double sinusoïde (X à 1.8 rad/s, Y à 3.6 rad/s = 2× plus rapide)
    float bob_x = sinf(bob_time * 1.8f) * 0.015f;
    float bob_y = sinf(bob_time * 3.6f) * 0.008f;

    // Recul : fort kick arrière + léger kick vers le haut
    float recoil_back = recoil * 0.18f;
    float recoil_up   = recoil * 0.07f;

    // Position de base du grip (bas-droite de l'écran, en coordonnées caméra)
    float base_right   = 0.30f + bob_x;
    float base_down    = -0.22f + bob_y;
    float base_forward = 0.55f - recoil_back;

    Vector3 grip = Vector3Scale(cam_right, base_right);
    grip = Vector3Add(grip, Vector3Scale(cam_up, base_down));
    grip = Vector3Add(grip, Vector3Scale(cam_dir, base_forward));
    grip = Vector3Add(grip, Vector3Scale(cam_up, recoil_up));

    // Palette de couleurs Tron
    Color skin_col   = {210, 170, 130, 255};
    Color skin_dark  = {180, 140, 100, 255};
    Color sleeve_col = { 60,  60,  60, 255};
    Color body_col   = { 12,  14,  20, 255}; // Noir absolu
    Color accent_col = {  0, 200, 255, 255}; // Cyan primaire
    Color accent_dim = {  0,  55,  90, 255}; // Cyan sombre

    rlDisableDepthTest();
    BeginMode3D(vm_cam);

    // drawBox : boîte centrée sur `c`, dimensions w×h×l, en coordonnées caméra.
    // Construit 8 sommets via cam_right/cam_up/cam_dir, puis les 12 triangles (6 faces).
    auto drawBox = [&](Vector3 c, float w, float h, float l, Color col) {
        float hw = w * 0.5f, hh = h * 0.5f, hl = l * 0.5f;
        auto p = [&](float rx, float ry, float rz) -> Vector3 {
            return Vector3Add(c, Vector3Add(Vector3Scale(cam_right, rx),
                                           Vector3Add(Vector3Scale(cam_up, ry),
                                                      Vector3Scale(cam_dir, rz))));
        };
        Vector3 v[8] = {p(-hw,-hh,-hl), p(hw,-hh,-hl), p(-hw,hh,-hl), p(hw,hh,-hl),
                        p(-hw,-hh, hl), p(hw,-hh, hl), p(-hw,hh, hl), p(hw,hh, hl)};
        DrawTriangle3D(v[4], v[5], v[7], col); DrawTriangle3D(v[4], v[7], v[6], col); // Avant
        DrawTriangle3D(v[1], v[0], v[2], col); DrawTriangle3D(v[1], v[2], v[3], col); // Arrière
        DrawTriangle3D(v[2], v[3], v[7], col); DrawTriangle3D(v[2], v[7], v[6], col); // Haut
        DrawTriangle3D(v[0], v[1], v[5], col); DrawTriangle3D(v[0], v[5], v[4], col); // Bas
        DrawTriangle3D(v[1], v[3], v[7], col); DrawTriangle3D(v[1], v[7], v[5], col); // Droite
        DrawTriangle3D(v[4], v[6], v[2], col); DrawTriangle3D(v[4], v[2], v[0], col); // Gauche
    };

    // Animation bolt-action
    // pump_t : 0.0 (fin) → 1.0 (début). pump_phase : aller-retour 0→1→0.
    float pump_t     = bolt_timer / BOLT_ANIM_DURATION;
    float pump_phase = (pump_t > 0.5f) ? (1.0f - pump_t) * 2.0f : pump_t * 2.0f;
    float pump_back  = pump_phase * 0.10f;

    // ── Canon ─────────────────────────────────────────────────────────────────
    const float BRL_L = 0.50f, BRL_S = 0.044f;
    Vector3 brl_org  = Vector3Add(grip, Vector3Scale(cam_up, 0.038f));
    brl_org = Vector3Add(brl_org, Vector3Scale(cam_dir, 0.08f));
    Vector3 brl_ctr  = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L * 0.5f));
    Vector3 barrel_tip = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L + 0.006f));
    drawBox(brl_ctr, BRL_S, BRL_S, BRL_L, body_col);
    // Stripes lumineuses cyan sur les arêtes du canon
    drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_up,    BRL_S * 0.5f + 0.002f)), BRL_S - 0.008f, 0.003f, BRL_L, accent_col);
    drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_right,  BRL_S * 0.5f + 0.001f)), 0.003f, BRL_S - 0.008f, BRL_L, accent_col);
    drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_right, -BRL_S * 0.5f - 0.001f)), 0.003f, BRL_S - 0.008f, BRL_L, accent_col);
    // Bouche du canon (rebord cyan + âme creuse)
    drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_dir, BRL_L * 0.5f + 0.002f)), BRL_S + 0.004f, BRL_S + 0.004f, 0.004f, accent_col);
    drawBox(Vector3Add(brl_ctr, Vector3Scale(cam_dir, BRL_L * 0.5f + 0.005f)), BRL_S - 0.012f, BRL_S - 0.012f, 0.007f, body_col);
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
    drawBox(Vector3Add(recv, Vector3Scale(cam_up, RCV_H * 0.5f + 0.002f)), 0.014f, 0.003f, RCV_L, accent_col);
    for (int s = -1; s <= 1; s += 2) {
        Vector3 sp = Vector3Add(recv, Vector3Scale(cam_right, s * (RCV_W * 0.5f + 0.001f)));
        drawBox(Vector3Add(sp, Vector3Scale(cam_up,  0.020f)), 0.003f, 0.007f, RCV_L * 0.88f, accent_col);
        drawBox(Vector3Add(sp, Vector3Scale(cam_up, -0.020f)), 0.003f, 0.007f, RCV_L * 0.88f, accent_dim);
    }
    {
        Vector3 cell = Vector3Add(recv, Vector3Scale(cam_right, RCV_W * 0.5f + 0.001f));
        cell = Vector3Add(cell, Vector3Scale(cam_dir, 0.008f));
        drawBox(cell, 0.004f, 0.038f, 0.060f, accent_col);
        drawBox(Vector3Add(cell, Vector3Scale(cam_right, 0.003f)), 0.003f, 0.030f, 0.052f, accent_dim);
    }
    drawBox(Vector3Add(recv, Vector3Scale(cam_dir,  RCV_L * 0.5f + 0.001f)), RCV_W + 0.004f, RCV_H + 0.004f, 0.003f, accent_dim);
    drawBox(Vector3Add(recv, Vector3Scale(cam_dir, -RCV_L * 0.5f - 0.001f)), RCV_W + 0.004f, RCV_H + 0.004f, 0.003f, accent_dim);

    // ── Garde (sliding, suit pump_back) ──────────────────────────────────────
    {
        const float PM_W = 0.070f, PM_H = 0.032f, PM_L = 0.082f;
        Vector3 pump_ctr = Vector3Add(grip, Vector3Scale(cam_dir, 0.24f - pump_back));
        pump_ctr = Vector3Add(pump_ctr, Vector3Scale(cam_up, -0.004f));
        drawBox(pump_ctr, PM_W, PM_H, PM_L, body_col);
        drawBox(Vector3Add(pump_ctr, Vector3Scale(cam_up, PM_H * 0.5f + 0.002f)), PM_W - 0.010f, 0.003f, PM_L, accent_col);
        drawBox(Vector3Add(pump_ctr, Vector3Scale(cam_dir,  PM_L * 0.5f + 0.001f)), PM_W + 0.004f, PM_H + 0.004f, 0.003f, accent_col);
        drawBox(Vector3Add(pump_ctr, Vector3Scale(cam_dir, -PM_L * 0.5f - 0.001f)), PM_W + 0.004f, PM_H + 0.004f, 0.003f, accent_col);
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
        drawBox(Vector3Add(gc, Vector3Scale(cam_dir,  0.019f)), 0.044f, 0.092f, 0.003f, accent_dim);
        drawBox(Vector3Add(gc, Vector3Scale(cam_dir, -0.019f)), 0.044f, 0.092f, 0.003f, accent_dim);
    }

    // ── Pontet de détente ─────────────────────────────────────────────────────
    {
        Vector3 tg = Vector3Add(grip, Vector3Scale(cam_dir, 0.034f));
        tg = Vector3Add(tg, Vector3Scale(cam_up, -0.028f));
        drawBox(tg, 0.054f, 0.007f, 0.044f, body_col);
        drawBox(Vector3Add(tg, Vector3Scale(cam_up, -0.012f)), 0.013f, 0.020f, 0.006f, body_col);
    }

    // ── Crosse ────────────────────────────────────────────────────────────────
    {
        Vector3 bridge = Vector3Add(recv, Vector3Scale(cam_dir, -(RCV_L * 0.5f + 0.012f)));
        drawBox(bridge, RCV_W, RCV_H - 0.010f, 0.024f, body_col);
        Vector3 stk = Vector3Add(bridge, Vector3Scale(cam_dir, -0.136f));
        stk = Vector3Add(stk, Vector3Scale(cam_up, -0.008f));
        drawBox(stk, 0.056f, 0.060f, 0.248f, body_col);
        drawBox(Vector3Add(stk, Vector3Scale(cam_up, 0.032f)), 0.010f, 0.003f, 0.240f, accent_col);
        for (int s = -1; s <= 1; s += 2) {
            Vector3 sp = Vector3Add(stk, Vector3Scale(cam_right, s * 0.029f));
            drawBox(Vector3Add(sp, Vector3Scale(cam_up, -0.010f)), 0.003f, 0.007f, 0.240f, accent_dim);
        }
        drawBox(Vector3Add(stk, Vector3Scale(cam_dir, -0.126f)), 0.058f, 0.062f, 0.003f, accent_dim);
    }

    // ── Guidon ────────────────────────────────────────────────────────────────
    {
        Vector3 sb = Vector3Add(recv, Vector3Scale(cam_up, RCV_H * 0.5f + 0.006f));
        sb = Vector3Add(sb, Vector3Scale(cam_dir, RCV_L * 0.5f - 0.028f));
        drawBox(sb, 0.020f, 0.004f, 0.012f, body_col);
        Vector3 sp = Vector3Add(sb, Vector3Scale(cam_up, 0.010f));
        drawBox(sp, 0.004f, 0.014f, 0.004f, body_col);
        DrawSphere(Vector3Add(sp, Vector3Scale(cam_up, 0.009f)), 0.005f, accent_col);
    }

    // ── Flash de bouche (burst cyan au moment du tir) ─────────────────────────
    // Visible uniquement quand recoil > 0.5 (première moitié du recul).
    if (recoil > 0.5f) {
        float fa = (recoil - 0.5f) * 2.0f;
        unsigned char a = (unsigned char)(fa * 255);
        Color energy      = {  0, 200, 255, a};
        Color energy_core = {200, 240, 255, a};
        drawBox(Vector3Add(barrel_tip, Vector3Scale(cam_dir, 0.04f * fa)),
                0.08f * fa, 0.08f * fa, 0.08f * fa, energy);
        drawBox(Vector3Add(barrel_tip, Vector3Scale(cam_dir, 0.02f * fa)),
                0.04f * fa, 0.04f * fa, 0.04f * fa, energy_core);
        DrawLine3D(barrel_tip, Vector3Add(barrel_tip, Vector3Scale(cam_up,    0.24f * fa)), energy);
        DrawLine3D(barrel_tip, Vector3Add(barrel_tip, Vector3Scale(cam_up,   -0.24f * fa)), energy);
        DrawLine3D(barrel_tip, Vector3Add(barrel_tip, Vector3Scale(cam_right,  0.24f * fa)), energy);
        DrawLine3D(barrel_tip, Vector3Add(barrel_tip, Vector3Scale(cam_right, -0.24f * fa)), energy);
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
    // DrawCylinderEx : cylindre entre deux points 3D. Rayons différents → aspect conique.
    {
        Vector3 wrist = Vector3Add(grip, Vector3Scale(cam_dir, -0.04f));
        wrist = Vector3Add(wrist, Vector3Scale(cam_up, -0.072f));
        Vector3 elbow = Vector3Add(wrist, Vector3Scale(cam_right,  0.16f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_up,    -0.11f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_dir,   -0.13f));
        DrawCylinderEx(wrist, elbow, 0.034f, 0.040f, 8, sleeve_col);
        DrawCylinderEx(Vector3Add(wrist, Vector3Scale(cam_dir, 0.01f)), wrist, 0.031f, 0.034f, 8, skin_col);
    }

    // ── Main gauche (suit le déplacement de la garde) ─────────────────────────
    {
        Vector3 lh = Vector3Add(grip, Vector3Scale(cam_dir, 0.24f - pump_back));
        lh = Vector3Add(lh, Vector3Scale(cam_up,    -0.006f));
        lh = Vector3Add(lh, Vector3Scale(cam_right, -0.012f));
        drawBox(lh, 0.056f, 0.032f, 0.050f, skin_col);
        drawBox(Vector3Add(lh, Vector3Scale(cam_up, 0.020f)), 0.050f, 0.014f, 0.038f, skin_dark);
    }

    // ── Avant-bras gauche (suit la garde) ────────────────────────────────────
    {
        Vector3 lw = Vector3Add(grip, Vector3Scale(cam_dir, 0.22f - pump_back));
        lw = Vector3Add(lw, Vector3Scale(cam_up,    -0.030f));
        lw = Vector3Add(lw, Vector3Scale(cam_right, -0.012f));
        Vector3 le = Vector3Add(lw, Vector3Scale(cam_right, -0.18f));
        le = Vector3Add(le, Vector3Scale(cam_up,  -0.09f));
        le = Vector3Add(le, Vector3Scale(cam_dir, -0.10f));
        DrawCylinderEx(lw, le, 0.032f, 0.037f, 8, sleeve_col);
        DrawCylinderEx(Vector3Add(lw, Vector3Scale(cam_dir, 0.01f)), lw, 0.029f, 0.032f, 8, skin_col);
    }

    // ── Douilles éjectées ─────────────────────────────────────────────────────
    // Rendues dans l'espace viewmodel (coordonnées relatives au grip).
    // DrawCylinderEx orienté selon l'axe de rotation (plan cam_right/cam_up).
    for (const auto& s : shell_casings) {
        float alpha = std::min(s.timer / 0.3f, 1.0f); // Fade sur les 0.3s finales
        Color brass = {210, 165, 50, (unsigned char)(alpha * 255)};
        Vector3 sp  = grip;
        sp = Vector3Add(sp, Vector3Scale(cam_right, s.r));
        sp = Vector3Add(sp, Vector3Scale(cam_up,    s.u));
        sp = Vector3Add(sp, Vector3Scale(cam_dir,   s.f));
        float   rad      = s.rot * DEG2RAD;
        Vector3 axis     = Vector3Add(Vector3Scale(cam_right, cosf(rad)),
                                      Vector3Scale(cam_up,    sinf(rad)));
        Vector3 case_tip = Vector3Add(sp, Vector3Scale(axis, 0.026f));
        DrawCylinderEx(sp, case_tip, 0.007f, 0.005f, 6, brass);
    }

    EndMode3D();
    rlEnableDepthTest(); // Réactiver pour la prochaine frame
}
