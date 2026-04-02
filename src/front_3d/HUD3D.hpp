#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// HUD3D.hpp — HUD overlay 2D (Pass 3 du pipeline de rendu)
//
// Toutes ces fonctions s'appellent après EndMode3D() + rlDisableDepthTest()
// pour garantir que le 2D s'affiche toujours devant la scène 3D.
// ─────────────────────────────────────────────────────────────────────────────

#include "../engine/GameState.hpp"
#include "raylib.h"
#include "front3d_types.hpp"
#include <vector>

// Réticule en croix au centre de l'écran
//   Vert si has_hover=true (réticule sur une case valide), blanc sinon.
void draw_crosshair(bool has_hover);

// Barre de statut en bas de l'écran : mode, timer IA, tour, captures, coordonnée visée
void draw_hud(const GameState& state, UI3DState current_state,
              bool has_hover, int hovered_row, int hovered_col);

// Overlay noir semi-transparent + message de fin + score IA + instruction [R]
// winner : gagnant réel fourni par GameUI3D::apply_win_check (fiable même quand
//          les deux joueurs ont 5 pierres simultanément sur le plateau).
void draw_game_over_overlay(const GameState& state, UI3DState current_state, Player winner);

// Croix X animées aux positions des pierres capturées (GetWorldToScreen)
void draw_capture_anims_3d(const std::vector<CaptureAnim3D>& anims,
                           const Camera3D& camera);

// Panneau historique des coups (côté gauche de l'écran, semi-transparent)
void draw_history(const GameState& state);
