#include "Input.hpp"
#include "../engine/Rules.hpp"

namespace Input {

void handle_menu_input(const sf::Event &event, UIState &current_state,
                       int &menu_selection) {
  if (const auto *keyPressed = event.template getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Up ||
        keyPressed->scancode == sf::Keyboard::Scancode::Down) {
      menu_selection = (menu_selection == 0) ? 1 : 0;
    } else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
      if (menu_selection == 0) {
        current_state = UIState::PLAYING_SOLO;
      } else {
        current_state = UIState::PLAYING_MULTI;
      }
    }
  }
}

void handle_game_input(const sf::Event &event, UIState &current_state,
                       GameState &state) {
  if (const auto *keyPressed = event.template getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
      current_state = UIState::MAIN_MENU; // Back to Menu
    }
  }

  if (const auto *mouseButton =
          event.template getIf<sf::Event::MouseButtonPressed>()) {
    if (mouseButton->button == sf::Mouse::Button::Left) {
      int mx = mouseButton->position.x - MARGIN;
      int my = mouseButton->position.y - MARGIN;

      int grid_x = (mx + CELL_SIZE / 2) / CELL_SIZE;
      int grid_y = (my + CELL_SIZE / 2) / CELL_SIZE;

      if (Rules::is_valid_move(state, grid_x, grid_y)) {
        Player current = state.current_player; // Before placement modifies it
        if (state.place_stone(grid_x, grid_y)) {
          Rules::process_captures(state, grid_x, grid_y, current);
        }
      }
    }
  }
}

void process_events(sf::RenderWindow &window, UIState &current_state,
                    int &menu_selection, GameState &state) {
  while (const std::optional<sf::Event> event = window.pollEvent()) {
    if (event->is<sf::Event::Closed>()) {
      window.close();
    }

    if (current_state == UIState::MAIN_MENU) {
      handle_menu_input(*event, current_state, menu_selection);
    } else {
      handle_game_input(*event, current_state, state);
    }
  }
}
} // namespace Input
