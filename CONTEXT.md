# CONTEXT — Projet Gomoku (42 School)

> Pré-prompt à lire en début de chaque session de travail.
> Mis à jour : 2026-04-02

---

## 1. Identité du projet

**Sujet** : Créer une IA capable de battre des joueurs humains au Gomoku (42 School, v3.3).
**Exécutable** : `Gomoku`
**Build** : `make` / `make all` — Makefile avec règles `$(NAME)`, `all`, `clean`, `fclean`, `re`. Pas de relink.

---

## 2. Règles du jeu (rappel sujet)

- **Plateau** : Goban 19×19, pierres illimitées.
- **Victoire par alignement** : 5 pierres ou plus en ligne (H/V/D).
- **Victoire par capture** : Capturer 10 pierres adverses (5 paires).
- **Capture** : Encadrer une paire adverse (exactement 2 pierres, pas plus). Les intersections libérées redeviennent jouables. Interdit de se "suicider" dans une capture.
- **Endgame capture** : Un alignement de 5 ne gagne que s'il ne peut pas être brisé par une capture immédiate. Si un joueur a perdu 4 paires et que l'adversaire peut en capturer une 5ème → défaite par capture.
- **Double-trois interdit** : Impossible de jouer un coup créant deux alignements "three-free" simultanés. **Exception** : autorisé si généré par une capture.

---

## 3. Stack technique

| Composant       | Technologie         | Rôle                                      |
|-----------------|---------------------|-------------------------------------------|
| Langage         | C++                 | Tout le projet                            |
| Moteur IA       | C++ standard        | Minimax alpha-beta, agnostique du rendu   |
| Front 2D        | SFML                | Vue principale, partie obligatoire        |
| Front 3D        | Raylib              | Mode FPS bonus (`./Gomoku --fps`)         |

---

## 4. Architecture du code

```
src/
├── main.cpp              # Point d'entrée, branchement 2D / 3D selon args
├── engine/               # INTOUCHABLE — moteur partagé
│   ├── Gomoku.hpp/cpp    # Orchestrateur IA (getBestMove)
│   ├── GameState.hpp     # État du plateau
│   ├── Bitboard.hpp      # Représentation bitboard
│   ├── Rules.hpp/cpp     # Validation des règles
│   └── GomokuMinimax*.cpp # Algorithme minimax alpha-beta
├── front/                # INTOUCHABLE — vue 2D SFML
│   ├── GameUI.hpp/cpp    # Boucle de rendu 2D + HUD
│   └── Input.hpp/cpp     # Gestion clavier/souris 2D
└── front_3d/             # Zone de travail — vue 3D Raylib
    ├── front3d_types.hpp # Types partagés (UI3DState, CaptureAnim3D)
    │                     #   — N'inclut ni engine ni raylib, ordre-libre
    ├── front3d_compat.hpp# Helpers Player/Cell + boardToWorld()
    │                     #   — Résout le conflit macro Raylib BLACK/WHITE vs enum Cell
    ├── GameUI3D.hpp/cpp  # Orchestrateur : boucle run(), caméra FPS, input,
    │                     #   thread IA, apply_win_check, pipeline 3 passes
    ├── Board3D.hpp/cpp   # Goban 3D : plateau, grille, hoshi, pierres,
    │                     #   raycast, hover, surbrillance IA, best-move
    ├── HUD3D.hpp/cpp     # HUD overlay 2D : réticule, barre de statut,
    │                     #   historique des coups, captures animées, game-over
    └── Viewmodel3D.hpp/cpp # Arme FPS : balancement idle, recul, bolt-action
```

### Pipeline de rendu 3 passes (render_scene)

| Passe | Contexte | Contenu |
|-------|----------|---------|
| 1 — Scène monde | `BeginMode3D` / `EndMode3D` | Goban, pierres, hover, highlights |
| 2 — Viewmodel   | Espace caméra virtuel séparé | Arme FPS (`draw_viewmodel_3d`) |
| 3 — HUD 2D      | Après `rlDisableDepthTest()` | Réticule, statut, historique, game-over |

### Résolution du conflit de macros Raylib / engine

Raylib définit `BLACK` et `WHITE` comme `Color{}` C-style ; l'engine les utilise
comme valeurs de `enum Cell`. Dans chaque `.cpp` du module front_3d :
1. Inclure `GameState.hpp` en premier.
2. Sauvegarder : `static constexpr Cell kBlack = BLACK;` (× 3).
3. Inclure `raylib.h` (écrase les macros).
4. Inclure `front3d_compat.hpp` (n'utilise que `static_cast<Cell>`, pas les macros).

**Règle absolue** : `src/engine/` et `src/front/` ne sont **jamais modifiés** pour ajouter du 3D.
SFML et Raylib ne coexistent **jamais** dans le même processus.

---

## 5. Périmètre de travail de la session

**Responsabilité** : Frontend uniquement — rendu 2D (`src/front/`) et rendu 3D (`src/front_3d/`).
**Interdit de toucher** : moteur IA (`src/engine/`), algorithme minimax, heuristique.

---

## 6. Points de validation critiques (sujet)

| Critère                        | Seuil                          | Sanction si raté         |
|--------------------------------|--------------------------------|--------------------------|
| Temps de réflexion IA          | < 0.5 seconde en moyenne       | Non-validation du projet |
| Profondeur d'arbre Minimax     | ≥ 10 niveaux                   | Note plafonnée           |
| Chronomètre IA affiché dans UI | Obligatoire                    | Non-validation du projet |
| Stabilité (pas de crash)       | 0 crash, 0 arrêt inattendu     | Note = 0                 |
| Bonus 3D                       | Évalué seulement si 2D parfait | Ignoré sinon             |

---

## 7. Modes de jeu

- **IA vs Humain** : l'IA joue automatiquement après le coup du joueur.
- **Humain vs Humain (hotseat)** : avec suggestion de coup IA (case mise en surbrillance).
- **Mode FPS 3D** (`--fps`) : caméra FPS fixe, visée souris, tir = placement de pierre par raycast.

---

## 8. TODO List actuelle

TODO 1 : dans le solo vs IA du front 2D, il est affiché le temps total que l'ia prend pour réfléchir, mais il faudrait afficher le temps moyen par coup. remplace le .

---

## 9. Décisions d'architecture enregistrées (ADR)

| ADR | Décision |
|-----|----------|
| ADR-001 | Langage : C++ |
| ADR-002 | Vue 2D : SFML |
| ADR-003 | Pattern MVC strict — noyau agnostique du rendu |
| ADR-004 | Optimisation IA : élagage Alpha-Beta |
| ADR-005 | Bonus 3D : Raylib, activé via `--fps`, bloqué si 2D non parfait |
| ADR-3D-001 | Bibliothèque 3D : Raylib |
| ADR-3D-002 | Isolation SFML/Raylib : branchement dans `main.cpp`, jamais simultanés |
| ADR-3D-003 | Pas d'interface abstraite — duplication du contrat, pas du code |
| ADR-3D-004 | Caméra FPS fixe — rotation seule, position lockée |
| ADR-3D-005 | Placement des pierres : `GetMouseRay` + intersection avec plan du goban |
| ADR-3D-006 | Dossier dédié `src/front_3d/`, aucun fichier 2D touché |
| ADR-3D-007 | Thread IA séparé, même pattern que le 2D |
| ADR-3D-008 | HUD en overlay 2D via `DrawText` Raylib après `EndMode3D()` |
| ADR-3D-009 | front_3d éclaté en 4 sous-modules (Board3D, HUD3D, Viewmodel3D, GameUI3D) + 2 headers partagés (front3d_types.hpp, front3d_compat.hpp) |
| ADR-3D-010 | Conflit macro Raylib/engine résolu par sauvegarde kBlack/kWhite/kEmpty avant inclusion raylib.h ; helpers dans front3d_compat.hpp |
| ADR-3D-011 | Viewmodel rendu dans un espace caméra virtuel séparé (indépendant de la scène monde) pour garantir l'arme toujours visible |

---

## 10. Contexte soutenance

La soutenance porte principalement sur :
- Explication **détaillée** de l'implémentation Minimax (alpha-beta, profondeur, heuristique).
- Démonstration du **chronomètre IA** dans l'UI.
- Validation des règles du sujet (captures, double-trois, endgame).

Le code frontend (2D et 3D) doit être **propre, documenté et robuste** pour la démonstration live.

---

## 11. Consignes pour Claude dans cette session

1. **Ne jamais modifier** `src/engine/` ni l'algorithme IA.
2. Travailler exclusivement sur `src/front/` (2D) et `src/front_3d/` (3D).
3. Priorité : d'abord finir et solidifier le rendu 2D, ensuite le 3D.
4. Tout changement doit être non-régressif : le mode 2D reste 100% fonctionnel.
5. Réponses directes, sans sycophancy. Si quelque chose est cassé ou mal conçu, le dire clairement.
6. Favoriser la lisibilité du code (soutenance oblige) sans sur-ingénierie.
