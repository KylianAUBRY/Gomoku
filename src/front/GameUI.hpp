#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameUI.hpp — Vue 2D (SFML)
//
// Rôle : fenêtre, rendu et boucle principale du mode 2D.
// Principe MVC : GameUI = Vue + Contrôleur léger.
//   - Elle NE MODIFIE PAS directement l'état du jeu (GameState).
//   - Elle délègue toute la logique input à Input::process_events().
//   - Elle lit GameState en lecture seule pour le rendu.
// ─────────────────────────────────────────────────────────────────────────────

#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include <SFML/Graphics.hpp>

// ─── Machine à états de l'UI ─────────────────────────────────────────────────
// Détermine quelle vue est active et quel comportement adopter.
// PLAYING_BOT et PLAYING_BENCHMARK ne réagissent pas aux clics humains.
enum class UIState { MAIN_MENU, PLAYING_SOLO, PLAYING_MULTI, PLAYING_BOT, PLAYING_BENCHMARK };

// ─── Constantes de mise en page ───────────────────────────────────────────────
// Toutes en pixels. Définies ici pour être partagées avec Input.cpp
// (qui calcule grid_x/grid_y à partir de MARGIN et CELL_SIZE).
//
// Formule pixel → grille (Input.cpp) :
//   grid_x = (mx - MARGIN + CELL_SIZE / 2) / CELL_SIZE
// Le "+ CELL_SIZE / 2" est un arrondi implicite vers le point de grille le plus proche.
static constexpr float CELL_SIZE     = 35.0f;   // Distance entre deux intersections (px)
static constexpr int   BOARD_SIZE    = 19;       // 19×19 = standard Goban
static constexpr float MARGIN        = 45.0f;   // Espace autour de la grille pour les coordonnées
static constexpr float HISTORY_WIDTH = 250.0f;  // Largeur du panneau historique (droite)

class GameUI {
public:
  GameUI();

  // Point d'entrée de la boucle principale SFML.
  // Prend state et gomoku par référence : ils sont partagés avec le moteur IA.
  void run(GameState &state, Gomoku &gomoku);

  // Accesseurs pour Input.cpp — évite de dupliquer l'état entre les deux modules.
  sf::RenderWindow &getWindow()       { return window; }
  UIState          &getCurrentState() { return current_state; }
  int              &getMenuSelection(){ return menu_selection; }
  bool             &getSuggestionShown(){ return suggestion_shown; }

private:
  sf::RenderWindow window;    // Fenêtre SFML (contexte OpenGL 2D)
  sf::Font         font;      // Police chargée depuis assets/font.ttf
  UIState          current_state;
  int              menu_selection; // Index 0-3 pour naviguer dans le menu
  bool             suggestion_shown; // true = afficher pierre fantôme du meilleur coup

  // ── Animation de capture ──────────────────────────────────────────────────
  // Quand une paire est capturée, on ajoute une CaptureAnim à la liste.
  // Chaque frame, timer -= dt. Quand timer <= 0, on retire l'anim.
  // Le alpha de la croix X est proportionnel au timer (fade-out naturel).
  struct CaptureAnim { int row, col; float timer; };
  std::vector<CaptureAnim> capture_anims;

  // sf::Clock : compteur de temps SFML. restart() renvoie le temps écoulé
  // depuis le dernier restart() ET remet l'horloge à zéro.
  // Utilisé pour calculer dt (delta time) à chaque frame.
  sf::Clock frame_clock;

  // ── Surbrillance du dernier coup IA ──────────────────────────────────────
  // Après chaque coup IA, on stocke sa position et on démarre un timer 0.5s.
  // draw_ai_highlight() dessine un anneau rouge dont l'opacité = timer / 0.5f.
  int   ai_highlight_row;
  int   ai_highlight_col;
  float ai_highlight_timer; // Décroît de 0.5 → 0.0

  // ── Méthodes de rendu ────────────────────────────────────────────────────
  // Appelées dans l'ordre par render() — important : SFML utilise le
  // painter's algorithm (pas de Z-buffer), donc l'ordre de draw() compte.
  void render_menu();
  void render(const GameState &state);

  void draw_board();                                            // Grille + coordonnées
  void draw_stones(const GameState &state);                    // Pierres noires/blanches
  void draw_hud(const GameState &state);                       // Barre bas : timer, tour, captures
  void draw_game_over(const GameState &state, UIState ui_state); // Overlay victoire
  void draw_history(const GameState &state);                   // Panneau droit : historique + suggestion
  void draw_best_move(const GameState &state);                 // Pierre fantôme semi-transparente
  void draw_ai_highlight();                                    // Anneau rouge fadeout dernier coup IA
  void update_capture_anims(float dt);                         // Décrémente les timers
  void draw_capture_anims();                                   // Dessine les croix X
};
