#include "Rules.hpp"
#include <vector>

namespace Rules
{

// Collects positions of a line of 5+ same-colored stones along axis (dx,dy)
// through (sx,sy). Returns true if >= 5 stones found.
static bool get_line_positions(const BitBoard &board, Cell player_cell,
                               int sx, int sy, int dx, int dy,
                               std::vector<std::pair<int,int>> &out)
{
    out.clear();
    // Rewind to the beginning of the line
    int x = sx, y = sy;
    while (in_bounds(x - dx, y - dy) && board.get(y - dy, x - dx) == player_cell)
    { x -= dx; y -= dy; }
    // Collect forward
    while (in_bounds(x, y) && board.get(y, x) == player_cell)
    { out.push_back({x, y}); x += dx; y += dy; }
    return (int)out.size() >= 5;
}

// Returns true if the stone at (x,y) is part of a capturable 2-stone pair,
// i.e. there exists a direction where: OPP-P-P-EMPTY or EMPTY-P-P-OPP
// and (x,y) is one of the two P's.
static bool in_capturable_pair(const BitBoard &board, Cell player_cell,
                               Cell opp_cell, int x, int y)
{
    static const int axes[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
    for (auto &a : axes)
    {
        int dx = a[0], dy = a[1];
        // (x,y) is the LEFT stone of the pair → right neighbor is also player
        if (in_bounds(x+dx, y+dy) && board.get(y+dy, x+dx) == player_cell
            && in_bounds(x-dx, y-dy) && in_bounds(x+2*dx, y+2*dy))
        {
            Cell L = board.get(y-dy,   x-dx);
            Cell R = board.get(y+2*dy, x+2*dx);
            if ((L == opp_cell && R == EMPTY) || (L == EMPTY && R == opp_cell))
                return true;
        }
        // (x,y) is the RIGHT stone of the pair → left neighbor is also player
        if (in_bounds(x-dx, y-dy) && board.get(y-dy, x-dx) == player_cell
            && in_bounds(x-2*dx, y-2*dy) && in_bounds(x+dx, y+dy))
        {
            Cell L = board.get(y-2*dy, x-2*dx);
            Cell R = board.get(y+dy,   x+dx);
            if ((L == opp_cell && R == EMPTY) || (L == EMPTY && R == opp_cell))
                return true;
        }
    }
    return false;
}

bool check_alignment_at(const BitBoard &board, Cell player_cell, int sx, int sy,
                        int dx, int dy)
{
    int count = 0;
    int x = sx, y = sy;

    while (in_bounds(x, y) && board.get(y, x) == player_cell)
    {
        count++;
        x += dx;
        y += dy;
    }

    // Check opposite direction to get the full line
    x = sx - dx;
    y = sy - dy;
    while (in_bounds(x, y) && board.get(y, x) == player_cell)
    {
        count++;
        x -= dx;
        y -= dy;
    }

    return count >= 5;
}

bool check_win_condition(const GameState &state, Player player)
{
    Cell player_cell = (player == Player::BLACK) ? BLACK : WHITE;
    Cell opp_cell    = (player == Player::BLACK) ? WHITE : BLACK;

    static const int axes[4][2] = {{1,0},{0,1},{1,1},{1,-1}};

    for (int y = 0; y < 19; ++y)
    {
        for (int x = 0; x < 19; ++x)
        {
            if (state.board.get(y, x) != player_cell)
                continue;

            for (auto &a : axes)
            {
                std::vector<std::pair<int,int>> line;
                if (!get_line_positions(state.board, player_cell, x, y, a[0], a[1], line))
                    continue;

                // If any stone in the line is part of a capturable pair → not a win yet
                bool breakable = false;
                for (auto &[lx, ly] : line)
                {
                    if (in_capturable_pair(state.board, player_cell, opp_cell, lx, ly))
                    { breakable = true; break; }
                }
                if (!breakable)
                    return true;
            }
        }
    }
    return false;
}

CaptureResult process_captures(GameState &state, int x, int y,
                               Player current_player)
{
    CaptureResult result = {false, 0};
    Cell own_cell = (current_player == Player::BLACK) ? BLACK : WHITE;
    Cell opp_cell = (current_player == Player::BLACK) ? WHITE : BLACK;

    // 8 directions to check for captures: (dx, dy)
    int directions[8][2] = {{1, 0}, {-1, 0},  {0, 1},  {0, -1},
                            {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};

    for (auto &dir : directions)
    {
        int dx = dir[0], dy = dir[1];
        int x1 = x + dx, y1 = y + dy;
        int x2 = x + 2 * dx, y2 = y + 2 * dy;
        int x3 = x + 3 * dx, y3 = y + 3 * dy;

        if (in_bounds(x3, y3))
        {
            // If sequence is: Own(x,y) - Opp(x1,y1) - Opp(x2,y2) - Own(x3,y3)
            if (state.board.get(y1, x1) == opp_cell &&
                state.board.get(y2, x2) == opp_cell &&
                state.board.get(y3, x3) == own_cell)
            {
                // Capture! Remove opponent stones.
                state.board.set(y1, x1, EMPTY);
                state.board.set(y2, x2, EMPTY);
                result.captured = true;
                result.count += 2;
            }
        }
    }

    if (result.captured)
    {
        if (current_player == Player::BLACK)
            state.board.blackCaptures += static_cast<uint8_t>(result.count);
        else
            state.board.whiteCaptures += static_cast<uint8_t>(result.count);
    }

    return result;
}

bool is_valid_move(const GameState &state, int x, int y)
{
    if (!state.is_empty(x, y))
        return false;
    // Placeholder for Double-Three verification
    return true;
}

bool check_win_by_capture(const GameState &state, Player player)
{
    if (player == Player::BLACK)
        return state.board.blackCaptures >= 10;
    return state.board.whiteCaptures >= 10;
}

} // namespace Rules
