// ─────────────────────────────────────────────────────────────────────────────
// Board3D.cpp — Rendu 3D du goban : plateau, pierres, indicateurs, raycast
// ─────────────────────────────────────────────────────────────────────────────

// ⚠ ORDRE OBLIGATOIRE : engine avant sauvegardes, sauvegardes avant raylib
#include "../engine/GameState.hpp"

// Sauvegardes Cell AVANT que raylib définisse ses macros Color
static constexpr Cell kBlack = BLACK;
static constexpr Cell kEmpty = EMPTY;
// kWhite non nécessaire ici : Board3D ne compare que kBlack et kEmpty

// Board3D.hpp inclut engine (no-op via pragma once) puis raylib
#include "Board3D.hpp"
#include "front3d_compat.hpp" // boardToWorld, isBlackPlayer (après raylib)

#include <cmath>

// ── Constantes géométrie
// ──────────────────────────────────────────────────────
static constexpr float STONE_RADIUS = 0.35f;

// ─────────────────────────────────────────────────────────────────────────────
// board3d_raycast — centre écran → case du goban
//
// Étape 1 : GetScreenToWorldRay(centre, camera) → rayon 3D depuis la caméra.
// Étape 2 : Intersection rayon / plan Z=0 (goban dans le plan Z=0) :
//           t = -ray.position.z / ray.direction.z
// Étape 3 : Convertir le point d'impact en (col, row).
// ─────────────────────────────────────────────────────────────────────────────
bool board3d_raycast(const Camera3D &camera, int &out_row, int &out_col) {
  Ray ray = GetScreenToWorldRay(
      {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f}, camera);

  // Éviter la division par zéro (rayon parallèle au plan Z=0)
  if (fabsf(ray.direction.z) < 0.0001f)
    return false;

  float t = -ray.position.z / ray.direction.z;
  if (t < 0.0f)
    return false; // Goban derrière la caméra

  float hit_x = ray.position.x + t * ray.direction.x;
  float hit_y = ray.position.y + t * ray.direction.y;

  int col = (int)roundf(hit_x);
  int row = (int)roundf(18.0f - hit_y); // Inversion Y : row=0 → Y monde=18

  if (col < 0 || col >= 19 || row < 0 || row >= 19)
    return false;

  out_row = row;
  out_col = col;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_board_3d — plateau et grille en 3D
//
// Surface en deux quads (DrawTriangle3D, pas DrawCube) pour une couleur
// uniforme sans shading automatique. Grille à Z=0.01f (offset anti Z-fighting).
// Hoshi : 9 positions standard, petites sphères DARKGRAY.
// ─────────────────────────────────────────────────────────────────────────────
void draw_board_3d() {
  Color base_col = {180, 140, 70, 255}; // Bordure (bois sombre)
  Color surface_col = {220, 179, 92,
                       255}; // Surface jouable (même couleur que 2D)

  // Bordure (légèrement plus grande et plus basse que la surface)
  Vector3 b0 = {-1.0f, -1.0f, -0.2f}, b1 = {19.0f, -1.0f, -0.2f};
  Vector3 b2 = {19.0f, 19.0f, -0.2f}, b3 = {-1.0f, 19.0f, -0.2f};
  DrawTriangle3D(b0, b1, b2, base_col);
  DrawTriangle3D(b0, b2, b3, base_col);

  // Surface jouable (dépasse légèrement les lignes grâce à la marge m)
  float m = 0.25f;
  Vector3 s0 = {-m, -m, 0.0f}, s1 = {18.0f + m, -m, 0.0f};
  Vector3 s2 = {18.0f + m, 18.0f + m, 0.0f}, s3 = {-m, 18.0f + m, 0.0f};
  DrawTriangle3D(s0, s1, s2, surface_col);
  DrawTriangle3D(s0, s2, s3, surface_col);

  // Grille (Z=0.01f : léger offset au-dessus de la surface pour éviter le
  // Z-fighting)
  Color lineColor = {0, 0, 0, 255};
  for (int i = 0; i < 19; i++) {
    DrawLine3D({0.0f, (float)i, 0.01f}, {18.0f, (float)i, 0.01f}, lineColor);
    DrawLine3D({(float)i, 0.0f, 0.01f}, {(float)i, 18.0f, 0.01f}, lineColor);
  }

  // Hoshi : 9 points étoiles standard du Goban 19×19
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
// STONE_RADIUS = 0.35f : légèrement plus petit que 0.5 (demi-cellule)
// pour laisser la grille visible, cohérent avec le 2D (CELL_SIZE * 0.4f).
// z_offset = STONE_RADIUS * 0.6f : la sphère repose légèrement au-dessus du
// plan.
// ─────────────────────────────────────────────────────────────────────────────
void draw_stones_3d(const GameState &state) {
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
// Deux DrawCircle3D de rayon légèrement différent pour plus de lisibilité.
// Alpha décroît de 255 à 0 sur 0.5s (timer normalisé sur 0.5).
// ─────────────────────────────────────────────────────────────────────────────
void draw_ai_highlight_3d(int row, int col, float timer, bool is_solo) {
  if (!is_solo || timer <= 0.0f || row < 0)
    return;

  float alpha = timer / 0.5f; // Normalisation 0→1
  unsigned char a = (unsigned char)(alpha * 255);

  Vector3 pos = boardToWorld(row, col, 0.08f);
  DrawCircle3D(pos, 0.48f, {0.0f, 0.0f, 1.0f}, 0.0f, {220, 50, 50, a});
  DrawCircle3D(pos, 0.52f, {0.0f, 0.0f, 1.0f}, 0.0f,
               {220, 50, 50, (unsigned char)(a / 2)});
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_hover_indicator — indicateur de la case visée par le réticule
//
// Conditions d'affichage :
//   - Raycast valide (has_hover)
//   - Partie en cours (non terminée)
//   - En Solo : c'est le tour du joueur humain (Noir)
//   - La case est vide
// ─────────────────────────────────────────────────────────────────────────────
void draw_hover_indicator(const GameState &state, bool has_hover, int row,
                          int col, bool is_solo) {
  if (!has_hover || state.game_over)
    return;
  if (is_solo && !isBlackPlayer(state.current_player))
    return;
  if (!state.board.isEmpty(row, col))
    return;

  Vector3 pos = boardToWorld(row, col, STONE_RADIUS * 0.6f);
  Color ghost = isBlackPlayer(state.current_player)
                    ? Color{20, 20, 20, 100}     // Fantôme noir
                    : Color{240, 240, 240, 100}; // Fantôme blanc
  DrawSphere(pos, STONE_RADIUS, ghost);

  // Cercle vert au sol pour indiquer la case précisément
  Vector3 ring_pos = boardToWorld(row, col, 0.05f);
  DrawCircle3D(ring_pos, 0.45f, {0.0f, 0.0f, 1.0f}, 0.0f, GREEN);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_best_move_3d — suggestion de coup (cercle pulsant rouge)
//
// Utilisé en mode Multi (suggestion calculée après chaque coup humain).
// pulse = (sin(time*8) + 1) / 2 → alpha oscille entre 160 et 255.
// ─────────────────────────────────────────────────────────────────────────────
void draw_best_move_3d(const GameState &state) {
  if (state.game_over)
    return;
  if (state.best_move_suggestion.row < 0 || state.best_move_suggestion.col < 0)
    return;

  Vector3 pos = boardToWorld(state.best_move_suggestion.row,
                             state.best_move_suggestion.col, 0.05f);

  float pulse = (sinf((float)GetTime() * 8.0f) + 1.0f) * 0.5f;
  unsigned char alpha = (unsigned char)(160 + pulse * 95);

  DrawCircle3D(pos, 0.50f, {0.0f, 0.0f, 1.0f}, 0.0f, {255, 0, 0, alpha});
  DrawCircle3D(pos, 0.55f, {0.0f, 0.0f, 1.0f}, 0.0f,
               {255, 60, 60, (unsigned char)(alpha / 2)});
}

// Raylib et SFML : deux lib sous openGL
// les deux tournent sous GPU

// 2 triangles pour le goban (un quad)
// ~19×2 lignes pour la grille
// ~50-100 sphères max (pierres)
// Le viewmodel (quelques dizaines de boîtes)

// Vertex shader : transforme chaque sommet du triangle de l'espace 3D vers
// l'espace écran (projection) Rastérisation : détermine quels pixels de l'écran
// sont "à l'intérieur" du triangle projeté Fragment shader : calcule la couleur
// de chaque pixel couvert