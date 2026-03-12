#pragma once

#include "../engine/GameState.hpp"
#include "../engine/Gomoku.hpp"
#include "GameUI.hpp"
#include <SFML/Graphics.hpp>

namespace Input {
// Process SFML events, separating Menu inputs and Gameplay inputs.
void process_events(sf::RenderWindow &window, UIState &current_state,
                    int &menu_selection, GameState &state, Gomoku &gomoku);
} // namespace Input
