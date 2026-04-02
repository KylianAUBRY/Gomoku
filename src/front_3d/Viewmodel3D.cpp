// ─────────────────────────────────────────────────────────────────────────────
// Viewmodel3D.cpp — Pierre-shooter en première personne (Option B)
//
// Rendu : DrawTriangle3D ne bénéficie d'aucun éclairage Raylib.
// Solution : drawBoxShaded — chaque face reçoit une teinte distincte simulant
//   un éclairage venant du haut-droite. Donne une vraie perception de volume.
//
// Design : pistolet lanceur de pierres Gomoku.
//   - Silhouette pistolet classique (corps horizontal + poignée verticale).
//   - Bande de couleur pierre sur le côté du boîtier (noir ou blanc selon
//   joueur).
//   - Pierre visible dans la bouche du canon.
//   - Animation de rechargement : le canon recule légèrement au tir.
// ─────────────────────────────────────────────────────────────────────────────

#include "Viewmodel3D.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm> // std::max, std::min
#include <cmath>

void draw_viewmodel_3d(const Camera3D &camera, float bob_time, float recoil,
                       float bolt_timer, bool is_black) {
  rlDrawRenderBatchActive();

  // ── Vecteurs de base ──────────────────────────────────────────────────────
  Vector3 cam_dir =
      Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  Vector3 cam_right = Vector3Normalize(Vector3CrossProduct(cam_dir, camera.up));
  Vector3 cam_up = Vector3Normalize(Vector3CrossProduct(cam_right, cam_dir));

  Camera3D vm_cam = {};
  vm_cam.position = {0.0f, 0.0f, 0.0f};
  vm_cam.target = cam_dir;
  vm_cam.up = cam_up;
  vm_cam.fovy = 65.0f;
  vm_cam.projection = CAMERA_PERSPECTIVE;

  // ── Animations ────────────────────────────────────────────────────────────
  float bob_x = sinf(bob_time * 1.8f) * 0.012f;
  float bob_y = sinf(bob_time * 3.6f) * 0.006f;

  float recoil_back = recoil * 0.16f;
  float recoil_up = recoil * 0.04f;

  // Rechargement : le canon recule puis revient
  float pump_t = bolt_timer / BOLT_ANIM_DURATION;
  float pump_phase = (pump_t > 0.5f) ? (1.0f - pump_t) * 2.0f : pump_t * 2.0f;
  float pump_back = pump_phase * 0.07f;

  // Ancrage bas-droite écran
  float base_right = 0.30f + bob_x;
  float base_down = -0.22f + bob_y;
  float base_forward = 0.52f - recoil_back;

  Vector3 grip = Vector3Scale(cam_right, base_right);
  grip = Vector3Add(grip, Vector3Scale(cam_up, base_down));
  grip = Vector3Add(grip, Vector3Scale(cam_dir, base_forward));
  grip = Vector3Add(grip, Vector3Scale(cam_up, recoil_up));

  // ── Palette ───────────────────────────────────────────────────────────────
  // Métal : gunmetal anthracite chaud
  Color metal = {58, 54, 50, 255};
  // Couleur pierre chargée
  Color stone_col =
      is_black ? Color{22, 22, 26, 255} : Color{232, 226, 210, 255};
  Color stone_hi =
      is_black ? Color{70, 70, 85, 255} : Color{255, 248, 230, 255};
  // Peau / manche
  Color skin_col = {212, 172, 132, 255};
  Color skin_dark = {182, 142, 102, 255};
  Color sleeve_col = {50, 50, 50, 255};

  // ── Helpers ───────────────────────────────────────────────────────────────
  // shade : applique un delta de luminosité en clampant [0, 255]
  auto shade = [](Color c, int d) -> Color {
    int r = c.r + d, g = c.g + d, b = c.b + d;
    return {(unsigned char)(r < 0     ? 0
                            : r > 255 ? 255
                                      : r),
            (unsigned char)(g < 0     ? 0
                            : g > 255 ? 255
                                      : g),
            (unsigned char)(b < 0     ? 0
                            : b > 255 ? 255
                                      : b),
            c.a};
  };

  // drawBoxShaded : boîte 3D avec éclairage simulé par face.
  // Lumière fictive venant du haut-droite → 6 teintes distinctes.
  //   Top    : +40  (face la plus éclairée)
  //   Right  : +18  (face latérale éclairée)
  //   Front  :  +8  (face avant, légèrement illuminée)
  //   Left   : -12  (face latérale dans l'ombre)
  //   Back   : -22  (face arrière, sombre)
  //   Bottom : -32  (face inférieure, la plus sombre)
  auto drawBoxShaded = [&](Vector3 c, float w, float h, float l, Color col) {
    float hw = w * 0.5f, hh = h * 0.5f, hl = l * 0.5f;
    auto p = [&](float rx, float ry, float rz) -> Vector3 {
      return Vector3Add(c, Vector3Add(Vector3Scale(cam_right, rx),
                                      Vector3Add(Vector3Scale(cam_up, ry),
                                                 Vector3Scale(cam_dir, rz))));
    };
    Vector3 v[8] = {p(-hw, -hh, -hl), p(hw, -hh, -hl), p(-hw, hh, -hl),
                    p(hw, hh, -hl),   p(-hw, -hh, hl), p(hw, -hh, hl),
                    p(-hw, hh, hl),   p(hw, hh, hl)};
    Color top = shade(col, +40);
    Color bot = shade(col, -32);
    Color front = shade(col, +8);
    Color back = shade(col, -22);
    Color right = shade(col, +18);
    Color left = shade(col, -12);
    DrawTriangle3D(v[4], v[5], v[7], front);
    DrawTriangle3D(v[4], v[7], v[6], front);
    DrawTriangle3D(v[1], v[0], v[2], back);
    DrawTriangle3D(v[1], v[2], v[3], back);
    DrawTriangle3D(v[2], v[3], v[7], top);
    DrawTriangle3D(v[2], v[7], v[6], top);
    DrawTriangle3D(v[0], v[1], v[5], bot);
    DrawTriangle3D(v[0], v[5], v[4], bot);
    DrawTriangle3D(v[1], v[3], v[7], right);
    DrawTriangle3D(v[1], v[7], v[5], right);
    DrawTriangle3D(v[4], v[6], v[2], left);
    DrawTriangle3D(v[4], v[2], v[0], left);
  };

  // drawBoxFlat : boîte couleur uniforme, pour les accents fins (filets,
  // bords).
  auto drawBoxFlat = [&](Vector3 c, float w, float h, float l, Color col) {
    float hw = w * 0.5f, hh = h * 0.5f, hl = l * 0.5f;
    auto p = [&](float rx, float ry, float rz) -> Vector3 {
      return Vector3Add(c, Vector3Add(Vector3Scale(cam_right, rx),
                                      Vector3Add(Vector3Scale(cam_up, ry),
                                                 Vector3Scale(cam_dir, rz))));
    };
    Vector3 v[8] = {p(-hw, -hh, -hl), p(hw, -hh, -hl), p(-hw, hh, -hl),
                    p(hw, hh, -hl),   p(-hw, -hh, hl), p(hw, -hh, hl),
                    p(-hw, hh, hl),   p(hw, hh, hl)};
    DrawTriangle3D(v[4], v[5], v[7], col);
    DrawTriangle3D(v[4], v[7], v[6], col);
    DrawTriangle3D(v[1], v[0], v[2], col);
    DrawTriangle3D(v[1], v[2], v[3], col);
    DrawTriangle3D(v[2], v[3], v[7], col);
    DrawTriangle3D(v[2], v[7], v[6], col);
    DrawTriangle3D(v[0], v[1], v[5], col);
    DrawTriangle3D(v[0], v[5], v[4], col);
    DrawTriangle3D(v[1], v[3], v[7], col);
    DrawTriangle3D(v[1], v[7], v[5], col);
    DrawTriangle3D(v[4], v[6], v[2], col);
    DrawTriangle3D(v[4], v[2], v[0], col);
  };

  rlDisableDepthTest();
  BeginMode3D(vm_cam);

  // ── Canon (recule au tir avec pump_back) ─────────────────────────────────
  const float BRL_L = 0.22f, BRL_W = 0.048f, BRL_H = 0.048f;
  Vector3 brl_org = Vector3Add(grip, Vector3Scale(cam_up, 0.034f));
  brl_org = Vector3Add(brl_org, Vector3Scale(cam_dir, 0.06f - pump_back));
  Vector3 brl_ctr = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L * 0.5f));
  Vector3 brl_tip = Vector3Add(brl_org, Vector3Scale(cam_dir, BRL_L));

  drawBoxShaded(brl_ctr, BRL_W, BRL_H, BRL_L, metal);

  // Filet de guidon sur le dessus du canon
  drawBoxFlat(Vector3Add(brl_ctr, Vector3Scale(cam_up, BRL_H * 0.5f + 0.001f)),
              0.006f, 0.003f, BRL_L * 0.85f, shade(metal, +55));

  // Bouche : rebord + âme sombre
  drawBoxFlat(Vector3Add(brl_tip, Vector3Scale(cam_dir, 0.003f)),
              BRL_W + 0.006f, BRL_H + 0.006f, 0.005f, shade(metal, +30));
  drawBoxFlat(Vector3Add(brl_tip, Vector3Scale(cam_dir, 0.007f)),
              BRL_W - 0.018f, BRL_H - 0.018f, 0.010f, shade(metal, -45));

  // ── Pierre dans la bouche du canon ────────────────────────────────────────
  // Petite sphère visible juste en retrait de la bouche — identifie
  // visuellement la pierre qui va être tirée.
  Vector3 stone_pos = Vector3Add(brl_tip, Vector3Scale(cam_dir, -0.022f));
  DrawSphereEx(stone_pos, 0.016f, 6, 8, stone_col);
  // Fin rebord de bouche par-dessus (masque proprement)
  drawBoxFlat(Vector3Add(brl_tip, Vector3Scale(cam_dir, 0.001f)), BRL_W, BRL_H,
              0.003f, shade(metal, +25));

  // ── Boîtier / receiver ────────────────────────────────────────────────────
  const float RCV_W = 0.068f, RCV_H = 0.078f, RCV_L = 0.160f;
  Vector3 recv = Vector3Add(grip, Vector3Scale(cam_up, 0.022f));
  recv = Vector3Add(recv, Vector3Scale(cam_dir, -0.060f));

  drawBoxShaded(recv, RCV_W, RCV_H, RCV_L, metal);

  // Arête avant et arrière du boîtier (chanfrein)
  drawBoxFlat(Vector3Add(recv, Vector3Scale(cam_dir, RCV_L * 0.5f + 0.001f)),
              RCV_W + 0.002f, RCV_H + 0.002f, 0.004f, shade(metal, +22));
  drawBoxFlat(Vector3Add(recv, Vector3Scale(cam_dir, -RCV_L * 0.5f - 0.001f)),
              RCV_W + 0.002f, RCV_H + 0.002f, 0.004f, shade(metal, +10));

  // Bande pierre : indicateur couleur joueur sur le flanc droit du boîtier
  // Recueillie en retrait de 0.001 pour ne pas Z-fighter
  {
    Vector3 strip_ctr =
        Vector3Add(recv, Vector3Scale(cam_right, RCV_W * 0.5f + 0.001f));
    strip_ctr = Vector3Add(strip_ctr, Vector3Scale(cam_up, 0.010f));
    // Fond de la bande (couleur pierre)
    drawBoxFlat(strip_ctr, 0.004f, RCV_H * 0.55f, RCV_L * 0.72f, stone_col);
    // Halo supérieur (teinte highlight)
    Vector3 hi_ctr = Vector3Add(
        strip_ctr, Vector3Scale(cam_up, RCV_H * 0.55f * 0.5f + 0.001f));
    drawBoxFlat(hi_ctr, 0.004f, 0.004f, RCV_L * 0.72f, stone_hi);
  }

  // ── Fenêtre d'éjection (creuse visible sur le flanc gauche) ──────────────
  {
    Vector3 ej =
        Vector3Add(recv, Vector3Scale(cam_right, -(RCV_W * 0.5f + 0.001f)));
    ej = Vector3Add(ej, Vector3Scale(cam_up, 0.008f));
    ej = Vector3Add(ej, Vector3Scale(cam_dir, 0.020f));
    drawBoxFlat(ej, 0.004f, RCV_H * 0.35f, RCV_L * 0.30f, shade(metal, -40));
  }

  // ── Chargeur (tube sous le boîtier) ──────────────────────────────────────
  // Simule un réservoir de pierres tubulaire
  {
    const float MAG_W = 0.042f, MAG_H = 0.036f, MAG_L = RCV_L * 0.82f;
    Vector3 mag = Vector3Add(
        recv, Vector3Scale(cam_up, -(RCV_H * 0.5f + MAG_H * 0.5f + 0.002f)));
    mag = Vector3Add(mag, Vector3Scale(cam_dir, 0.014f));
    drawBoxShaded(mag, MAG_W, MAG_H, MAG_L, shade(metal, -8));
    // Anneau de séparation milieu
    drawBoxFlat(mag, MAG_W + 0.003f, MAG_H + 0.003f, 0.004f, shade(metal, +15));
  }

  // ── Poignée ───────────────────────────────────────────────────────────────
  {
    const float GRP_W = 0.044f, GRP_H = 0.092f, GRP_L = 0.040f;
    Vector3 gc = Vector3Add(grip, Vector3Scale(cam_up, -0.052f));
    gc = Vector3Add(gc, Vector3Scale(cam_dir, -0.014f));
    drawBoxShaded(gc, GRP_W, GRP_H, GRP_L, shade(metal, -6));
    // Stries de grip (3 nervures horizontales)
    for (int i = -1; i <= 1; i++) {
      Vector3 rib = Vector3Add(gc, Vector3Scale(cam_up, i * 0.024f));
      drawBoxFlat(
          Vector3Add(rib, Vector3Scale(cam_right, GRP_W * 0.5f + 0.001f)),
          0.003f, 0.006f, GRP_L * 0.80f, shade(metal, +20));
      drawBoxFlat(
          Vector3Add(rib, Vector3Scale(cam_right, -(GRP_W * 0.5f + 0.001f))),
          0.003f, 0.006f, GRP_L * 0.80f, shade(metal, +20));
    }
  }

  // ── Pontet de détente ─────────────────────────────────────────────────────
  {
    Vector3 tg = Vector3Add(grip, Vector3Scale(cam_dir, 0.020f));
    tg = Vector3Add(tg, Vector3Scale(cam_up, -0.032f));
    drawBoxShaded(tg, 0.050f, 0.007f, 0.040f, shade(metal, -4));
    // Détente
    Vector3 tr = Vector3Add(tg, Vector3Scale(cam_up, -0.014f));
    tr = Vector3Add(tr, Vector3Scale(cam_dir, 0.004f));
    drawBoxFlat(tr, 0.010f, 0.022f, 0.006f, shade(metal, +12));
  }

  // ── Main droite ───────────────────────────────────────────────────────────
  {
    Vector3 rh = Vector3Add(grip, Vector3Scale(cam_up, -0.050f));
    drawBoxShaded(rh, 0.056f, 0.038f, 0.046f, skin_col);
    // Doigts (repli vers l'avant)
    Vector3 fi = Vector3Add(rh, Vector3Scale(cam_dir, 0.026f));
    fi = Vector3Add(fi, Vector3Scale(cam_up, 0.008f));
    drawBoxShaded(fi, 0.052f, 0.018f, 0.022f, skin_col);
    // Pouce
    Vector3 th = Vector3Add(rh, Vector3Scale(cam_right, -0.029f));
    th = Vector3Add(th, Vector3Scale(cam_up, 0.013f));
    drawBoxShaded(th, 0.014f, 0.013f, 0.028f, skin_dark);
  }

  // ── Avant-bras droit ──────────────────────────────────────────────────────
  {
    Vector3 wrist = Vector3Add(grip, Vector3Scale(cam_dir, -0.040f));
    wrist = Vector3Add(wrist, Vector3Scale(cam_up, -0.068f));
    Vector3 elbow = Vector3Add(wrist, Vector3Scale(cam_right, 0.16f));
    elbow = Vector3Add(elbow, Vector3Scale(cam_up, -0.10f));
    elbow = Vector3Add(elbow, Vector3Scale(cam_dir, -0.13f));
    DrawCylinderEx(wrist, elbow, 0.030f, 0.037f, 10, sleeve_col);
    // Poignet (peau visible à la jonction manche/main)
    DrawCylinderEx(Vector3Add(wrist, Vector3Scale(cam_dir, 0.008f)), wrist,
                   0.027f, 0.030f, 10, skin_col);
  }

  // ── Flash de tir ─────────────────────────────────────────────────────────
  // Burst couleur pierre : bref et intense sur la première moitié du recul.
  if (recoil > 0.5f) {
    float fa = (recoil - 0.5f) * 2.0f; // [0..1]
    unsigned char a_main = (unsigned char)(fa * 200);
    unsigned char a_core = (unsigned char)(fa * 160);
    Color flash = {stone_col.r, stone_col.g, stone_col.b, a_main};
    Color flash_core = {255, 255, 255, a_core};
    Vector3 fp = Vector3Add(brl_tip, Vector3Scale(cam_dir, 0.05f * fa));
    DrawSphereEx(fp, 0.055f * fa, 5, 6, flash);
    DrawSphereEx(fp, 0.028f * fa, 4, 5, flash_core);
    DrawLine3D(brl_tip, Vector3Add(brl_tip, Vector3Scale(cam_up, 0.18f * fa)),
               flash);
    DrawLine3D(brl_tip, Vector3Add(brl_tip, Vector3Scale(cam_up, -0.18f * fa)),
               flash);
    DrawLine3D(brl_tip,
               Vector3Add(brl_tip, Vector3Scale(cam_right, 0.18f * fa)), flash);
    DrawLine3D(brl_tip,
               Vector3Add(brl_tip, Vector3Scale(cam_right, -0.18f * fa)),
               flash);
  }

  EndMode3D();
  rlEnableDepthTest();
}
