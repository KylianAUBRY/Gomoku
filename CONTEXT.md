# CONTEXT — Projet Gomoku (42 School)

> Pré-prompt à lire en début de chaque session de travail.
> Mis à jour : 2026-03-31

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
│   ├── Gomoku.hpp/cpp    # Orchestrateur IA (getBestMove3)
│   ├── GameState.hpp     # État du plateau
│   ├── Bitboard.hpp      # Représentation bitboard
│   ├── Rules.hpp/cpp     # Validation des règles
│   └── GomokuMinimax*.cpp # Algorithme minimax alpha-beta
├── front/                # INTOUCHABLE — vue 2D SFML
│   ├── GameUI.hpp/cpp    # Boucle de rendu 2D + HUD
│   └── Input.hpp/cpp     # Gestion clavier/souris 2D
└── front_3d/             # Zone de travail — vue 3D Raylib
    └── GameUI3D.hpp/cpp  # Boucle de rendu 3D + HUD overlay
```

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

### 2D + commun

| # | Description | Statut |
|---|-------------|--------|
| TODO 1 | Fix la hitbox de l'écran titre, de selction des modes  (front 2D). Si la souris n'estp as directement sur un des texte de selection, cliquer dans le vide ne devrait rien faire | À faire |
| TODO 2 | Retirer la suggestion d'aide IA pour le joueur (case en surbrillance → doit disparaître) | À faire |
| TODO 3 | Animation de capture : 2 croix rouges à l'emplacement des pierres capturées, durée 1s, disparition en fondu — **en 2D ET en 3D** | À faire |
| TODO 4 | Contre l'IA : cooldown 0.5s entre le coup du joueur et la réponse de l'IA | À faire |
| TODO 5 | Contre l'IA : le coup joué par l'IA apparaît en surbrillance pendant 0.5s | À faire |
| TODO 6 | Compteur de captures pour les deux joueurs : affiché `0/10` au départ, +2 par capture | À faire |

### 3D uniquement (`src/front_3d/`)

| # | Description | Statut |
|---|-------------|--------|
| TODO 7 | Refactoriser le visuel de l'arme via une structure pour permettre l'affichage de différentes armes | À faire |
| TODO 8 | Deux visuels d'arme sélectionnables : Sniper AWP et Shotgun | À faire |
| TODO 9 | Cartouche qui tombe au sol après un tir | À faire |

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
