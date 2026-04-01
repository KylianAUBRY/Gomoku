# REVIEW — Soutenance Front Gomoku
> Activer avec le mot **review** en début de session Claude.
> Ancré dans le code réel. Mis à jour : 2026-04-01.

---

## 0. Checklist avant de montrer le projet

- [ ] `make re` compile sans warning
- [ ] `./Gomoku` → mode 2D démarre
- [ ] `./Gomoku --fps` → mode 3D démarre
- [ ] Timer IA visible en cyan dans le HUD (Solo)
- [ ] Chrono < 500 ms en moyenne
- [ ] Capture de pair bien retirée du plateau
- [ ] Double-trois bloqué si tentative

---

## 1. Architecture globale

```
main.cpp  ──── initEvalTable()  ← heuristique précalculée ici
           ├── --fps ? GameUI3D : GameUI
           └── ui.run(state, game)   ← state et game partagés, jamais copiés
```

**Isolation SFML / Raylib** : `main.cpp` branche sur l'argument `--fps`.
Les deux libs ne coexistent **jamais** dans le même processus.
SFML ni Raylib ne sont inclus dans `src/engine/`.

**Pattern MVC** :
- Modèle : `GameState` + `BitBoard` (engine, non modifiable)
- Vue : `GameUI` (SFML) ou `GameUI3D` (Raylib)
- Contrôleur : `Input` (namespace, 2D) ou méthodes handle_* (3D)

---

## 2. Front 2D — SFML (`src/front/`)

### 2.1 Boucle principale (`GameUI::run`)

```
while (window.isOpen()) {
    snapshot du board avant              ← pour détecter captures et coup IA
    Input::process_events(...)           ← tout l'input et la logique
    détecter capture → capture_anims
    détecter coup IA → ai_highlight
    dt = frame_clock.restart()
    update_capture_anims(dt)
    render_menu() ou render(state)
}
```

### 2.2 Rendu (`GameUI::render`)

Ordre de dessin (important : painter's algorithm, SFML ne gère pas la profondeur) :

1. `window.clear(220, 179, 92)` — couleur bois goban
2. `draw_board()` — grilles + coordonnées
3. `draw_stones()` — `sf::CircleShape`, rayon = `CELL_SIZE * 0.4f`
4. `draw_ai_highlight()` — anneau rouge fadeout 0.5s
5. `draw_capture_anims()` — croix X en rouge alpha décroissant
6. `draw_best_move()` — pierre fantôme semi-transparente
7. `draw_hud()` — barre sombre en bas : mode, timer IA, tour, captures
8. `draw_history()` — panneau droit 250px
9. `draw_game_over()` — overlay noir si `state.game_over`

### 2.3 Coordonnées pixel → grille

```cpp
// Input.cpp, handle_game_input
int grid_x = (mx - MARGIN + CELL_SIZE / 2) / CELL_SIZE;
int grid_y = (my - MARGIN + CELL_SIZE / 2) / CELL_SIZE;
```
`MARGIN = 45.0f`, `CELL_SIZE = 35.0f`
Le `+ CELL_SIZE / 2` arrondit au point de grille le plus proche.

### 2.4 Timer IA (`draw_hud`)

```cpp
// GameUI.cpp:254
snprintf(buffer, sizeof(buffer), "AI Time: %.2f ms", state.last_ai_move_time_ms);
```
`state.last_ai_move_time_ms` est écrit dans `Input.cpp` après chaque coup IA :
```cpp
state.last_ai_move_time_ms = (double)ai_move.computeTimeMs;
```
`computeTimeMs` vient du moteur (champ de `Move`).

### 2.5 Cooldown IA 500 ms

```cpp
// Input.cpp: après placement humain
s_ai_pending = true;
s_ai_pending_since = std::chrono::high_resolution_clock::now();

// Dans process_events, chaque frame :
if (elapsed_ms >= 500.0) {
    s_ai_pending = false;
    Move ai_move = gomoku.getBestMove(state.board, ai_cell);
    state.last_ai_move_time_ms = (double)ai_move.computeTimeMs;
    state.place_stone(ai_move.col, ai_move.row);
    apply_win_check(state);
}
```
Pourquoi 500 ms ? Délai visuel intentionnel pour que le joueur voie son coup avant que l'IA réponde.

### 2.6 Règle endgame capture (`apply_win_check`, Input.cpp)

**Principe** (sujet p.3) : un alignement de 5 ne gagne que si l'adversaire ne peut pas immédiatement briser cette ligne par capture.

**Implémentation** :
```
Joueur aligne 5
  └─ can_opponent_capture_from_alignment() ?
      ├─ NON → game_over immédiat
      └─ OUI → s_pending_alignment_win = true
                  L'adversaire joue
                  └─ La ligne est-elle encore intacte ?
                      ├─ OUI → pending_winner gagne
                      └─ NON → la ligne a été brisée, jeu continue
```

`can_opponent_capture_from_alignment` scanne toutes les cases vides et vérifie le pattern `[vide]-[gagnant]-[gagnant]-[adversaire]` dans les 8 directions, où au moins une des deux pierres gagnantes appartient à l'alignement.

**Capture win** (10 pierres) : toujours immédiate, pas de règle endgame.

### 2.7 Suggestion de coup

Bouton dans le panneau historique (y=50, h=35, PLAYING_MULTI ou tour humain en SOLO).
Click → `update_best_move_suggestion()` → `gomoku.getBestMove()` → pierre fantôme dessinée.

### 2.8 Animations captures

```cpp
// Snapshot avant input
BitBoard board_snap = state.board;
// Après input, diff :
if (board_snap.get(r,c) != EMPTY && state.board.get(r,c) == EMPTY)
    capture_anims.push_back({r, c, 1.0f}); // timer = 1.0s
```
Rendu : deux barres rotées ±45° (croix X), `alpha = timer * 255`.

### 2.9 Modes de jeu (2D)

| UIState | Description |
|---------|-------------|
| `MAIN_MENU` | Menu clavier/souris |
| `PLAYING_SOLO` | Humain (Noir) vs IA (Blanc) |
| `PLAYING_MULTI` | Hotseat 1v1 avec suggestion |
| `PLAYING_BOT` | IA getBestMove vs IA getBestMove2 |
| `PLAYING_BENCHMARK` | 4 parties croisées, stats terminal |

---

## 3. Front 3D — Raylib (`src/front_3d/`)

### 3.1 Conflit de macros (critique)

Raylib définit `BLACK` et `WHITE` comme des couleurs `Color{0,0,0,255}`.
L'engine définit `BLACK` et `WHITE` comme des valeurs de `Cell` (enum).

**Solution** dans `GameUI3D.cpp` :
```cpp
// Engine avant raylib
#include "../engine/GameState.hpp"
// Sauvegarder avant pollution
static constexpr Cell kBlack = BLACK;
static constexpr Cell kWhite = WHITE;
static constexpr Cell kEmpty = EMPTY;
// Maintenant safe
#include "raylib.h"
```
Tout le code 3D utilise `kBlack`/`kWhite`/`kEmpty` au lieu de `BLACK`/`WHITE`.
Même problème pour `Player::BLACK` → helper `isBlackPlayer(Player p)`.

### 3.2 Caméra FPS fixe

**Position** : `{9.0f, 9.0f, 22.0f}` (centre du goban 18x18, hauteur 22 unités).
**Principe** : la position ne bouge jamais, seul le `target` change.

```cpp
// update_camera_rotation() — chaque frame
Vector3 forward;
forward.x = sinf(cam_yaw) * cosf(cam_pitch);
forward.y = sinf(cam_pitch);
forward.z = -cosf(cam_yaw) * cosf(cam_pitch);
camera.target = camera.position + forward;
```

**Limites** (pour garder le goban visible) :
- Yaw (horizontal) : ±0.75 rad (~43°)
- Pitch (vertical) : ±0.60 rad (~34°)

Sensibilité souris : `MOUSE_SENSITIVITY = 0.003f` rad/pixel.

### 3.3 Raycast centre écran → intersection goban

Le goban est dans le plan Z=0 du monde.

```cpp
Ray ray = GetScreenToWorldRay(
    {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f}, camera);

// Intersect Z=0 plane: ray.origin.z + t * ray.direction.z = 0
float t = -ray.position.z / ray.direction.z;
float hit_x = ray.position.x + t * ray.direction.x;
float hit_y = ray.position.y + t * ray.direction.y;

int col = (int)roundf(hit_x);
int row = (int)roundf(18.0f - hit_y);   // Y inversé (0 = bas en world)
```

`GetScreenToWorldRay` est une fonction Raylib qui déprojecte un pixel en rayon 3D via la matrice de projection/view de la caméra.

### 3.4 Système de coordonnées 3D

```cpp
static inline Vector3 boardToWorld(int row, int col, float z_offset = 0.0f) {
    return {(float)col, 18.0f - (float)row, z_offset};
}
```
- `col` → axe X (0 à 18)
- `row` → axe Y inversé (row=0 → Y=18, row=18 → Y=0)
- Pierres : Z légèrement au-dessus du plan (`STONE_RADIUS * 0.6f`)

### 3.5 Render pipeline 3D

```
BeginDrawing()
  BeginMode3D(camera)          ← world scene
    draw_board_3d()
    draw_stones_3d()
    draw_ai_highlight_3d()
    draw_hover_indicator()
    draw_best_move_3d()
  EndMode3D()

  draw_viewmodel_3d()          ← pass séparée, depth buffer vidé

  rlDisableDepthTest()         ← HUD 2D jamais occulté par le Z-buffer
  draw_crosshair()
  draw_hud()
  draw_capture_anims_3d()
  draw_history()
  draw_game_over_overlay()
EndDrawing()
```

Pourquoi deux passes 3D ? Le viewmodel (arme) doit toujours apparaître devant la scène, même si géométriquement derrière un mur. Vider le depth buffer et redessiner en second garantit ça.

### 3.6 Viewmodel (arme AWP)

Rendu dans un espace caméra virtuel distinct (`vm_cam`, position = `{0,0,0}`, FOV = 65°).
- **Bob** : `sinf(vm_bob_time * 1.8f)` X + `sinf(vm_bob_time * 3.6f)` Y (double fréquence)
- **Recoil** : `vm_recoil` decay `dt * 6.0f`, recul arrière + haut
- **Bolt-action** : `bolt_anim_timer_` 0.6s, bloque tout nouveau tir
- **Shell casing** : physique simple (gravité `-4.0f * dt` sur Y), durée 0.7s

### 3.7 Point critique : endgame capture non implémenté en 3D

`check_game_over` (3D) vs `apply_win_check` (2D) :

```cpp
// 3D — simplifié, SANS règle endgame capture
void GameUI3D::check_game_over(GameState &state) {
    if (Rules::check_win_condition(state, BLACK) || ...)
        state.game_over = true;
}

// 2D — avec règle endgame (pending win, une seule chance de briser)
static bool apply_win_check(GameState &state) { ... }
```

**Conséquence** : en 3D, un alignement de 5 gagne immédiatement même si l'adversaire aurait pu capturer. Le 2D est conforme au sujet, le 3D non. À assumer si le correcteur teste le 3D en détail.

---

## 4. Heuristique et evalTable (`main.cpp`)

### 4.1 Localisation inhabituelle

L'heuristique est définie dans `main.cpp`, pas dans `engine/`. Elle expose :
- `evalTable[262144][2]` — table précalculée : `[score, flags]` pour chaque combinaison de 9 cellules
- `patterns_white[]`, `patterns_black[]` — patterns scorés
- `patterns_free_three_white/black[]` — patterns pour la règle double-trois
- `initEvalTable()` — remplit la table au démarrage (une seule fois)

### 4.2 Encodage base-4

Chaque cellule = 2 bits : `0=vide, 1=blanc, 2=noir, 3=blocage`
9 cellules → 18 bits → max 4^9 = 262144 valeurs → d'où la taille de `evalTable`.

```cpp
int encode4(const int *cells, int length) {
    int code = 0;
    for (int i = length - 1; i >= 0; i--)
        code = code * 4 + cells[i];
    return code;
}
```

### 4.3 Scores des patterns (blanc, positif = bon pour blanc)

| Pattern | Score | Signification |
|---------|-------|---------------|
| `11111` | +2 000 000 | 5 en ligne blanc = victoire |
| `011110` | +100 000 | 4 ouvert |
| `101110` | +50 000 | 4 semi-ouvert avec trou |
| `01110` | +5 000 | 3 ouvert |
| `0110` | +300 | 2 ouvert |
| Symétrique en noir | valeurs négatives | — |

Captures détectées inline dans `initEvalTable` (pattern `[moi]-[adv]-[adv]-[moi]`), score ±30 000 + flag.

### 4.4 Flags evalTable[i][1]

Bits encodés par macros (définis dans engine) :
- `ADD_WHITE_CAPTURES_DOWN/UP` — blanc peut capturer vers le bas/haut
- `ADD_BLACK_CAPTURES_DOWN/UP`
- `ADD_WHITE_THREES` / `ADD_BLACK_THREES` — free-three détecté

Ces flags permettent au minimax de vérifier rapidement double-trois et potentiel de capture sans re-scanner.

---

## 5. Questions probables soutenance + réponses

**Q : Pourquoi SFML pour le 2D et Raylib pour le 3D ?**
→ SFML est mature, bien documentée, idéale pour du 2D immédiat (sprites, formes, textes). Raylib fournie une API 3D simple avec gestion caméra, raycasting et viewmodel intégrés. Elles ne peuvent pas coexister (deux boucles de contexte OpenGL), donc on branche dans `main.cpp`.

**Q : Explique le timer IA.**
→ En Solo 2D : `state.last_ai_move_time_ms = (double)ai_move.computeTimeMs;` après chaque coup IA, affiché en cyan dans le HUD. En 3D : mesuré localement avec `chrono::high_resolution_clock` dans `handle_ai_turn`. Le sujet l'exige explicitement sous peine de non-validation.

**Q : Comment fonctionne le placement en 3D ?**
→ Chaque frame, `GetScreenToWorldRay` déproject le centre de l'écran en rayon 3D. On intersecte ce rayon avec le plan Z=0 (le goban). Le point d'intersection est arrondi à l'entier le plus proche → (col, row). Clic gauche → `Rules::is_valid_move` → `state.place_stone`.

**Q : Comment est détecté le double-trois ?**
→ `Rules::is_valid_move` → `Rules::check_double_three()`. La détection utilise les flags `ADD_WHITE/BLACK_THREES` de `evalTable`, précalculés au démarrage. Exception : autorisé si le coup produit une capture.

**Q : Pourquoi le viewmodel a une caméra séparée ?**
→ Si le viewmodel était rendu avec la caméra du monde, sa position dépendrait de la scène et il pourrait être partiellement occulté par le goban. La caméra `vm_cam` (origin=0,0,0) place l'arme toujours au même endroit relatif à l'œil, et on vide le depth buffer entre les deux passes pour garantir qu'il s'affiche devant tout.

**Q : Pourquoi les coordonnées Y sont inversées en 3D ?**
→ En Raylib, Y monte (convention mathématique). Dans la représentation interne du goban, row=0 est en haut. `boardToWorld` fait `Y = 18.0f - row` pour aligner les deux conventions.

**Q : Le 3D implémente-t-il toutes les règles ?**
→ Non complètement. La règle endgame capture (pending alignment win) est implémentée en 2D dans `apply_win_check` mais simplifiée en 3D dans `check_game_over`. Le 3D est un bonus, évalué uniquement si le 2D est parfait.

**Q : Comment fonctionne le benchmark ?**
→ 4 parties : parties 1-2 `getBestMove=Noir / getBestMove2=Blanc`, parties 3-4 inversées. Chaque coup mesure `computeTimeMs`, moyenné progressivement (online average). En fin de 4 parties, résumé imprimé dans le terminal.

**Q : Comment gères-tu le `can_opponent_capture_from_alignment` ?**
→ On marque toutes les cellules appartenant à un run ≥5 du gagnant. Ensuite pour chaque case vide du plateau, on vérifie dans 8 directions si on peut former `[vide]-[gagnant]-[gagnant]-[adversaire]` avec au moins une des deux pierres gagnantes dans l'alignement. Si oui → pending, l'adversaire a une chance.

---

## 6. Constantes clés à connaître par cœur

| Constante | Valeur | Fichier |
|-----------|--------|---------|
| `CELL_SIZE` | 35.0f px | GameUI.hpp |
| `BOARD_SIZE` | 19 | GameUI.hpp |
| `MARGIN` | 45.0f px | GameUI.hpp |
| `HISTORY_WIDTH` | 250.0f px | GameUI.hpp |
| `BOARD_WORLD_SIZE` | 18.0f | GameUI3D.hpp |
| `STONE_RADIUS` | 0.35f | GameUI3D.hpp |
| `CAM_YAW_LIMIT` | 0.75f rad | GameUI3D.hpp |
| `CAM_PITCH_LIMIT` | 0.60f rad | GameUI3D.hpp |
| `MOUSE_SENSITIVITY` | 0.003f | GameUI3D.hpp |
| `BOLT_ANIM_DURATION` | 0.60f s | GameUI3D.hpp |
| AI cooldown | 500 ms | Input.cpp / GameUI3D.cpp |
| AI highlight | 0.5s | GameUI.hpp / GameUI3D.hpp |
| evalTable size | 262144 | main.cpp |

---

## 7. Gaps et points faibles à connaître

1. **Endgame capture absent en 3D** : `check_game_over` ne fait pas le pending-win. À assumer.
2. **Lettres du goban** : `'I'` n'est **pas** sautée (le skip est commenté). Colonnes A–S avec I inclus. Git log : "Fixed Board letters Issues" = commit récent.
3. **Heuristique dans main.cpp** : architecturalement discutable (devrait être dans engine), mais fonctionnel.
4. **Pas de Bot vs Bot en 3D** : le menu 3D propose uniquement Solo et Multi.
5. **Suggestion de coup en 3D** : toujours active en Multi (pas de bouton toggle comme en 2D, appelée après chaque coup humain).
