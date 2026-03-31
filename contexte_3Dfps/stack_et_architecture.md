**Stack Technique**
* **Langage** : C++ (identique au reste du projet).
* **Rendu 3D** : Raylib (leger, pas de dependances lourdes, API C compatible C++).
* **Moteur IA** : Reutilisation directe de `Gomoku`, `BitBoard`, `GameState`, `Rules` depuis `src/engine/`.

**Architecture Logicielle**

```
src/
├── engine/              # Inchange — moteur partage
│   ├── Gomoku.hpp/cpp
│   ├── GameState.hpp
│   ├── Bitboard.hpp
│   ├── Rules.hpp/cpp
│   └── GomokuMinimax*.cpp
├── front/               # Inchange — mode 2D SFML
│   ├── GameUI.hpp/cpp
│   └── Input.hpp/cpp
└── front_3d/            # NOUVEAU — mode FPS 3D Raylib
    ├── GameUI3D.hpp/cpp    # Boucle de rendu 3D + HUD
    ├── Input3D.hpp/cpp     # Gestion souris/clavier FPS + raycast
    ├── Board3D.hpp/cpp     # Modele 3D du goban + pierres (spheres)
    └── FPSCamera.hpp/cpp   # Camera FPS fixe (rotation seule)
```

**Flux Principal (Mode 3D)**

```
main.cpp
  │
  ├─ parse args : "--fps" ?
  │    ├─ NON → GameUI::run(state, gomoku)      [mode 2D, inchange]
  │    └─ OUI → GameUI3D::run(state, gomoku)     [mode 3D]
  │
  └─ GameUI3D::run()
       │
       ├── InitWindow() Raylib
       ├── Boucle principale (60 FPS)
       │    ├── Input3D::process()
       │    │    ├── Rotation camera (souris)
       │    │    ├── Raycast vers le goban (clic gauche)
       │    │    │    └── Intersection touchee → state.place_stone(x, y)
       │    │    └── Autres inputs (restart, quit, etc.)
       │    │
       │    ├── Si tour IA → gomoku.getBestMove3() dans un thread
       │    │    └── Resultat → state.place_stone(x, y)
       │    │
       │    ├── BeginDrawing()
       │    │    ├── BeginMode3D(camera)
       │    │    │    ├── Board3D::draw_board()     # Plateau en 3D
       │    │    │    └── Board3D::draw_stones()    # Spheres sur intersections
       │    │    ├── EndMode3D()
       │    │    ├── HUD 2D overlay
       │    │    │    ├── Crosshair
       │    │    │    ├── Chronometre IA
       │    │    │    ├── Score captures
       │    │    │    ├── Tour actuel
       │    │    │    └── Historique coups
       │    │    └── EndDrawing()
       │    │
       │    └── Verif game_over → ecran de fin
       │
       └── CloseWindow()
```

**Rendu 3D — Details**

* **Goban** : Plan horizontal ou mur vertical (a tester). Grille 19x19 tracee avec `DrawLine3D()`. Materiau bois (texture ou couleur unie).
* **Pierres** : `DrawSphere()` posees sur les intersections. Noires et blanches, avec eclairage basique.
* **Arme** : Modele simple (cube/cylindre) ancre a la camera en bas a droite (vue FPS classique).
* **Raycast** : `GetMouseRay()` Raylib + intersection avec le plan du goban. Snap sur l'intersection la plus proche.
* **Eclairage** : Lumiere directionnelle simple pour donner du volume aux pierres.

**Threading**
* Le calcul IA tourne dans un thread separe (comme en 2D).
* Le rendu Raylib reste sur le thread principal.
* Communication via `GameState` (atomique ou mutex si necessaire).
