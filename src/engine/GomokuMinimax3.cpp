#include "Gomoku.hpp"
#include <algorithm>
#include <climits>
#include <vector>

// Calcule le score résultant du coup sans modifier le board original.
// Retourne std::numeric_limits<int>::max() si le coup est illégal (double-trois).
int get_score(const BitBoard& board, const Move& move, Cell player)
{
    static const int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    BitBoard tmp = board;
    int pos = idx(move.row, move.col);
    int threesBefore = 0, threesAfter = 0;
    int scoreBefore = tmp.score;
    int scoreAfter;
    int toRemove[16][2];
    int removeCount = 0;

    for (int di = 0; di < 4; di++) {
        int code = computeLineScore(tmp, move.row, move.col, dirs[di][0], dirs[di][1]);
        if (player == WHITE)
            threesBefore += GET_WHITE_THREES(evalTable[code][1]);
        else
            threesBefore += GET_BLACK_THREES(evalTable[code][1]);
        scoreBefore -= evalTable[code][0];
    }

    if (player == WHITE) setBit(tmp.white, pos);
    else setBit(tmp.black, pos);

    scoreAfter = scoreBefore;
    for (int di = 0; di < 4; di++) {
        int dr = dirs[di][0], dc = dirs[di][1];
        int code = computeLineScore(tmp, move.row, move.col, dr, dc);
        int flags = evalTable[code][1];

        if (player == WHITE) {
            threesAfter += GET_WHITE_THREES(flags);
            if (GET_WHITE_CAPTURES_UP(flags)) {
                toRemove[removeCount][0] = move.row + dr;   toRemove[removeCount][1] = move.col + dc;   removeCount++;
                toRemove[removeCount][0] = move.row + 2*dr; toRemove[removeCount][1] = move.col + 2*dc; removeCount++;
            }
            if (GET_WHITE_CAPTURES_DOWN(flags)) {
                toRemove[removeCount][0] = move.row - dr;   toRemove[removeCount][1] = move.col - dc;   removeCount++;
                toRemove[removeCount][0] = move.row - 2*dr; toRemove[removeCount][1] = move.col - 2*dc; removeCount++;
            }
        } else {
            threesAfter += GET_BLACK_THREES(flags);
            if (GET_BLACK_CAPTURES_UP(flags)) {
                toRemove[removeCount][0] = move.row + dr;   toRemove[removeCount][1] = move.col + dc;   removeCount++;
                toRemove[removeCount][0] = move.row + 2*dr; toRemove[removeCount][1] = move.col + 2*dc; removeCount++;
            }
            if (GET_BLACK_CAPTURES_DOWN(flags)) {
                toRemove[removeCount][0] = move.row - dr;   toRemove[removeCount][1] = move.col - dc;   removeCount++;
                toRemove[removeCount][0] = move.row - 2*dr; toRemove[removeCount][1] = move.col - 2*dc; removeCount++;
            }
        }
        scoreAfter += evalTable[code][0];
    }

    if (threesAfter - threesBefore >= 2)
        return std::numeric_limits<int>::max(); // coup illégal

    if (removeCount > 0) {
        scoreAfter = scoreBefore;
        if (player == WHITE) clearBit(tmp.white, pos);
        else clearBit(tmp.black, pos);
        for (int i = 0; i < removeCount; i++) {
            for (int di = 0; di < 4; di++) {
                int code = computeLineScore(tmp, move.row, move.col, dirs[di][0], dirs[di][1]);
                scoreAfter -= evalTable[code][0];
            }
        }
        for (int i = 0; i < removeCount; i++)
            tmp.set(toRemove[i][0], toRemove[i][1], EMPTY);

        if (player == WHITE) {
            scoreAfter += MANUAL_CAPTURE_SCORE * removeCount;
            tmp.whiteCaptures += static_cast<uint8_t>(removeCount);
            if (tmp.whiteCaptures >= 10) scoreAfter += 1000000;
            setBit(tmp.white, pos);
        } else {
            scoreAfter -= MANUAL_CAPTURE_SCORE * removeCount;
            tmp.blackCaptures += static_cast<uint8_t>(removeCount);
            if (tmp.blackCaptures >= 10) scoreAfter -= 1000000;
            setBit(tmp.black, pos);
        }
        for (int i = 0; i < removeCount; i++) {
            for (int di = 0; di < 4; di++) {
                int code = computeLineScore(tmp, move.row, move.col, dirs[di][0], dirs[di][1]);
                scoreAfter += evalTable[code][0];
            }
        }
        for (int di = 0; di < 4; di++) {
            int dr = dirs[di][0], dc = dirs[di][1];
            int code = computeLineScore(tmp, move.row, move.col, dr, dc);
            scoreAfter += evalTable[code][0];
        }
    }
    return scoreAfter;
}

void apply_move(BitBoard& board, const Move& move, Cell player)
{
    static const int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    int pos = idx(move.row, move.col);

    if (player == WHITE) setBit(board.white, pos);
    else setBit(board.black, pos);

    for (int di = 0; di < 4; di++) {
        int dr = dirs[di][0], dc = dirs[di][1];
        int code = computeLineScore(board, move.row, move.col, dr, dc);
        int flags = evalTable[code][1];
        if (player == WHITE) {
            if (GET_WHITE_CAPTURES_UP(flags)) {
                board.whiteCaptures += 2;
                board.set(move.row + dr, move.col + dc, EMPTY);
                board.set(move.row + 2*dr, move.col + 2*dc, EMPTY);
            }
            if (GET_WHITE_CAPTURES_DOWN(flags)) {
                board.whiteCaptures += 2;
                board.set(move.row - dr, move.col - dc, EMPTY);
                board.set(move.row - 2*dr, move.col - 2*dc, EMPTY);
            }
        } else {
            if (GET_BLACK_CAPTURES_UP(flags)) {
                board.blackCaptures += 2;
                board.set(move.row + dr, move.col + dc, EMPTY);
                board.set(move.row + 2*dr, move.col + 2*dc, EMPTY);
            }
            if (GET_BLACK_CAPTURES_DOWN(flags)) {
                board.blackCaptures += 2;
                board.set(move.row - dr, move.col - dc, EMPTY);
                board.set(move.row - 2*dr, move.col - 2*dc, EMPTY);
            }
        }
    }
}


Move Gomoku::minimax3(int depth, BitBoard& board, Cell player, int alpha, int beta) {
    static int historyHeuristic[SIZE][SIZE] = {};
    static Move killerMoves[DEPTH_LIMIT + 2][2] = {};

    if (depth == 0) {
        for (int row = 0; row < SIZE; row++)
            for (int col = 0; col < SIZE; col++)
                historyHeuristic[row][col] = 0;
        for (int d = 0; d < DEPTH_LIMIT + 2; d++) {
            killerMoves[d][0] = {-1, -1, 0, 0};
            killerMoves[d][1] = {-1, -1, 0, 0};
        }
    }

    if (depth > DEPTH_LIMIT || std::abs(board.score) >= 1000000)
        return {-1, -1, board.score, 0};

    std::vector<Move> moves = generateMoves(board, player);
    Cell opponent = (player == WHITE) ? BLACK : WHITE;

    uint8_t whiteCapturesBefore = board.whiteCaptures;
    uint8_t blackCapturesBefore = board.blackCaptures;
    uint64_t white_board[BitBoard_SIZE];
    uint64_t black_board[BitBoard_SIZE];
    std::memcpy(black_board, board.black, sizeof(board.black));
    std::memcpy(white_board, board.white, sizeof(board.white));

    auto sameCell = [](const Move& a, const Move& b) {
        return a.row == b.row && a.col == b.col;
    };

    // Move ordering: supprime les coups illégaux, calcule le score réel dans move.score
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](Move& move) {
        int score = get_score(board, move, player);
        if (score == std::numeric_limits<int>::max())
            return true; // coup illégal, supprimé
        move.score = score; // score réel de la position, sans bonus ordering
        return false;
    }), moves.end());

    // Tri avec bonus killer/history calculés à la volée, sans polluer move.score
    auto orderScore = [&](const Move& m) {
        int s = m.score;
        if (depth < DEPTH_LIMIT + 2) {
            if (sameCell(m, killerMoves[depth][0]))      s += 500000;
            else if (sameCell(m, killerMoves[depth][1])) s += 450000;
        }
        return s + historyHeuristic[m.row][m.col];
    };
    if (player == WHITE)
        std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) { return orderScore(a) > orderScore(b); });
    else
        std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) { return orderScore(a) < orderScore(b); });

    // if ((int)moves.size() > MOVE_LIMIT)
    //     moves.resize(MOVE_LIMIT);

    auto updateKiller = [&](const Move& m) {
        if (depth >= DEPTH_LIMIT + 2) return;
        if (!sameCell(m, killerMoves[depth][0])) {
            killerMoves[depth][1] = killerMoves[depth][0];
            killerMoves[depth][0] = m;
        }
    };

    if (player == WHITE) {
        Move best = {-1, -1, std::numeric_limits<int>::min(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            board.score = move.score;
            apply_move(board, move, WHITE);
            Move eval = minimax3(depth + 1, board, opponent, alpha, beta);
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
            board.score = move.score;
            apply_move(board, move, BLACK);
            Move eval = minimax3(depth + 1, board, opponent, alpha, beta);
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

Move Gomoku::getBestMove3(BitBoard& board, Cell player) {
    // TODO : implémenter minimax avec évaluation heuristique
  struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    Move bestMove = minimax3(0, board, player, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                   (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("get_best_move3: %.3f ms, move.score : %d, board.score : %d\n", elapsed, bestMove.score, board.score);

    return bestMove;
}
