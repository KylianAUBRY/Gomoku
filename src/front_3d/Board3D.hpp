#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Board3D.hpp — Rendu 3D du goban : plateau, pierres, indicateurs, raycast
//
// ⚠ ORDRE D'INCLUSION dans les .cpp qui sauvegardent kBlack/kWhite :
//   1. #include "../engine/GameState.hpp"
//   2. static constexpr Cell kBlack = BLACK;  ← sauvegardes
//   3. #include "Board3D.hpp"                 ← inclut raylib, écrase les macros
// ─────────────────────────────────────────────────────────────────────────────

#include "../engine/GameState.hpp"
#include "raylib.h"
#include "front3d_types.hpp"

// ── Raycast ───────────────────────────────────────────────────────────────────
// Projette un rayon depuis le centre de l'écran vers le plan Z=0 du goban.
// Retourne true si le rayon touche une case valide ; out_row/out_col mis à jour.
bool board3d_raycast(const Camera3D& camera, int& out_row, int& out_col);

// ── Rendu scène (à appeler entre BeginMode3D / EndMode3D) ────────────────────
void draw_board_3d();                                        // Plateau + grille + hoshi
void draw_stones_3d(const GameState& state);                 // Pierres noires et blanches
void draw_ai_highlight_3d(int row, int col, float timer,
                          bool is_solo);                     // Anneau rouge fadeout dernier coup IA
void draw_hover_indicator(const GameState& state,
                          bool has_hover, int row, int col,
                          bool is_solo);                     // Fantôme + cercle vert case visée
void draw_best_move_3d(const GameState& state);              // Cercle pulsant suggestion (mode Multi)
