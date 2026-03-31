#include "Gomoku.hpp"
#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

// =============================================================================
// ZOBRIST HASHING
// =============================================================================
//
// Tables définies dans GomokuCommon.cpp, déclarées extern dans Gomoku.hpp.
// makeMove met board.hash à jour en O(1) directement (XOR incrémental).
// Le hash est restauré en O(1) lors du undo via hashBefore.
// computeFullHash est appelé une seule fois dans getBestMove2 pour initialiser.
// =============================================================================


// Calcule le hash complet depuis zéro en parcourant tout le plateau.
// Appelé une seule fois dans getBestMove2, et après chaque makeMove
// (car makeMove ne met pas board.hash à jour).
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

// =============================================================================
// TRANSPOSITION TABLE (TT)
// =============================================================================
//
// Le même état de jeu peut être atteint par plusieurs chemins différents
// dans l'arbre minimax (transpositions). La TT stocke les scores déjà
// calculés pour éviter de re-explorer ces branches.
//
// Chaque entrée contient :
//   hash  : hash complet pour vérifier les collisions.
//            (la table est indexée par hash % TT_SIZE ; deux positions
//            différentes peuvent avoir le même index → on vérifie le hash)
//   score : la valeur calculée
//   depth : profondeur RESTANTE quand le score a été calculé.
//            On n'utilise l'entrée que si elle a été calculée au moins
//            aussi profond que notre recherche actuelle.
//   flag  : type de borne sur le score
//
//     TT_EXACT : score exact — la recherche a exploré tous les coups
//                sans coupure et le score est entre alpha et beta
//
//     TT_LOWER : borne inférieure — vrai score ≥ score stocké
//                (fail-high WHITE : on a coupé car score ≥ beta,
//                 on n'a pas exploré toutes les branches, mais on sait
//                 que le score est au moins aussi bon)
//
//     TT_UPPER : borne supérieure — vrai score ≤ score stocké
//                (fail-low WHITE  : tous les coups étaient ≤ alpha,
//                 ou coupure BLACK : score ≤ alpha)
//
// Utilisation à la sonde :
//   TT_EXACT             → score utilisable directement
//   TT_LOWER et score≥β  → coupure bêta  (on peut couper)
//   TT_UPPER et score≤α  → coupure alpha (on peut couper)
// =============================================================================


// Sonde la TT. Retourne true et écrit dans 'score' si une entrée utilisable
// est trouvée pour ce hash à cette profondeur.
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

// Stocke le résultat dans la TT.
// Stratégie "depth-preferred" : on remplace une entrée existante seulement
// si on a calculé plus profond (l'entrée plus profonde est plus précieuse).
static void ttStore(uint64_t hash, int depthRemaining, int score, TTFlag flag)
{
    TTEntry& e = ttTable[hash & TT_MASK];
    if (e.hash == hash && e.depth > depthRemaining) return;
    e = { hash, score, (int8_t)depthRemaining, flag };
}

// =============================================================================
// MINIMAX2 — avec Transposition Table
// =============================================================================

Move Gomoku::minimax3(int depth, BitBoard& board, Cell player, int alpha, int beta)
{
    static int  historyHeuristic[SIZE][SIZE] = {};
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
    // ─────────────────────────────────────────────────────────────────────────

    std::vector<Move> moves = generateMoves(board, player);
    Cell opponent = (player == WHITE) ? BLACK : WHITE;

    // Sauvegarde de l'état complet avant la boucle de coups
    uint8_t  whiteCapturesBefore = board.whiteCaptures;
    uint8_t  blackCapturesBefore = board.blackCaptures;
    uint64_t hashBefore          = board.hash; // ← clé : on restaure en O(1) après chaque coup
    uint64_t white_board[BitBoard_SIZE];
    uint64_t black_board[BitBoard_SIZE];
    std::memcpy(black_board, board.black, sizeof(board.black));
    std::memcpy(white_board, board.white, sizeof(board.white));

    auto sameCell = [](const Move& a, const Move& b) {
        return a.row == b.row && a.col == b.col;
    };

    auto updateKiller = [&](const Move& m) {
        if (depth >= DEPTH_LIMIT + 2) return;
        if (!sameCell(m, killerMoves[depth][0])) {
            killerMoves[depth][1] = killerMoves[depth][0];
            killerMoves[depth][0] = m;
        }
    };

    for (Move& move : moves) {
        if (depth < DEPTH_LIMIT + 2) {
            if (sameCell(move, killerMoves[depth][0]))
                move.score += 500000;
            else if (sameCell(move, killerMoves[depth][1]))
                move.score += 450000;
        }
        move.score += historyHeuristic[move.row][move.col];
    }
    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) { return a.score > b.score; });

    Move   best;
    TTFlag ttFlag;
    bool   cutOff = false;

    if (player == WHITE) {
        best = {-1, -1, std::numeric_limits<int>::min(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, WHITE) == 1)
                continue; // coup illégal

            Move eval = minimax3(depth + 1, board, opponent, alpha, beta);

            // Undo complet
            std::memcpy(board.black, black_board, sizeof(black_board));
            std::memcpy(board.white, white_board, sizeof(white_board));
            board.whiteCaptures = whiteCapturesBefore;
            board.blackCaptures = blackCapturesBefore;
            board.score         = scoreBefore;
            board.hash          = hashBefore; // ← undo du hash en O(1)

            if (eval.score > best.score)
                best = {move.row, move.col, eval.score, 0};
            if (best.score > alpha)
                alpha = best.score;
            if (alpha >= beta) {
                updateKiller(move);
                historyHeuristic[move.row][move.col] += 1 << (DEPTH_LIMIT - depth);
                cutOff = true;
                break; // coupure bêta
            }
        }
        // Calcul du flag TT :
        //   cutOff          → fail-high → vrai score ≥ best.score → borne inférieure
        //   best ≤ alphaOrig → fail-low  → vrai score ≤ best.score → borne supérieure
        //   sinon           → score exact (toutes les branches explorées)
        if      (cutOff)                  ttFlag = TT_LOWER;
        else if (best.score <= alphaOrig) ttFlag = TT_UPPER;
        else                              ttFlag = TT_EXACT;

    } else {
        best = {-1, -1, std::numeric_limits<int>::max(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, BLACK) == 1)
                continue; // coup illégal

            Move eval = minimax3(depth + 1, board, opponent, alpha, beta);

            std::memcpy(board.black, black_board, sizeof(black_board));
            std::memcpy(board.white, white_board, sizeof(white_board));
            board.whiteCaptures = whiteCapturesBefore;
            board.blackCaptures = blackCapturesBefore;
            board.score         = scoreBefore;
            board.hash          = hashBefore; // ← undo du hash en O(1)

            if (eval.score < best.score)
                best = {move.row, move.col, eval.score, 0};
            if (best.score < beta)
                beta = best.score;
            if (alpha >= beta) {
                updateKiller(move);
                historyHeuristic[move.row][move.col] += 1 << (DEPTH_LIMIT - depth);
                cutOff = true;
                break; // coupure alpha
            }
        }
        // Pour le minimiseur :
        //   cutOff → BLACK a trouvé un score ≤ alpha → WHITE ne laissera pas arriver ici
        //            → vrai score pourrait être encore plus bas → borne supérieure
        //   sinon  → toutes les branches explorées → score exact
        ttFlag = cutOff ? TT_UPPER : TT_EXACT;
    }

    // ── Transposition Table : store ──────────────────────────────────────────
    // On ne stocke pas la racine (depth==0) : on a besoin du coup, pas du score seul.
    if (depth > 0)
        ttStore(board.hash, depthRemaining, best.score, ttFlag);
    // ─────────────────────────────────────────────────────────────────────────

    return best;
}

Move Gomoku::getBestMove3(BitBoard& board, Cell player)
{
    struct timespec start, end;

    // Initialise la table Zobrist une seule fois (seed fixe → déterministe)
    initZobrist();

    // Calcule le hash complet du board courant depuis zéro.
    // À partir d'ici, après chaque makeMove on recalcule via computeFullHash,
    // et après chaque undo on restaure depuis hashBefore.
    board.hash = computeFullHash(board);

    clock_gettime(CLOCK_MONOTONIC, &start);
    Move bestMove = minimax3(0, board, player, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                     (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("get_best_move3: %.3f ms, move.score : %d, board.score : %d\n", elapsed, bestMove.score, board.score);

    return bestMove;
}
