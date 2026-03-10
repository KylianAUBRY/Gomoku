#include "Gomoku.hpp"
#include <climits>
#include <vector>

Gomoku::Gomoku() {
    for (auto& row : board)
        row.fill(EMPTY);
}

static int evaluate(const Board& board, Cell player) {
    // Simple evaluation: count cells owned by player
    int score = 0;
    for (const auto& row : board)
        for (const auto& cell : row)
            if (cell == player) score++;
    return score;
}

Move Gomoku::getBestMove(const Board& board, Cell player) {
    auto start = std::chrono::steady_clock::now();

    int bestScore = INT_MIN;
    Move bestMove = {-1, -1, 0};

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            if (board[row][col] == EMPTY) {
                Board newBoard = board;
                newBoard[row][col] = player;

                Move result = minimax(newBoard, player);
                if (result.row == -1) { // minimax returned an eval
                    int score = evaluate(newBoard, player);
                    if (score > bestScore) {
                        bestScore = score;
                        bestMove = {row, col, 0};
                    }
                }
            }
        }
    }

    auto end = std::chrono::steady_clock::now();
    bestMove.computeTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return bestMove;
}

std::vector<Move> Gomoku::generateMoves(const Board& board) {
    std::vector<Move> moves;
    std::array<std::array<bool, SIZE>, SIZE> inRange = {};

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            if (board[row][col] != EMPTY) {
                for (int dr = -RANGE; dr <= RANGE; dr++) {
                    for (int dc = -RANGE; dc <= RANGE; dc++) {
                        int r = row + dr;
                        int c = col + dc;
                        if (r >= 0 && r < SIZE && c >= 0 && c < SIZE)
                            inRange[r][c] = true;
                    }
                }
            }
        }
    }

    for (int row = 0; row < SIZE; row++)
        for (int col = 0; col < SIZE; col++)
            if (board[row][col] == EMPTY && inRange[row][col])
                moves.push_back({row, col, 0, 0});

    // Si aucun coup trouvé (plateau vide), jouer au centre
    if (moves.empty())
        moves.push_back({SIZE / 2, SIZE / 2, 0, 0});

    return moves;
}

Move Gomoku::minimax(int depth, bool maximizingPlayer) { //fonction decouper en 3 partie, la premiere partie verifie si on a atteint la profondeur limite. 
    if (depth > DEPTH_LIMIT) {
        // return {-1, -1, evaluate(board, player)};
    }
    if (maximizingPlayer)
    {
        int maxEval = std::numeric_limits<int>::min();
        std::vector<Move> moves = generateMoves();
    }
    // return {-1, -1, 0};
}