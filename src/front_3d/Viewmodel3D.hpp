#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Viewmodel3D.hpp — Rendu de l'arme FPS en première personne
//
// Le viewmodel est rendu dans un espace caméra virtuel séparé (vm_cam),
// indépendant de la scène monde. Cela garantit que l'arme reste toujours
// devant la caméra, quel que soit l'angle de vue.
// ─────────────────────────────────────────────────────────────────────────────

#include "raylib.h"
#include "front3d_types.hpp"
#include <vector>

// Durée de l'animation bolt-action (verrou coulissant post-tir), en secondes
static constexpr float BOLT_ANIM_DURATION = 0.60f;

// draw_viewmodel_3d — arme + mains + avant-bras en première personne
//
// Paramètres :
//   camera       : caméra principale (pour extraire les vecteurs de base)
//   bob_time     : timer croissant pour le balancement idle sinusoïdal
//   recoil       : intensité du recul [0..1], décroît à 6× dt après chaque tir
//   bolt_timer   : temps restant de l'animation bolt-action [0..BOLT_ANIM_DURATION]
//   shell_casings: liste des douilles éjectées en cours d'animation
void draw_viewmodel_3d(const Camera3D&                  camera,
                       float                            bob_time,
                       float                            recoil,
                       float                            bolt_timer,
                       const std::vector<ShellCasing>&  shell_casings);
