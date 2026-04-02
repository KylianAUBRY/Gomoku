// ─────────────────────────────────────────────────────────────────────────────
// GameUI.cpp — Implémentation de la vue 2D (SFML)
// ─────────────────────────────────────────────────────────────────────────────

#include "GameUI.hpp"
#include "../engine/Rules.hpp"
#include "Input.hpp"
#include <algorithm>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// Constructeur : initialise la fenêtre SFML et les membres.
//
// Taille de la fenêtre calculée depuis les constantes de layout :
//   Largeur  = (BOARD_SIZE-1) * CELL_SIZE + 2*MARGIN + HISTORY_WIDTH
//            = 18*35 + 90 + 250 = 970 px
//   Hauteur  = (BOARD_SIZE-1) * CELL_SIZE + 2*MARGIN + 100 (barre HUD)
//            = 18*35 + 90 + 100 = 820 px
//
// sf::Style::Titlebar | sf::Style::Close : fenêtre non redimensionnable
// (pas de sf::Style::Resize), ce qui simplifie le calcul de layout fixe.
// ─────────────────────────────────────────────────────────────────────────────
GameUI::GameUI()
    : window(sf::VideoMode(
                 {(unsigned int)((BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN +
                                 HISTORY_WIDTH),
                  (unsigned int)((BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN +
                                 100)}),
             "Gomoku - Local PvP",
             sf::Style::Titlebar | sf::Style::Close) {
  window.setFramerateLimit(60); // Limite à 60 fps : évite 100% CPU, réduit tearing
  current_state = UIState::MAIN_MENU;
  menu_selection = 0;
  suggestion_shown = false;
  ai_highlight_row = -1;
  ai_highlight_col = -1;
  ai_highlight_timer = 0.0f;

  // sf::Font::openFromFile() charge la police en mémoire.
  // On utilise une seule instance partagée pour tous les textes (perf).
  if (!font.openFromFile("assets/font.ttf")) {
    std::cerr << "Failed to load font from assets/font.ttf" << std::endl;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Boucle principale SFML
//
// Séquence par frame :
//   1. Snapshot du plateau AVANT l'input → permet de détecter les changements
//   2. Input::process_events() → modifie state (pierres, captures, victoire)
//   3. Diff avant/après → déclenchement des animations
//   4. Mise à jour des timers (dt = temps réel entre frames)
//   5. Rendu
//
// Pourquoi le snapshot AVANT l'input ?
//   Input peut poser une pierre ET déclencher une capture dans la même frame.
//   Le diff board_snap vs state.board révèle exactement quelles cases sont
//   passées de non-vide à vide (= pierres capturées).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::run(GameState &state, Gomoku &gomoku) {
  while (window.isOpen()) {

    // Snapshot avant input
    uint8_t cap_b_before = state.board.blackCaptures;
    uint8_t cap_w_before = state.board.whiteCaptures;
    BitBoard board_snap = state.board;

    // Tout l'input (humain, IA, bot-vs-bot, benchmark) est délégué ici
    Input::process_events(window, current_state, menu_selection, state, gomoku, suggestion_shown);

    // Détection des captures : case non-vide avant → vide après = capturée
    if (state.board.blackCaptures != cap_b_before || state.board.whiteCaptures != cap_w_before) {
      for (int r = 0; r < 19; r++)
        for (int c = 0; c < 19; c++)
          if (board_snap.get(r, c) != EMPTY && state.board.get(r, c) == EMPTY)
            capture_anims.push_back({r, c, 1.0f}); // timer = 1.0s
    }

    // Détection du dernier coup IA : case vide avant → WHITE après en mode Solo
    if (current_state == UIState::PLAYING_SOLO) {
      for (int r = 0; r < 19; r++)
        for (int c = 0; c < 19; c++)
          if (board_snap.get(r, c) == EMPTY && state.board.get(r, c) == WHITE) {
            ai_highlight_row   = r;
            ai_highlight_col   = c;
            ai_highlight_timer = 0.5f; // durée de la surbrillance
          }
    }

    // dt = delta time en secondes depuis la dernière frame
    // frame_clock.restart() renvoie le temps ET remet l'horloge à zéro
    float dt = frame_clock.restart().asSeconds();
    update_capture_anims(dt);
    if (ai_highlight_timer > 0.0f) ai_highlight_timer -= dt;

    if (current_state == UIState::MAIN_MENU) {
      render_menu();
    } else {
      render(state);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Menu principal
//
// La sélection active est mise en jaune, les autres en blanc.
// menu_selection est modifié par Input via flèches ou survol souris.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::render_menu() {
  window.clear(sf::Color(40, 40, 40));

  sf::Text title(font, "GOMOKU", 60);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Bold);
  // Centrage horizontal : position = centre_fenêtre - demi_largeur_texte
  title.setPosition(
      {window.getSize().x / 2.0f - title.getGlobalBounds().size.x / 2.0f,
       100.0f});

  sf::Text solo(font, "Mode Solo (vs IA)", 40);
  solo.setFillColor(menu_selection == 0 ? sf::Color::Yellow : sf::Color::White);
  sf::FloatRect solo_bounds = solo.getGlobalBounds();
  solo.setPosition(
      {window.getSize().x / 2.0f - solo_bounds.size.x / 2.0f, 280.0f});

  sf::Text multi(font, "Mode Multi (Local 1v1)", 40);
  multi.setFillColor(menu_selection == 1 ? sf::Color::Yellow : sf::Color::White);
  sf::FloatRect multi_bounds = multi.getGlobalBounds();
  multi.setPosition(
      {window.getSize().x / 2.0f - multi_bounds.size.x / 2.0f, 370.0f});

  sf::Text bot(font, "Bot vs Bot", 40);
  bot.setFillColor(menu_selection == 2 ? sf::Color::Yellow : sf::Color::White);
  sf::FloatRect bot_bounds = bot.getGlobalBounds();
  bot.setPosition(
      {window.getSize().x / 2.0f - bot_bounds.size.x / 2.0f, 460.0f});

  sf::Text bench(font, "Benchmark (4 parties)", 40);
  bench.setFillColor(menu_selection == 3 ? sf::Color::Yellow : sf::Color::White);
  sf::FloatRect bench_bounds = bench.getGlobalBounds();
  bench.setPosition(
      {window.getSize().x / 2.0f - bench_bounds.size.x / 2.0f, 550.0f});

  window.draw(title);
  window.draw(solo);
  window.draw(multi);
  window.draw(bot);
  window.draw(bench);

  window.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendu du plateau (mode jeu)
//
// SFML = painter's algorithm : pas de Z-buffer, l'ordre des draw() détermine
// qui est "au-dessus". Ordre : fond → grille → pierres → anims → HUD → overlay.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::render(const GameState &state) {
  window.clear(sf::Color(220, 179, 92)); // Couleur bois goban

  draw_board();
  draw_stones(state);
  draw_ai_highlight();      // Anneau rouge sur le dernier coup IA (au-dessus des pierres)
  draw_capture_anims();     // Croix X aux endroits des captures récentes

  if (current_state == UIState::PLAYING_SOLO  ||
      current_state == UIState::PLAYING_MULTI ||
      current_state == UIState::PLAYING_BOT   ||
      current_state == UIState::PLAYING_BENCHMARK) {
    draw_best_move(state);  // Pierre fantôme du meilleur coup suggéré
    draw_hud(state);        // Barre inférieure : timer IA, tour, captures
    draw_history(state);    // Panneau droit : liste des coups + bouton suggestion

    if (state.game_over) {
      draw_game_over(state, current_state); // Overlay victoire par-dessus tout
    }
  }

  // Double-buffering SFML : display() swap les buffers front/back
  window.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_board : grille + coordonnées A-S / 1-19
//
// Chaque ligne est un sf::RectangleShape de 2px d'épaisseur.
// Position d'une intersection (col, row) en pixels :
//   x_px = MARGIN + col * CELL_SIZE
//   y_px = MARGIN + row * CELL_SIZE
//
// Coordonnées : colonnes A–S (sans sauter I — le saut est commenté),
//               lignes 1–19 de haut en bas.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_board() {
  sf::RectangleShape line(sf::Vector2f((BOARD_SIZE - 1) * CELL_SIZE, 2));
  line.setFillColor(sf::Color::Black);

  for (int i = 0; i < BOARD_SIZE; ++i) {
    // Ligne horizontale i
    line.setPosition({(float)MARGIN, (float)(MARGIN + i * CELL_SIZE - 1)});
    window.draw(line);

    // Ligne verticale i
    sf::RectangleShape vline(sf::Vector2f(2, (BOARD_SIZE - 1) * CELL_SIZE));
    vline.setFillColor(sf::Color::Black);
    vline.setPosition({(float)(MARGIN + i * CELL_SIZE - 1), (float)MARGIN});
    window.draw(vline);
  }

  // Coordonnées axe X : lettres A-S (I inclus — le skip est commenté)
  // Coordonnées axe Y : chiffres 1-19
  for (int i = 0; i < BOARD_SIZE; ++i) {
    char col_char = 'A' + i;
    // if (col_char >= 'I') col_char++; // Désactivé : I est inclus
    std::string col_str(1, col_char);

    // Lettre en haut de la colonne
    sf::Text col_text_top(font, col_str, 18);
    col_text_top.setFillColor(sf::Color::Black);
    // Centrage sur l'intersection : position -= demi_largeur_texte
    col_text_top.setPosition(
        {(float)(MARGIN + i * CELL_SIZE -
                 col_text_top.getGlobalBounds().size.x / 2.0f),
         (float)(MARGIN - 30.0f)});
    window.draw(col_text_top);

    // Lettre en bas de la colonne
    sf::Text col_text_bot(font, col_str, 18);
    col_text_bot.setFillColor(sf::Color::Black);
    col_text_bot.setPosition(
        {(float)(MARGIN + i * CELL_SIZE -
                 col_text_bot.getGlobalBounds().size.x / 2.0f),
         (float)(MARGIN + (BOARD_SIZE - 1) * CELL_SIZE + 10.0f)});
    window.draw(col_text_bot);

    // Chiffre à gauche de la ligne
    std::string row_str = std::to_string(i + 1);
    sf::Text row_text_left(font, row_str, 18);
    row_text_left.setFillColor(sf::Color::Black);
    row_text_left.setPosition(
        {(float)(MARGIN - 30.0f -
                 row_text_left.getGlobalBounds().size.x / 2.0f),
         (float)(MARGIN + i * CELL_SIZE - 12.0f)});
    window.draw(row_text_left);

    // Chiffre à droite de la ligne
    sf::Text row_text_right(font, row_str, 18);
    row_text_right.setFillColor(sf::Color::Black);
    row_text_right.setPosition(
        {(float)(MARGIN + (BOARD_SIZE - 1) * CELL_SIZE + 15.0f),
         (float)(MARGIN + i * CELL_SIZE - 12.0f)});
    window.draw(row_text_right);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_stones : dessin de toutes les pierres présentes sur le plateau
//
// sf::CircleShape : disque SFML, l'origine est recentrée (setOrigin) pour que
// setPosition() place le centre du disque sur l'intersection, pas son coin haut-gauche.
// Rayon = CELL_SIZE * 0.4f → légèrement plus petit que la demi-cellule (0.5f)
// pour laisser visible la grille entre les pierres.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_stones(const GameState &state) {
  float radius = CELL_SIZE * 0.4f;
  sf::CircleShape stone(radius);
  stone.setOrigin({radius, radius}); // Centrage du disque sur son centre géométrique

  for (int y = 0; y < BOARD_SIZE; ++y) {
    for (int x = 0; x < BOARD_SIZE; ++x) {
      if (state.board.get(y, x) == BLACK) {
        stone.setFillColor(sf::Color::Black);
        // Conversion grille → pixels : x_px = MARGIN + col * CELL_SIZE
        stone.setPosition(
            {(float)(MARGIN + x * CELL_SIZE), (float)(MARGIN + y * CELL_SIZE)});
        window.draw(stone);
      } else if (state.board.get(y, x) == WHITE) {
        stone.setFillColor(sf::Color::White);
        stone.setPosition(
            {(float)(MARGIN + x * CELL_SIZE), (float)(MARGIN + y * CELL_SIZE)});
        window.draw(stone);
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_hud : barre inférieure (fond sombre, 100px de hauteur)
//
// Contient 4 zones :
//   Gauche : mode + timer IA (en cyan — couleur imposée par le sujet)
//   Centre : tour courant + couleur du joueur
//   Droite : compteurs de captures (icône pierre + chiffre)
//
// Timer IA : affiché uniquement en mode Solo (last_ai_move_time_ms).
//   En Bot vs Bot : moyenne mobile pour chaque bot.
//   En Benchmark  : moyenne des 4 parties en cours.
//
// Chronomètre requis par le sujet (p.6) : "No timer, no project validation."
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_hud(const GameState &state) {
  float hud_y      = (BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN;
  float hud_height = 100.0f;

  sf::RectangleShape hud_bg(sf::Vector2f(window.getSize().x, hud_height));
  hud_bg.setFillColor(sf::Color(40, 40, 40));
  hud_bg.setPosition({0, hud_y});
  window.draw(hud_bg);

  // ── Zone gauche : mode ────────────────────────────────────────────────────
  std::string mode_str;
  if (current_state == UIState::PLAYING_SOLO)
    mode_str = "Mode Solo - Esc to Quit";
  else if (current_state == UIState::PLAYING_MULTI)
    mode_str = "Mode Multi - Esc to Quit";
  else if (current_state == UIState::PLAYING_BOT)
    mode_str = "Bot vs Bot - Esc to Quit";
  else
    mode_str = "Benchmark - Esc to Quit";
  sf::Text info(font, mode_str, 20);
  info.setFillColor(sf::Color::White);
  info.setPosition({20.0f, hud_y + 15.0f});
  window.draw(info);

  // ── Timer IA (cyan) ───────────────────────────────────────────────────────
  // state.last_ai_move_time_ms : écrit par Input.cpp après chaque appel getBestMove().
  // Valeur = ai_move.computeTimeMs (champ rempli par le moteur, en ms).
  if (current_state == UIState::PLAYING_SOLO) {
    char buffer[64];
    if (state.white_move_count > 0)
      snprintf(buffer, sizeof(buffer), "AI Time (avg): %.2f ms",
               state.white_avg_time_ms);
    else
      snprintf(buffer, sizeof(buffer), "AI Time (avg): --");
    sf::Text timer_text(font, buffer, 18);
    timer_text.setFillColor(sf::Color::Cyan);
    timer_text.setPosition({20.0f, hud_y + 55.0f});
    window.draw(timer_text);
  } else if (current_state == UIState::PLAYING_BOT) {
    // Moyenne mobile incrémentale : moy_n = moy_{n-1} + (val - moy_{n-1}) / n
    // Calculée dans Input.cpp, affichée ici.
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Moy Bot1(N): %.2f ms  |  Moy Bot2(B): %.2f ms",
             state.black_avg_time_ms, state.white_avg_time_ms);
    sf::Text timer_text(font, buffer, 16);
    timer_text.setFillColor(sf::Color::Cyan);
    timer_text.setPosition({20.0f, hud_y + 55.0f});
    window.draw(timer_text);
  } else if (current_state == UIState::PLAYING_BENCHMARK) {
    char buffer[160];
    int game = state.benchmark_game;
    if (game >= 1 && game <= 4) {
      snprintf(buffer, sizeof(buffer),
               "Partie %d/4  |  Moy getBestMove: %.1f ms  |  Moy getBestMove2: %.1f ms",
               game, state.black_avg_time_ms, state.white_avg_time_ms);
    } else {
      snprintf(buffer, sizeof(buffer), "Benchmark termine — voir le terminal");
    }
    sf::Text timer_text(font, buffer, 15);
    timer_text.setFillColor(sf::Color::Cyan);
    timer_text.setPosition({20.0f, hud_y + 55.0f});
    window.draw(timer_text);
  }

  // ── Centre : tour courant ─────────────────────────────────────────────────
  // Le texte "NOIR" est en noir avec outline blanc pour être lisible sur fond sombre.
  std::string turn_str = "Tour " + std::to_string(state.current_turn) + " : ";
  turn_str += (state.current_player == Player::BLACK) ? "NOIR" : "BLANC";
  sf::Text turn_text(font, turn_str, 24);

  if (state.current_player == Player::BLACK) {
    turn_text.setFillColor(sf::Color::Black);
    turn_text.setOutlineColor(sf::Color::White);
    turn_text.setOutlineThickness(1.0f);
  } else {
    turn_text.setFillColor(sf::Color::White);
  }
  turn_text.setPosition(
      {window.getSize().x / 2.0f - turn_text.getGlobalBounds().size.x / 2.0f,
       hud_y + 35.0f});
  window.draw(turn_text);

  // ── Droite : compteurs de captures ───────────────────────────────────────
  // Format : icône disque noir + ": N/10" | icône disque blanc + ": N/10"
  float cap_x = window.getSize().x - 210.0f;
  float cap_y = hud_y + 50.0f;
  float cap_r = 10.0f;
  sf::CircleShape cap_stone(cap_r);
  cap_stone.setOrigin({cap_r, cap_r});

  cap_stone.setFillColor(sf::Color::Black);
  cap_stone.setOutlineColor(sf::Color::White);
  cap_stone.setOutlineThickness(1.5f);
  cap_stone.setPosition({cap_x, cap_y});
  window.draw(cap_stone);

  sf::Text cap_black(font, ": " + std::to_string(state.board.blackCaptures) + "/10", 20);
  cap_black.setFillColor(sf::Color::White);
  cap_black.setPosition({cap_x + cap_r + 2.0f, cap_y - cap_r - 2.0f});
  window.draw(cap_black);

  float cap_x2 = cap_x + 100.0f;
  cap_stone.setFillColor(sf::Color::White);
  cap_stone.setOutlineColor(sf::Color(180, 180, 180));
  cap_stone.setPosition({cap_x2, cap_y});
  window.draw(cap_stone);

  sf::Text cap_white(font, ": " + std::to_string(state.board.whiteCaptures) + "/10", 20);
  cap_white.setFillColor(sf::Color::White);
  cap_white.setPosition({cap_x2 + cap_r + 2.0f, cap_y - cap_r - 2.0f});
  window.draw(cap_white);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_game_over : overlay victoire
//
// Cas spécial Benchmark : affiche "BENCHMARK TERMINE" sans bouton Rejouer.
// Cas normal : détermine le gagnant via Rules::check_win_condition /
//              check_win_by_capture (double vérification), affiche le nom,
//              propose un bouton "Rejouer".
// L'overlay semi-transparent (alpha=150) laisse voir l'état final du plateau.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_game_over(const GameState &state, UIState ui_state) {
  if (ui_state == UIState::PLAYING_BENCHMARK && state.benchmark_game == 5) {
    sf::RectangleShape overlay(
        sf::Vector2f(window.getSize().x, window.getSize().y));
    overlay.setFillColor(sf::Color(0, 0, 0, 170));
    window.draw(overlay);

    sf::Text msg(font, "BENCHMARK TERMINE", 48);
    msg.setFillColor(sf::Color::Yellow);
    msg.setStyle(sf::Text::Bold);
    msg.setPosition(
        {window.getSize().x / 2.0f - msg.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 80});
    window.draw(msg);

    sf::Text sub(font, "Resultats affiches dans le terminal", 26);
    sub.setFillColor(sf::Color::White);
    sub.setPosition(
        {window.getSize().x / 2.0f - sub.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 10});
    window.draw(sub);

    sf::Text esc(font, "Appuyez sur Echap pour revenir au menu", 22);
    esc.setFillColor(sf::Color(180, 180, 180));
    esc.setPosition(
        {window.getSize().x / 2.0f - esc.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f + 40});
    window.draw(esc);
    return;
  }

  // Détermine le gagnant via Input::get_winner() — fiable même quand les deux
  // joueurs ont 5 pierres simultanément sur le plateau (cas pending + réponse).
  // Fallback sur la détection par plateau uniquement si get_winner() retourne NONE
  // (victoire sans pending : le gagnant est l'unique joueur avec 5 ou 10 captures).
  Player winner = Input::get_winner();
  std::string win_str = "GAME OVER";
  if (winner == Player::BLACK) {
      win_str = "BLACK WINS!";
  } else if (winner == Player::WHITE) {
      win_str = "WHITE WINS!";
  } else {
      // Fallback (ne devrait pas arriver mais sécurise l'affichage)
      bool black_wins = Rules::check_win_condition(state, Player::BLACK) ||
                        Rules::check_win_by_capture(state, Player::BLACK);
      bool white_wins = Rules::check_win_condition(state, Player::WHITE) ||
                        Rules::check_win_by_capture(state, Player::WHITE);
      if (black_wins && !white_wins) win_str = "BLACK WINS!";
      else if (white_wins)           win_str = "WHITE WINS!";
  }

  sf::RectangleShape overlay(
      sf::Vector2f(window.getSize().x, window.getSize().y));
  overlay.setFillColor(sf::Color(0, 0, 0, 150));
  window.draw(overlay);

  sf::Text win_msg(font, win_str, 50);
  win_msg.setFillColor(sf::Color::Red);
  win_msg.setStyle(sf::Text::Bold);
  win_msg.setPosition(
      {window.getSize().x / 2.0f - win_msg.getGlobalBounds().size.x / 2.0f,
       window.getSize().y / 2.0f - 100});
  window.draw(win_msg);

  // En Bot vs Bot : affiche les temps moyens sur l'overlay de fin
  if (ui_state == UIState::PLAYING_BOT) {
    char buf[128];
    snprintf(buf, sizeof(buf), "Temps moyen Bot1 (Noir): %.2f ms",
             state.black_avg_time_ms);
    sf::Text t1(font, buf, 22);
    t1.setFillColor(sf::Color::Cyan);
    t1.setPosition(
        {window.getSize().x / 2.0f - t1.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 40});
    window.draw(t1);

    snprintf(buf, sizeof(buf), "Temps moyen Bot2 (Blanc): %.2f ms",
             state.white_avg_time_ms);
    sf::Text t2(font, buf, 22);
    t2.setFillColor(sf::Color::Cyan);
    t2.setPosition(
        {window.getSize().x / 2.0f - t2.getGlobalBounds().size.x / 2.0f,
         window.getSize().y / 2.0f - 10});
    window.draw(t2);
  }

  // Bouton "Rejouer" : zone cliquable définie ici, détectée dans Input.cpp
  // (sf::FloatRect replay_btn avec les mêmes coordonnées)
  sf::RectangleShape btn(sf::Vector2f(200, 60));
  btn.setFillColor(sf::Color(100, 100, 100));
  btn.setPosition(
      {window.getSize().x / 2.0f - 100, window.getSize().y / 2.0f + 40});
  window.draw(btn);

  sf::Text replay_txt(font, "Rejouer", 30);
  replay_txt.setFillColor(sf::Color::White);
  replay_txt.setPosition(
      {window.getSize().x / 2.0f - replay_txt.getGlobalBounds().size.x / 2.0f,
       window.getSize().y / 2.0f + 50});
  window.draw(replay_txt);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_history : panneau droit (liste des coups + bouton suggestion)
//
// Affiche les 21 derniers coups (scrolling implicite : start_idx = size - 21).
// Chaque coup : icône disque + "Tour N : L-R" (L=lettre colonne, R=numéro ligne).
//
// Bouton suggestion :
//   - Visible en Multi (toujours) ou en Solo quand c'est le tour du joueur humain.
//   - Au clic (détecté dans Input.cpp), calcule le meilleur coup et active
//     suggestion_shown = true.
//   - Couleur verte si actif, bleue si inactif.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_history(const GameState &state) {
  float hist_x      = (BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN;
  float hist_width  = HISTORY_WIDTH;
  float hud_y_limit = (BOARD_SIZE - 1) * CELL_SIZE + 2 * MARGIN;
  float hist_height = hud_y_limit;

  sf::RectangleShape hist_bg(sf::Vector2f(hist_width, hist_height));
  hist_bg.setFillColor(sf::Color(50, 50, 50));
  hist_bg.setPosition({hist_x, 0});
  window.draw(hist_bg);

  sf::Text title(font, "Historique", 26);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Underlined | sf::Text::Bold);
  title.setPosition({hist_x + 10.0f, 10.0f});
  window.draw(title);

  // Bouton suggestion — sa zone cliquable est reproduite identiquement dans Input.cpp
  bool show_btn = !state.game_over &&
                  ((current_state == UIState::PLAYING_MULTI) ||
                   (current_state == UIState::PLAYING_SOLO &&
                    state.current_player == Player::BLACK));
  if (show_btn) {
    sf::RectangleShape btn(sf::Vector2f(230.0f, 35.0f));
    btn.setFillColor(suggestion_shown ? sf::Color(60, 140, 60) : sf::Color(60, 90, 160));
    btn.setPosition({hist_x + 10.0f, 50.0f});
    window.draw(btn);

    sf::Text btn_txt(font, suggestion_shown ? "Suggestion active" : "Suggestion", 19);
    btn_txt.setFillColor(sf::Color::White);
    btn_txt.setPosition(
        {hist_x + 10.0f + 115.0f - btn_txt.getGlobalBounds().size.x / 2.0f,
         57.0f});
    window.draw(btn_txt);
  }

  // Affichage des 21 derniers coups (scrolling par start_idx)
  size_t display_count = 21;
  size_t start_idx = 0;
  if (state.move_history.size() > display_count)
    start_idx = state.move_history.size() - display_count;

  float y_offset = 100.0f;
  float radius   = 10.0f;
  sf::CircleShape stone(radius);
  stone.setOrigin({radius, radius});

  for (size_t i = start_idx; i < state.move_history.size(); ++i) {
    const GameMove &mv = state.move_history[i];

    stone.setPosition({hist_x + 20.0f, y_offset + 10.0f});
    if (mv.player == Player::BLACK) {
      stone.setFillColor(sf::Color::Black);
      stone.setOutlineColor(sf::Color::White);
      stone.setOutlineThickness(1.5f);
    } else {
      stone.setFillColor(sf::Color::White);
      stone.setOutlineColor(sf::Color(160, 160, 160));
      stone.setOutlineThickness(1.5f);
    }
    window.draw(stone);

    // Formatage : "Tour N : L-R" avec L = lettre A-S, R = numéro 1-19
    char col_char    = 'A' + mv.x;
    std::string coord_str = std::string(1, col_char) + "-" + std::to_string(mv.y + 1);
    std::string line_str  = "Tour " + std::to_string(mv.turn) + " : " + coord_str;
    sf::Text line_text(font, line_str, 18);
    line_text.setFillColor(sf::Color::White);
    line_text.setPosition({hist_x + 40.0f, y_offset});
    window.draw(line_text);

    y_offset += 30.0f;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// update_capture_anims : gestion du cycle de vie des animations de capture
//
// Idiome erase-remove : std::remove_if déplace les éléments "morts" en fin de
// vecteur et renvoie un itérateur vers le premier mort. erase() les supprime.
// C'est le pattern standard C++ pour supprimer des éléments d'un vecteur
// pendant une itération sans invalider les itérateurs.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::update_capture_anims(float dt) {
  for (auto &a : capture_anims)
    a.timer -= dt;
  capture_anims.erase(
    std::remove_if(capture_anims.begin(), capture_anims.end(),
      [](const CaptureAnim &a) { return a.timer <= 0.0f; }),
    capture_anims.end());
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_capture_anims : croix X rouge fadeout aux positions des pierres capturées
//
// Deux barres rectangulaires rotées ±45° forment une croix X.
// Alpha = timer * 255 : l'opacité décroît linéairement de 255 à 0 sur 1 seconde.
// arm = CELL_SIZE * 0.38f : bras de la croix légèrement plus court que le rayon
// de pierre pour un rendu propre.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_capture_anims() {
  float arm = CELL_SIZE * 0.38f;
  for (const auto &a : capture_anims) {
    float cx = MARGIN + a.col * CELL_SIZE;
    float cy = MARGIN + a.row * CELL_SIZE;
    sf::Color color(220, 50, 50, (uint8_t)(a.timer * 255)); // Alpha = timer * 255

    sf::RectangleShape bar1(sf::Vector2f(arm * 2.0f, 4.0f));
    bar1.setOrigin(sf::Vector2f(arm, 2.0f)); // Centrage avant rotation
    bar1.setPosition({cx, cy});
    bar1.setFillColor(color);
    bar1.setRotation(sf::degrees(45));
    window.draw(bar1);

    sf::RectangleShape bar2(sf::Vector2f(arm * 2.0f, 4.0f));
    bar2.setOrigin(sf::Vector2f(arm, 2.0f));
    bar2.setPosition({cx, cy});
    bar2.setFillColor(color);
    bar2.setRotation(sf::degrees(-45));
    window.draw(bar2);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_ai_highlight : anneau rouge fadeout sur le dernier coup IA
//
// Alpha = (ai_highlight_timer / 0.5f) * 255 → 100% opaque au moment du coup,
// transparent 0.5s plus tard. Donne un retour visuel clair "l'IA vient de jouer ici".
// Utilise setFillColor(Transparent) + setOutlineColor() pour ne dessiner que
// l'anneau (bord) sans remplir le disque, ce qui laisse voir la pierre dessous.
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_ai_highlight() {
  if (current_state != UIState::PLAYING_SOLO) return;
  if (ai_highlight_timer <= 0.0f || ai_highlight_row < 0) return;

  float alpha  = ai_highlight_timer / 0.5f; // Normalisation 0.0 → 1.0
  float radius = CELL_SIZE * 0.46f;
  sf::CircleShape ring(radius);
  ring.setOrigin({radius, radius});
  ring.setFillColor(sf::Color::Transparent);
  ring.setOutlineColor(sf::Color(220, 50, 50, (uint8_t)(alpha * 255)));
  ring.setOutlineThickness(3.0f);
  ring.setPosition({MARGIN + ai_highlight_col * CELL_SIZE,
                    MARGIN + ai_highlight_row * CELL_SIZE});
  window.draw(ring);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_best_move : pierre fantôme du meilleur coup suggéré
//
// Affiche une pierre semi-transparente (alpha=100) à la position
// state.best_move_suggestion.{row, col}.
// Cette suggestion est calculée par getBestMove() dans Input.cpp au clic
// sur le bouton "Suggestion".
// Couleur correspondant au joueur courant (noir ou blanc fantôme).
// ─────────────────────────────────────────────────────────────────────────────
void GameUI::draw_best_move(const GameState &state) {
  if (!suggestion_shown)
    return;
  if (state.game_over)
    return;
  if (state.best_move_suggestion.row < 0 || state.best_move_suggestion.col < 0)
    return;

  float radius = CELL_SIZE * 0.4f;
  sf::CircleShape ghost(radius);
  ghost.setOrigin({radius, radius});

  if (state.current_player == Player::BLACK) {
    ghost.setFillColor(sf::Color(0, 0, 0, 100));       // Noir semi-transparent
  } else {
    ghost.setFillColor(sf::Color(255, 255, 255, 100)); // Blanc semi-transparent
  }

  ghost.setPosition(
      {(float)(MARGIN + state.best_move_suggestion.col * CELL_SIZE),
       (float)(MARGIN + state.best_move_suggestion.row * CELL_SIZE)});
  window.draw(ghost);
}
