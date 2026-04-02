#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// front3d_types.hpp — Types partagés du frontend 3D
//
// N'inclut ni engine ni raylib : peut être inclus dans n'importe quel contexte
// sans contrainte d'ordre. Toutes les autres unités front_3d l'incluent.
// ─────────────────────────────────────────────────────────────────────────────

#include <vector>

// Machine à états de l'UI 3D
enum class UI3DState { MAIN_MENU, PLAYING_SOLO, PLAYING_MULTI };

// Animation de capture : croix X fadeout à la position d'une pierre capturée
struct CaptureAnim3D {
    int   row, col;
    float timer; // Durée de vie décroissante (1.0s → 0.0s)
};

// Douille éjectée — physique simple dans l'espace viewmodel
// r/u/f : axes right/up/forward relatifs au grip
struct ShellCasing {
    float r, u, f;    // Position offset depuis le grip
    float vr, vu, vf; // Vitesse initiale
    float timer;      // Durée de vie (0.7s)
    float rot;        // Angle de rotation (degrés)
    float rot_spd;    // Vitesse angulaire (degrés/s)
};
