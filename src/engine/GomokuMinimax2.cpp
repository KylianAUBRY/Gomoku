#include "Gomoku.hpp"
#include <algorithm>
#include <climits>
#include <vector>

static uint64_t computeFullHash(const BitBoard& board)
{
    uint64_t h = 0;
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++)
            h ^= zobristTable[r][c][(int)board.get(r, c)];
    h ^= zobristCaptures[board.whiteCaptures][0];
    h ^= zobristCaptures[board.blackCaptures][1];
    return h;
}

static bool ttProbe(uint64_t hash, int depthRemaining, int alpha, int beta, int& score)
{
    const TTEntry& e = ttTable[hash & TT_MASK];
    if (e.hash != hash)            return false; // collision ou entrée vide
    if (e.depth < depthRemaining)  return false; // calculée trop superficiellement

    if (e.flag == TT_EXACT)                     { score = e.score; return true; }
    if (e.flag == TT_LOWER && e.score >= beta)  { score = e.score; return true; }
    if (e.flag == TT_UPPER && e.score <= alpha) { score = e.score; return true; }
    return false;
}

static void ttStore(uint64_t hash, int depthRemaining, int score, TTFlag flag)
{
    TTEntry& e = ttTable[hash & TT_MASK];
    if (e.hash == hash && e.depth > depthRemaining) return;
    e = { hash, score, (int8_t)depthRemaining, flag };
}

Move Gomoku::minimax2(int depth, BitBoard& board, Cell player, int alpha, int beta, int gamePhase, bool allowNull) {
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

    // ── Transposition Table : probe ──────────────────────────────────────────
    // depthRemaining : profondeur qu'il reste à explorer depuis ce nœud.
    // Plus c'est grand, plus l'entrée est précieuse (calcul plus profond).
    // On ne sonde pas à depth==0 car on a besoin du coup réel, pas du score.
    int depthRemaining = DEPTH_LIMIT - depth;
    int alphaOrig = alpha; // sauvegardé pour calculer le flag TT en fin de nœud
    if (depth > 0) {
        int ttScore;
        if (ttProbe(board.hash, depthRemaining, alpha, beta, ttScore))
            return {-1, -1, ttScore, 0};
        
    }

    //ÉLAGAGE FUTILE ASYMÉTRIQUE 
    if (depth >= DEPTH_LIMIT - 2) {
        int margin = gamePhase * (DEPTH_LIMIT - depth);
        if (player == WHITE) {
            // WHITE maximise : si score + margin <= alpha, élaguer
            if (board.score + margin <= alpha) {
                ttStore(board.hash, depthRemaining, board.score, TT_UPPER);
                return {-1, -1, board.score, 0};
            }
        } else {
            // BLACK minimise : si score - margin >= beta, élaguer
            if (board.score - margin >= beta) {
                ttStore(board.hash, depthRemaining, board.score, TT_LOWER);
                return {-1, -1, board.score, 0};
            }
        }
    }

    // ── ÉLAGAGE PAR COUP NUL ─────────────────────────────────────────────────
    // Si passer son tour (coup nul) provoque déjà une coupure beta/alpha,
    // alors jouer un vrai coup serait encore meilleur → on peut élaguer.
    // Interdit : racine, profondeur insuffisante, positions quasi-terminales,
    // et deux coups nuls consécutifs (allowNull).
    const int NULL_R = 2; // facteur de réduction
    if (allowNull && depth > 0 && depthRemaining > NULL_R + 1 && std::abs(board.score) < 900000) {
        Cell nullOpponent = (player == WHITE) ? BLACK : WHITE;
        Move nullEval = minimax(depth + 1 + NULL_R, board, nullOpponent, alpha, beta, gamePhase, false);
        if (player == WHITE && nullEval.score >= beta) {
            ttStore(board.hash, depthRemaining, nullEval.score, TT_LOWER);
            return {-1, -1, nullEval.score, 0};
        }
        if (player == BLACK && nullEval.score <= alpha) {
            ttStore(board.hash, depthRemaining, nullEval.score, TT_UPPER);
            return {-1, -1, nullEval.score, 0};
        }
    }

    uint8_t whiteCapturesBefore = board.whiteCaptures;
    uint8_t blackCapturesBefore = board.blackCaptures;
    uint64_t hashBefore          = board.hash;
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
    Move best;
    TTFlag ttFlag;
    bool   cutOff = false;
    if (player == WHITE) {
        best = {-1, -1, std::numeric_limits<int>::min(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, WHITE) == 1)
                continue ; // coup illégal
            Move eval = minimax2(depth + 1, board, opponent, alpha, beta, gamePhase);
            std::memcpy(board.black, black_board, sizeof(black_board));
            std::memcpy(board.white, white_board, sizeof(white_board));
            board.whiteCaptures = whiteCapturesBefore;
            board.blackCaptures = blackCapturesBefore;
            board.score = scoreBefore;
            board.hash = hashBefore;
            if (eval.score > best.score) {
                best = {move.row, move.col, eval.score, 0};
            }
            if (best.score > alpha)
                alpha = best.score;
            if (alpha >= beta) {
                updateKiller(move);
                historyHeuristic[move.row][move.col] += 1 << (DEPTH_LIMIT - depth);
                cutOff = true;
                break; // coupure bêta
            }
        }
        if (cutOff)
            ttFlag = TT_LOWER;
        else if (best.score <= alphaOrig)
            ttFlag = TT_UPPER;
        else
            ttFlag = TT_EXACT;
    } else {
        best = {-1, -1, std::numeric_limits<int>::max(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, BLACK) == 1)
                continue ; // coup illégal
            Move eval = minimax2(depth + 1, board, opponent, alpha, beta, gamePhase);
            std::memcpy(board.black, black_board, sizeof(black_board));
            std::memcpy(board.white, white_board, sizeof(white_board));
            board.whiteCaptures = whiteCapturesBefore;
            board.blackCaptures = blackCapturesBefore;
            board.score = scoreBefore;
            board.hash = hashBefore;
            if (eval.score < best.score) {
                best = {move.row, move.col, eval.score, 0};
            }
            if (best.score < beta)
                beta = best.score;
            if (alpha >= beta) {
                updateKiller(move);
                historyHeuristic[move.row][move.col] += 1 << (DEPTH_LIMIT - depth);
                cutOff = true;
                break; // coupure alpha
            }
        }
        ttFlag = cutOff ? TT_UPPER : TT_EXACT;
    }
    if (depth > 0)
        ttStore(board.hash, depthRemaining, best.score, ttFlag);
    return best;
}

void clearTTv2() { memset(ttTable, 0, sizeof(ttTable)); }

Move Gomoku::getBestMove2(BitBoard& board, Cell player) {
    struct timespec start, end;

    // Initialise la table Zobrist une seule fois (seed fixe → déterministe)
     int totalPieces = 0;
    for (int i = 0; i < BitBoard_SIZE; i++) {
        totalPieces += __builtin_popcountll(board.white[i]);
        totalPieces += __builtin_popcountll(board.black[i]);
    }

    // Déterminer la phase
    int gamePhase;
    if (totalPieces < 10) {
        gamePhase = 50;  // OUVERTURE
    } else if (totalPieces < 50) {
        gamePhase = 400;  // MILIEU
    } else {
        gamePhase = 700;  // FIN
    }

    initZobrist();

    // board.hash = computeFullHash(board) ^ 0xAAAAAAAAAAAAAAAAULL;
    board.hash = computeFullHash(board) ^ 0xBBBBBBBBBBBBBBBBULL; // Différent de getBestMove pour éviter les collisions entre les deux bots lors du benchmark


    clock_gettime(CLOCK_MONOTONIC, &start);
    // Move bestMove = minimax(0, board, player, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), gamePhase);

    Move bestMove = minimax2(0, board, player, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), gamePhase);
    clock_gettime(CLOCK_MONOTONIC, &end);

    // double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
    //                (end.tv_nsec - start.tv_nsec) / 1e6;
    // printf("get_best_move: %.3f ms, move.score : %d, board.score : %d\n", elapsed, bestMove.score, board.score);

    return bestMove;
}
