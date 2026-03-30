#include "Gomoku.hpp"
#include <algorithm>
#include <climits>
#include <vector>

Move Gomoku::minimax2(int depth, BitBoard& board, Cell player, int alpha, int beta) {
    static int historyHeuristic[SIZE][SIZE] = {};
    static Move killerMoves[DEPTH_LIMIT + 2][2] = {};

    if (depth == 0) {
        for (int row = 0; row < SIZE; row++) {
            for (int col = 0; col < SIZE; col++)
            historyHeuristic[row][col] = 0;
        }
        for (int d = 0; d < DEPTH_LIMIT + 2; d++) {
            killerMoves[d][0] = {-1, -1, 0, 0};
            killerMoves[d][1] = {-1, -1, 0, 0};
        }
    }
    if (depth > DEPTH_LIMIT || std::abs(board.score) >= 1000000)
        return {-1, -1, board.score, 0};

    uint8_t whiteCapturesBefore = board.whiteCaptures;
    uint8_t blackCapturesBefore = board.blackCaptures;
    uint64_t white_board[BitBoard_SIZE];
    uint64_t black_board[BitBoard_SIZE];
    std::memcpy(black_board, board.black, sizeof(board.black));
    std::memcpy(white_board, board.white, sizeof(board.white));

    std::vector<Move> moves = generateMoves(board, player);
    Cell opponent = (player == WHITE) ? BLACK : WHITE;

    // Move ordering: score chaque coup avec une évaluation shallow, puis trier
    auto sameCell = [](const Move &a, const Move &b) {
        return a.row == b.row && a.col == b.col;
    };

    auto updateKiller = [&](const Move& m) {
        if (depth >= DEPTH_LIMIT + 2) return;
        if (!sameCell(m, killerMoves[depth][0])) {
            killerMoves[depth][1] = killerMoves[depth][0];
            killerMoves[depth][0] = m;
        }
    };

    for (Move &move : moves) {
        if (depth < DEPTH_LIMIT + 2) {
            if (sameCell(move, killerMoves[depth][0]))
                move.score += 500000;
            else if (sameCell(move, killerMoves[depth][1]))
                move.score += 450000;
        }
        move.score += historyHeuristic[move.row][move.col];
    }
    std::sort(moves.begin(), moves.end(),  [](const Move &a, const Move &b) { return a.score > b.score; });

    // if ((int)moves.size() > MOVE_LIMIT)
    //     moves.resize(MOVE_LIMIT);

    if (player == WHITE) {
        Move best = {-1, -1, std::numeric_limits<int>::min(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, WHITE) == 1)
                continue ; // coup illégal
            Move eval = minimax2(depth + 1, board, opponent, alpha, beta);
            std::memcpy(board.black, black_board, sizeof(black_board));
            std::memcpy(board.white, white_board, sizeof(white_board));
            board.whiteCaptures = whiteCapturesBefore;
            board.blackCaptures = blackCapturesBefore;
            // undoMove(board, move, WHITE);
            board.score = scoreBefore; // reset score to avoid accumulation d'erreurs
            if (eval.score > best.score) {
                best = {move.row, move.col, eval.score, 0};
            }
            if (best.score > alpha)
                alpha = best.score;
            if (alpha >= beta) {
                updateKiller(move);
                historyHeuristic[move.row][move.col] += 1 << (DEPTH_LIMIT - depth);
                break; // coupure bêta
            }
        }
        return best;
    } else {
        Move best = {-1, -1, std::numeric_limits<int>::max(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, BLACK) == 1)
                continue ; // coup illégal
            Move eval = minimax2(depth + 1, board, opponent, alpha, beta);
            // undoMove(board, move, BLACK);

            std::memcpy(board.black, black_board, sizeof(black_board));
            std::memcpy(board.white, white_board, sizeof(white_board));
            board.whiteCaptures = whiteCapturesBefore;
            board.blackCaptures = blackCapturesBefore;
            board.score = scoreBefore; // reset score to avoid accumulation d'erreurs
            if (eval.score < best.score) {
                best = {move.row, move.col, eval.score, 0};
            }
            if (best.score < beta)
                beta = best.score;
            if (alpha >= beta) {
                updateKiller(move);
                historyHeuristic[move.row][move.col] += 1 << (DEPTH_LIMIT - depth);
                break; // coupure alpha
            }
        }
        return best;
    }
}

Move Gomoku::getBestMove2(BitBoard& board, Cell player) {
    // TODO : implémenter minimax avec évaluation heuristique
  struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    Move bestMove = minimax2(0, board, player, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                   (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("get_best_move2: %.3f ms, move.score : %d, board.score : %d\n", elapsed, bestMove.score, board.score);

    return bestMove;
}
