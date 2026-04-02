// ─────────────────────────────────────────────────────────────────────────────
// Viewmodel3D.cpp — Pierre-shooter en première personne
//
// Rendu : DrawTriangle3D ne bénéficie d'aucun éclairage Raylib.
// Solution : drawBoxShaded — chaque face reçoit une teinte distincte simulant
//   un éclairage venant du haut-droite. Donne une vraie perception de volume.
//
// Design :
//   - Canon avec rail picatinny sous-canon (6 encoches)
//   - Frein de bouche avec 3 ports de dégazage
//   - Guidon avant avec point rouge
//   - Hausse arrière U-notch
//   - Rail picatinny sur le dessus du boîtier (8 encoches)
//   - Culasse coulissante animée (bolt-action)
//   - Bande pierre joueur sur le flanc droit
//   - Chargeur avec baseplate + fenêtre de comptage
//   - Main avec knuckles + pouce
//   - Avant-bras avec manchon tissu
//   - Flash de tir 8 rayons (starburst) + halo orange
// ─────────────────────────────────────────────────────────────────────────────

#include "Viewmodel3D.hpp"
#include "raymath.h"
#include "rlgl.h"

// rlgl.h inclut les headers OpenGL via raylib.h (include guards actifs).
// GL_SILENCE_DEPRECATION : Apple marque OpenGL deprecated depuis 10.14 ;
// sans ce define, glClear génère -Wdeprecated-declarations → erreur avec -Werror.
// Raylib lui-même applique ce silence dans son propre code.
#if defined(__APPLE__)
    #ifndef GL_SILENCE_DEPRECATION
        #define GL_SILENCE_DEPRECATION
    #endif
    #include <OpenGL/gl3.h>
#else
    #include <GL/gl.h>
#endif

#include <cmath>

void draw_viewmodel_3d(const Camera3D &camera, float bob_time, float recoil,
                       float bolt_timer, bool is_black) {
    rlDrawRenderBatchActive();

    // ── Vecteurs de base ──────────────────────────────────────────────────────
    Vector3 cam_dir   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam_dir, camera.up));
    Vector3 cam_up    = Vector3Normalize(Vector3CrossProduct(cam_right, cam_dir));

    Camera3D vm_cam   = {};
    vm_cam.position   = {0.0f, 0.0f, 0.0f};
    vm_cam.target     = cam_dir;
    vm_cam.up         = cam_up;
    vm_cam.fovy       = 65.0f;
    vm_cam.projection = CAMERA_PERSPECTIVE;

    // ── Animations ────────────────────────────────────────────────────────────
    float bob_x = sinf(bob_time * 1.8f) * 0.012f;
    float bob_y = sinf(bob_time * 3.6f) * 0.006f;

    float recoil_back = recoil * 0.18f;
    float recoil_up   = recoil * 0.05f;

    // Bolt-action : la culasse recule sur la première moitié, revient sur la seconde
    float pump_t     = bolt_timer / BOLT_ANIM_DURATION;
    float pump_phase = (pump_t > 0.5f) ? (1.0f - pump_t) * 2.0f : pump_t * 2.0f;
    float bolt_slide = pump_phase * 0.050f; // amplitude glissement culasse

    // Ancrage bas-droite écran
    float base_right   = 0.32f + bob_x;
    float base_down    = -0.24f + bob_y;
    float base_forward = 0.54f - recoil_back;

    Vector3 grip = Vector3Scale(cam_right, base_right);
    grip = Vector3Add(grip, Vector3Scale(cam_up,    base_down));
    grip = Vector3Add(grip, Vector3Scale(cam_dir,   base_forward));
    grip = Vector3Add(grip, Vector3Scale(cam_up,    recoil_up));

    // ── Palette ───────────────────────────────────────────────────────────────
    Color metal_base = {50, 48, 46, 255};   // Acier gunmetal
    Color metal_dark = {28, 26, 24, 255};   // Creux / âme
    Color rail_col   = {68, 66, 62, 255};   // Picatinny
    Color stone_col  = is_black ? Color{18, 18, 22, 255} : Color{235, 228, 212, 255};
    Color stone_hi   = is_black ? Color{65, 65, 82, 255}  : Color{255, 250, 235, 255};
    Color skin_col   = {210, 170, 128, 255};
    Color skin_dark  = {176, 136, 96,  255};
    Color sleeve_col = {40,  40,  40,  255};

    // ── Helpers ───────────────────────────────────────────────────────────────
    auto shade = [](Color c, int d) -> Color {
        int r = c.r + d, g = c.g + d, b = c.b + d;
        return {(unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r),
                (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g),
                (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b),
                c.a};
    };

    // drawBoxShaded : éclairage simulé par face (lumière haut-droite)
    auto drawBoxShaded = [&](Vector3 c, float w, float h, float l, Color col) {
        float hw = w * 0.5f, hh = h * 0.5f, hl = l * 0.5f;
        auto p = [&](float rx, float ry, float rz) -> Vector3 {
            return Vector3Add(c, Vector3Add(Vector3Scale(cam_right, rx),
                                            Vector3Add(Vector3Scale(cam_up,  ry),
                                                       Vector3Scale(cam_dir, rz))));
        };
        Vector3 v[8] = {p(-hw,-hh,-hl), p(hw,-hh,-hl), p(-hw,hh,-hl), p(hw,hh,-hl),
                        p(-hw,-hh, hl), p(hw,-hh, hl), p(-hw,hh, hl), p(hw,hh, hl)};
        Color top   = shade(col, +42); Color bot   = shade(col, -35);
        Color front = shade(col, +10); Color back  = shade(col, -25);
        Color right = shade(col, +20); Color left  = shade(col, -15);
        DrawTriangle3D(v[4],v[5],v[7],front); DrawTriangle3D(v[4],v[7],v[6],front);
        DrawTriangle3D(v[1],v[0],v[2],back);  DrawTriangle3D(v[1],v[2],v[3],back);
        DrawTriangle3D(v[2],v[3],v[7],top);   DrawTriangle3D(v[2],v[7],v[6],top);
        DrawTriangle3D(v[0],v[1],v[5],bot);   DrawTriangle3D(v[0],v[5],v[4],bot);
        DrawTriangle3D(v[1],v[3],v[7],right); DrawTriangle3D(v[1],v[7],v[5],right);
        DrawTriangle3D(v[4],v[6],v[2],left);  DrawTriangle3D(v[4],v[2],v[0],left);
    };

    // drawBoxFlat : couleur uniforme, pour accents fins
    auto drawBoxFlat = [&](Vector3 c, float w, float h, float l, Color col) {
        float hw = w * 0.5f, hh = h * 0.5f, hl = l * 0.5f;
        auto p = [&](float rx, float ry, float rz) -> Vector3 {
            return Vector3Add(c, Vector3Add(Vector3Scale(cam_right, rx),
                                            Vector3Add(Vector3Scale(cam_up,  ry),
                                                       Vector3Scale(cam_dir, rz))));
        };
        Vector3 v[8] = {p(-hw,-hh,-hl), p(hw,-hh,-hl), p(-hw,hh,-hl), p(hw,hh,-hl),
                        p(-hw,-hh, hl), p(hw,-hh, hl), p(-hw,hh, hl), p(hw,hh, hl)};
        DrawTriangle3D(v[4],v[5],v[7],col); DrawTriangle3D(v[4],v[7],v[6],col);
        DrawTriangle3D(v[1],v[0],v[2],col); DrawTriangle3D(v[1],v[2],v[3],col);
        DrawTriangle3D(v[2],v[3],v[7],col); DrawTriangle3D(v[2],v[7],v[6],col);
        DrawTriangle3D(v[0],v[1],v[5],col); DrawTriangle3D(v[0],v[5],v[4],col);
        DrawTriangle3D(v[1],v[3],v[7],col); DrawTriangle3D(v[1],v[7],v[5],col);
        DrawTriangle3D(v[4],v[6],v[2],col); DrawTriangle3D(v[4],v[2],v[0],col);
    };

    // Vider le depth buffer : les valeurs de la scène (caméra principale à {9,9,22})
    // sont incompatibles avec vm_cam (caméra virtuelle à l'origine). Sans ce clear,
    // certains fragments du viewmodel échouent le depth test → faces "transparentes".
    // Backface culling : sans lui, les faces opposées à la caméra sont quand même
    // rendues et "traversent" les faces visibles → effet voir-à-travers.
    rlDrawRenderBatchActive();
    glClear(GL_DEPTH_BUFFER_BIT);
    BeginMode3D(vm_cam);          // active rlEnableDepthTest()
    rlEnableBackfaceCulling();    // élimine les faces dos-à-caméra → rendu opaque

    // ── Canon ─────────────────────────────────────────────────────────────────
    const float BRL_L = 0.26f, BRL_W = 0.052f, BRL_H = 0.052f;
    float pump_barrel = pump_phase * 0.040f;
    Vector3 brl_org = Vector3Add(grip, Vector3Scale(cam_up, 0.038f));
    brl_org = Vector3Add(brl_org, Vector3Scale(cam_dir, 0.065f - pump_barrel));
    Vector3 brl_ctr = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L * 0.5f));
    Vector3 brl_tip = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L));

    drawBoxShaded(brl_ctr, BRL_W, BRL_H, BRL_L, metal_base);

    // Rail picatinny sous le canon (6 encoches)
    {
        Vector3 rail = Vector3Add(brl_ctr, Vector3Scale(cam_up, -(BRL_H * 0.5f + 0.013f)));
        rail = Vector3Add(rail, Vector3Scale(cam_dir, 0.010f));
        float rail_len = BRL_L * 0.76f;
        drawBoxShaded(rail, 0.032f, 0.016f, rail_len, shade(metal_base, -5));
        for (int i = 0; i < 6; i++) {
            float off = -rail_len * 0.5f + (i + 0.5f) * (rail_len / 6.0f);
            Vector3 notch = Vector3Add(rail, Vector3Scale(cam_dir, off));
            notch = Vector3Add(notch, Vector3Scale(cam_up, -(0.016f * 0.5f + 0.001f)));
            drawBoxFlat(notch, 0.034f, 0.003f, 0.005f, shade(metal_base, -28));
        }
    }

    // ── Frein de bouche (muzzle brake) ───────────────────────────────────────
    {
        const float MB_W = BRL_W + 0.020f, MB_H = BRL_H + 0.016f, MB_L = 0.038f;
        Vector3 mb_ctr = Vector3Add(brl_tip, Vector3Scale(cam_dir, MB_L * 0.5f));
        drawBoxShaded(mb_ctr, MB_W, MB_H, MB_L, shade(metal_base, +8));
        // 3 ports de dégazage sur le dessus
        for (int i = 0; i < 3; i++) {
            float off = -MB_L * 0.28f + i * MB_L * 0.28f;
            Vector3 port = Vector3Add(mb_ctr, Vector3Scale(cam_dir, off));
            port = Vector3Add(port, Vector3Scale(cam_up, MB_H * 0.5f + 0.001f));
            drawBoxFlat(port, MB_W * 0.48f, 0.005f, 0.008f, metal_dark);
        }
        // Rebord avant
        Vector3 mb_front = Vector3Add(mb_ctr, Vector3Scale(cam_dir, MB_L * 0.5f + 0.002f));
        drawBoxFlat(mb_front, MB_W + 0.004f, MB_H + 0.004f, 0.005f, shade(metal_base, +32));
        // Âme sombre (bouche du canon)
        drawBoxFlat(Vector3Add(mb_front, Vector3Scale(cam_dir, 0.006f)),
                    BRL_W - 0.018f, BRL_H - 0.018f, 0.014f, metal_dark);
    }

    // Point de départ du flash (au bout du frein de bouche)
    Vector3 flash_origin = Vector3Add(brl_tip, Vector3Scale(cam_dir, 0.040f));

    // Pierre dans la bouche du canon (se rétracte pendant bolt-action)
    {
        float stone_retract = pump_phase * 0.028f;
        Vector3 sp = Vector3Add(brl_tip, Vector3Scale(cam_dir, -0.026f - stone_retract));
        DrawSphereEx(sp, 0.018f, 7, 8, stone_col);
        // Masque le débordement
        drawBoxFlat(Vector3Add(brl_tip, Vector3Scale(cam_dir, 0.001f)),
                    BRL_W, BRL_H, 0.004f, shade(metal_base, +22));
    }

    // ── Guidon avant (front sight post + point rouge) ─────────────────────────
    {
        Vector3 fs = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L * 0.86f));
        fs = Vector3Add(fs, Vector3Scale(cam_up, BRL_H * 0.5f + 0.008f));
        drawBoxFlat(fs, 0.006f, 0.015f, 0.007f, shade(metal_base, +52));
        // Point rouge en sommet du guidon
        DrawSphereEx(Vector3Add(fs, Vector3Scale(cam_up, 0.009f)),
                     0.003f, 4, 4, {210, 35, 35, 255});
    }

    // ── Boîtier / receiver ────────────────────────────────────────────────────
    const float RCV_W = 0.072f, RCV_H = 0.082f, RCV_L = 0.168f;
    Vector3 recv = Vector3Add(grip, Vector3Scale(cam_up, 0.026f));
    recv = Vector3Add(recv, Vector3Scale(cam_dir, -0.062f));

    drawBoxShaded(recv, RCV_W, RCV_H, RCV_L, metal_base);

    // Chanfreins avant / arrière
    drawBoxFlat(Vector3Add(recv, Vector3Scale(cam_dir,  RCV_L * 0.5f + 0.001f)),
                RCV_W + 0.002f, RCV_H + 0.002f, 0.004f, shade(metal_base, +26));
    drawBoxFlat(Vector3Add(recv, Vector3Scale(cam_dir, -RCV_L * 0.5f - 0.001f)),
                RCV_W + 0.002f, RCV_H + 0.002f, 0.004f, shade(metal_base, +12));

    // ── Rail picatinny sur le dessus du boîtier (8 encoches) ─────────────────
    {
        Vector3 top_rail = Vector3Add(recv, Vector3Scale(cam_up, RCV_H * 0.5f + 0.006f));
        drawBoxShaded(top_rail, RCV_W + 0.004f, 0.010f, RCV_L, rail_col);
        for (int i = 0; i < 8; i++) {
            float off = -RCV_L * 0.5f + (i + 0.5f) * (RCV_L / 8.0f);
            Vector3 notch = Vector3Add(top_rail, Vector3Scale(cam_dir, off));
            notch = Vector3Add(notch, Vector3Scale(cam_up, 0.005f + 0.001f));
            drawBoxFlat(notch, RCV_W + 0.006f, 0.003f, 0.006f, shade(rail_col, -30));
        }
    }

    // ── Hausse arrière (rear sight U-notch) ───────────────────────────────────
    {
        // Assise sur le rail picatinny
        Vector3 rs = Vector3Add(recv, Vector3Scale(cam_up, RCV_H * 0.5f + 0.010f + 0.012f));
        rs = Vector3Add(rs, Vector3Scale(cam_dir, -RCV_L * 0.28f));
        // Oreille gauche
        drawBoxFlat(Vector3Add(rs, Vector3Scale(cam_right, -0.014f)),
                    0.007f, 0.018f, 0.008f, shade(metal_base, +46));
        // Oreille droite
        drawBoxFlat(Vector3Add(rs, Vector3Scale(cam_right, +0.014f)),
                    0.007f, 0.018f, 0.008f, shade(metal_base, +46));
        // Base reliant les deux oreilles
        drawBoxFlat(rs, 0.038f, 0.008f, 0.010f, shade(metal_base, +36));
        // Échancrure (U-notch) : boîte noire centrale entre les oreilles
        drawBoxFlat(Vector3Add(rs, Vector3Scale(cam_up, 0.009f)),
                    0.014f, 0.016f, 0.011f, {12, 12, 12, 255});
    }

    // ── Culasse coulissante (bolt-action, animée) ─────────────────────────────
    {
        // Glisse vers l'arrière proportionnellement à bolt_slide
        Vector3 bolt = Vector3Add(recv, Vector3Scale(cam_right, -(RCV_W * 0.5f + 0.004f)));
        bolt = Vector3Add(bolt, Vector3Scale(cam_up, RCV_H * 0.10f));
        bolt = Vector3Add(bolt, Vector3Scale(cam_dir, RCV_L * 0.16f - bolt_slide));
        drawBoxShaded(bolt, 0.006f, 0.022f, 0.056f, shade(metal_base, +34));
        // Poignée de culasse (tige perpendiculaire)
        Vector3 hbase = Vector3Add(bolt, Vector3Scale(cam_right, -0.016f));
        hbase = Vector3Add(hbase, Vector3Scale(cam_up, -0.006f));
        drawBoxFlat(hbase, 0.024f, 0.007f, 0.010f, shade(metal_base, +42));
        // Boule en bout de poignée
        DrawSphereEx(Vector3Add(hbase, Vector3Scale(cam_right, -0.014f)),
                     0.009f, 5, 5, shade(metal_base, +58));
    }

    // Bande pierre (indicateur couleur joueur, flanc droit du boîtier)
    {
        Vector3 strip = Vector3Add(recv, Vector3Scale(cam_right, RCV_W * 0.5f + 0.001f));
        strip = Vector3Add(strip, Vector3Scale(cam_up, 0.008f));
        drawBoxFlat(strip, 0.004f, RCV_H * 0.50f, RCV_L * 0.68f, stone_col);
        Vector3 hi = Vector3Add(strip, Vector3Scale(cam_up, RCV_H * 0.25f + 0.001f));
        drawBoxFlat(hi, 0.004f, 0.005f, RCV_L * 0.68f, stone_hi);
    }

    // Fenêtre d'éjection (flanc gauche)
    {
        Vector3 ej = Vector3Add(recv, Vector3Scale(cam_right, -(RCV_W * 0.5f + 0.001f)));
        ej = Vector3Add(ej, Vector3Scale(cam_up,  0.010f));
        ej = Vector3Add(ej, Vector3Scale(cam_dir, 0.018f));
        drawBoxFlat(ej, 0.004f, RCV_H * 0.32f, RCV_L * 0.28f, shade(metal_dark, -8));
    }

    // ── Chargeur ──────────────────────────────────────────────────────────────
    {
        const float MAG_W = 0.046f, MAG_H = 0.044f, MAG_L = RCV_L * 0.80f;
        Vector3 mag = Vector3Add(recv, Vector3Scale(cam_up, -(RCV_H * 0.5f + MAG_H * 0.5f + 0.002f)));
        mag = Vector3Add(mag, Vector3Scale(cam_dir, 0.012f));
        drawBoxShaded(mag, MAG_W, MAG_H, MAG_L, shade(metal_base, -6));
        // Baseplate
        Vector3 bp = Vector3Add(mag, Vector3Scale(cam_up, -(MAG_H * 0.5f + 0.004f)));
        drawBoxFlat(bp, MAG_W + 0.007f, 0.009f, MAG_L + 0.007f, shade(metal_base, +20));
        // Fenêtre de comptage (flanc droit, rectangle sombre)
        Vector3 win = Vector3Add(mag, Vector3Scale(cam_right, MAG_W * 0.5f + 0.001f));
        win = Vector3Add(win, Vector3Scale(cam_up, 0.004f));
        drawBoxFlat(win, 0.004f, MAG_H * 0.52f, MAG_L * 0.38f, metal_dark);
        // Anneau de séparation milieu
        drawBoxFlat(mag, MAG_W + 0.004f, MAG_H + 0.004f, 0.005f, shade(metal_base, +17));
    }

    // ── Poignée ───────────────────────────────────────────────────────────────
    {
        const float GRP_W = 0.046f, GRP_H = 0.096f, GRP_L = 0.042f;
        Vector3 gc = Vector3Add(grip, Vector3Scale(cam_up,  -0.054f));
        gc = Vector3Add(gc, Vector3Scale(cam_dir, -0.016f));
        drawBoxShaded(gc, GRP_W, GRP_H, GRP_L, shade(metal_base, -4));
        // 4 stries de grip
        for (int i = -1; i <= 2; i++) {
            Vector3 rib = Vector3Add(gc, Vector3Scale(cam_up, i * 0.020f - 0.010f));
            drawBoxFlat(Vector3Add(rib, Vector3Scale(cam_right,  GRP_W * 0.5f + 0.001f)),
                        0.003f, 0.007f, GRP_L * 0.85f, shade(metal_base, +24));
            drawBoxFlat(Vector3Add(rib, Vector3Scale(cam_right, -(GRP_W * 0.5f + 0.001f))),
                        0.003f, 0.007f, GRP_L * 0.85f, shade(metal_base, +24));
        }
        // Dosseret arrière (backstrap)
        Vector3 bs = Vector3Add(gc, Vector3Scale(cam_dir, -(GRP_L * 0.5f + 0.005f)));
        drawBoxShaded(bs, GRP_W, GRP_H * 0.90f, 0.009f, shade(metal_base, -12));
    }

    // ── Pontet de détente ─────────────────────────────────────────────────────
    {
        Vector3 tg = Vector3Add(grip, Vector3Scale(cam_dir,  0.022f));
        tg = Vector3Add(tg, Vector3Scale(cam_up, -0.034f));
        drawBoxShaded(tg, 0.054f, 0.007f, 0.042f, shade(metal_base, -2));
        Vector3 tr = Vector3Add(tg, Vector3Scale(cam_up,  -0.016f));
        tr = Vector3Add(tr, Vector3Scale(cam_dir, 0.005f));
        drawBoxFlat(tr, 0.011f, 0.025f, 0.007f, shade(metal_base, +14));
    }

    // ── Main droite ───────────────────────────────────────────────────────────
    {
        Vector3 rh = Vector3Add(grip, Vector3Scale(cam_up, -0.052f));
        drawBoxShaded(rh, 0.058f, 0.040f, 0.048f, skin_col);
        // Doigts repliés
        Vector3 fi = Vector3Add(rh, Vector3Scale(cam_dir, 0.028f));
        fi = Vector3Add(fi, Vector3Scale(cam_up, 0.010f));
        drawBoxShaded(fi, 0.054f, 0.019f, 0.024f, skin_col);
        // Phalange supérieure (knuckles)
        Vector3 kn = Vector3Add(fi, Vector3Scale(cam_up, 0.012f));
        drawBoxShaded(kn, 0.050f, 0.010f, 0.022f, shade(skin_col, +9));
        // Pouce
        Vector3 th = Vector3Add(rh, Vector3Scale(cam_right, -0.030f));
        th = Vector3Add(th, Vector3Scale(cam_up, 0.015f));
        drawBoxShaded(th, 0.014f, 0.014f, 0.030f, skin_dark);
    }

    // ── Avant-bras droit ──────────────────────────────────────────────────────
    {
        Vector3 wrist = Vector3Add(grip, Vector3Scale(cam_dir, -0.042f));
        wrist = Vector3Add(wrist, Vector3Scale(cam_up, -0.070f));
        Vector3 elbow = Vector3Add(wrist, Vector3Scale(cam_right,  0.18f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_up,  -0.11f));
        elbow = Vector3Add(elbow, Vector3Scale(cam_dir, -0.14f));
        DrawCylinderEx(wrist, elbow, 0.032f, 0.040f, 10, sleeve_col);
        // Poignet peau visible à la jonction
        DrawCylinderEx(Vector3Add(wrist, Vector3Scale(cam_dir, 0.010f)),
                       wrist, 0.028f, 0.032f, 10, skin_col);
    }

    // ── Flash de tir — starburst 8 rayons + halo orange ──────────────────────
    if (recoil > 0.5f) {
        float fa        = (recoil - 0.5f) * 2.0f;
        Color flash     = {stone_col.r, stone_col.g, stone_col.b, (unsigned char)(fa * 220)};
        Color flash_glo = {255, 170, 50,  (unsigned char)(fa * 160)};
        Color flash_cor = {255, 255, 255, (unsigned char)(fa * 190)};

        Vector3 fp = Vector3Add(flash_origin, Vector3Scale(cam_dir, 0.04f * fa));

        // Sphères concentriques
        DrawSphereEx(fp, 0.070f * fa, 5, 6, flash);
        DrawSphereEx(fp, 0.045f * fa, 5, 5, flash_glo);
        DrawSphereEx(fp, 0.024f * fa, 4, 4, flash_cor);

        // 4 rayons cardinaux
        float r_len = 0.22f * fa;
        DrawLine3D(fp, Vector3Add(fp, Vector3Scale(cam_up,     r_len)), flash);
        DrawLine3D(fp, Vector3Add(fp, Vector3Scale(cam_up,    -r_len)), flash);
        DrawLine3D(fp, Vector3Add(fp, Vector3Scale(cam_right,  r_len)), flash);
        DrawLine3D(fp, Vector3Add(fp, Vector3Scale(cam_right, -r_len)), flash);

        // 4 rayons diagonaux (légèrement plus courts)
        float d_len = 0.16f * fa;
        auto diag_pt = [&](float su, float sr) -> Vector3 {
            return Vector3Add(fp, Vector3Add(Vector3Scale(cam_up,    su * d_len),
                                             Vector3Scale(cam_right, sr * d_len)));
        };
        DrawLine3D(fp, diag_pt( 1,  1), flash_glo);
        DrawLine3D(fp, diag_pt(-1, -1), flash_glo);
        DrawLine3D(fp, diag_pt( 1, -1), flash_glo);
        DrawLine3D(fp, diag_pt(-1,  1), flash_glo);
    }

    rlDisableBackfaceCulling();   // restaurer l'état (HUD et Board3D n'utilisent pas le culling)
    EndMode3D();
}
