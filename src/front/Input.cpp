#include "Input.hpp"
#include "../engine/Rules.hpp"

namespace Input
{

void handle_menu_input(const sf::Event &event, UIState &current_state,
                       int &menu_selection, const sf::RenderWindow &window)
{
    if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
    {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Up ||
            keyPressed->scancode == sf::Keyboard::Scancode::Down)
        {
            menu_selection = (menu_selection == 0) ? 1 : 0;
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter)
        {
            if (menu_selection == 0)
            {
                current_state = UIState::PLAYING_SOLO;
            }
            else
            {
                current_state = UIState::PLAYING_MULTI;
            }
        }
    }

    // Mouse hover logic for menu
    if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>())
    {
        float mx = mouseMoved->position.x;
        float my = mouseMoved->position.y;
        float center_x = window.getSize().x / 2.0f;

        // Approximated bounding boxes based on position and size
        sf::FloatRect solo_rect({center_x - 150, 300}, {300, 50});
        sf::FloatRect multi_rect({center_x - 200, 400}, {400, 50});

        if (solo_rect.contains({mx, my}))
        {
            menu_selection = 0;
        }
        else if (multi_rect.contains({mx, my}))
        {
            menu_selection = 1;
        }
    }

    // Mouse click logic for menu
    if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (mouseButton->button == sf::Mouse::Button::Left)
        {
            if (menu_selection == 0)
            {
                current_state = UIState::PLAYING_SOLO;
            }
            else if (menu_selection == 1)
            {
                current_state = UIState::PLAYING_MULTI;
            }
        }
    }
}

void handle_game_input(const sf::Event &event, UIState &current_state,
                       GameState &state, const sf::RenderWindow &window)
{
    if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
    {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
        {
            current_state = UIState::MAIN_MENU; // Back to Menu
            state.reset();                      // Always reset on exit
        }
    }

    if (const auto *mouseButton = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (mouseButton->button == sf::Mouse::Button::Left)
        {
            float mx = mouseButton->position.x;
            float my = mouseButton->position.y;

            if (state.game_over)
            {
                // Check replay button click
                sf::FloatRect replay_btn({window.getSize().x / 2.0f - 100,
                                          window.getSize().y / 2.0f + 20},
                                         {200, 60});
                if (replay_btn.contains({mx, my}))
                {
                    state.reset();
                }
                return; // Prevent further interactions if game over
            }

            int grid_x = (mx - MARGIN + CELL_SIZE / 2) / CELL_SIZE;
            int grid_y = (my - MARGIN + CELL_SIZE / 2) / CELL_SIZE;

            if (Rules::is_valid_move(state, grid_x, grid_y))
            {
                Player current =
                    state.current_player; // Before placement modifies it
                if (state.place_stone(grid_x, grid_y))
                {
                    Rules::process_captures(state, grid_x, grid_y, current);

                    // Check win condition after the move
                    if (Rules::check_win_condition(state, Player::BLACK) ||
                        Rules::check_win_by_capture(state, Player::BLACK) ||
                        Rules::check_win_condition(state, Player::WHITE) ||
                        Rules::check_win_by_capture(state, Player::WHITE))
                    {
                        state.game_over = true;
                    }
                }
            }
        }
    }
}

void process_events(sf::RenderWindow &window, UIState &current_state,
                    int &menu_selection, GameState &state)
{
    while (const std::optional<sf::Event> event = window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            window.close();
        }

        if (current_state == UIState::MAIN_MENU)
        {
            handle_menu_input(*event, current_state, menu_selection, window);
        }
        else
        {
            handle_game_input(*event, current_state, state, window);
        }
    }
}
} // namespace Input
