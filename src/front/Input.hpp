#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Input.hpp — Contrôleur 2D (SFML)
//
// Séparation de responsabilités :
//   - GameUI gère le RENDU uniquement.
//   - Input (namespace) gère toute la LOGIQUE d'entrée et les transitions d'état.
//
// Pourquoi un namespace plutôt qu'une classe ?
//   Input n'a pas d'état propre visible de l'extérieur — son état interne
//   (s_ai_pending, s_pending_alignment_win) est géré par des static locaux dans
//   le .cpp, invisibles depuis l'extérieur. Un namespace est suffisant et plus
//   simple qu'une classe singleton.
// ─────────────────────────────────────────────────────────────────────────────

#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "GameUI.hpp"
#include <SFML/Graphics.hpp>

namespace Input {

// Point d'entrée unique appelé chaque frame depuis GameUI::run().
// Draîne la queue d'événements SFML (pollEvent) et dispatch selon l'état courant.
// Gère aussi les actions non-événementielles (cooldown IA, bot-vs-bot, benchmark)
// qui s'exécutent chaque frame indépendamment des événements.
void process_events(sf::RenderWindow &window, UIState &current_state,
                    int &menu_selection, GameState &state, Gomoku &gomoku,
                    bool &suggestion_shown);

} // namespace Input
