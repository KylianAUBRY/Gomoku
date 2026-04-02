#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// front3d_compat.hpp — Helpers pour le conflit de macros Raylib / engine
//
// ⚠ ORDRE D'INCLUSION OBLIGATOIRE dans chaque .cpp qui utilise ce header :
//
//   1. #include "../engine/GameState.hpp"    ← définit enum Cell { EMPTY=0, BLACK=1, WHITE=2 }
//   2. static constexpr Cell kBlack = BLACK; ← sauvegarder AVANT raylib
//      static constexpr Cell kWhite = WHITE;
//      static constexpr Cell kEmpty = EMPTY;
//   3. #include "raylib.h"                   ← écrase BLACK/WHITE comme macros Color
//   4. #include "front3d_compat.hpp"         ← safe : n'utilise pas les macros
//
// playerToCell() utilise static_cast<Cell> (valeurs fixes EMPTY=0, BLACK=1, WHITE=2)
// et n'a donc pas besoin des sauvegardes kBlack/kWhite.
// ─────────────────────────────────────────────────────────────────────────────

// ── Helpers Player ────────────────────────────────────────────────────────────
// Évitent d'écrire Player::BLACK dans les .cpp (la macro BLACK l'expanderait).

static inline bool   isBlackPlayer(Player p)  { return (int)p == 1; }
static inline Player makePlayerBlack()         { return static_cast<Player>(1); }
static inline Player makePlayerWhite()         { return static_cast<Player>(2); }
static inline Player makePlayerNone()          { return static_cast<Player>(0); }

// Conversion Player → Cell (enum Cell non-class : EMPTY=0, BLACK=1, WHITE=2)
static inline Cell playerToCell(Player p) {
    return static_cast<Cell>(isBlackPlayer(p) ? 1 : 2);
}

// ── Conversion grille → monde 3D ─────────────────────────────────────────────
// col (0-18) → X monde direct
// row (0-18) → Y monde inversé : row=0 (haut du goban) = Y=18
static inline Vector3 boardToWorld(int row, int col, float z_offset = 0.0f) {
    return {(float)col, 18.0f - (float)row, z_offset};
}
