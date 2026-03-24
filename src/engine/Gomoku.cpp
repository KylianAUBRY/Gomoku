#include "Gomoku.hpp"
#include <algorithm>
#include <climits>
#include <vector>

// Bitboard helpers: 361 bits = 6 x uint64_t (bits 0..360, bit 361..383 inutilisés)
// static constexpr int TOTAL_BITS = SIZE * SIZE; // 361


static inline int idx(int row, int col) {
    return row * SIZE + col;
}

static inline bool getBit(const uint64_t* bb, int pos) {
    return (bb[pos / 64] >> (pos % 64)) & 1ULL;
}

static inline void setBit(uint64_t* bb, int pos) {
    bb[pos / 64] |= (1ULL << (pos % 64));
}

static inline void clearBit(uint64_t* bb, int pos) {
    bb[pos / 64] &= ~(1ULL << (pos % 64));
}

// Vérifie si la case (row,col) est occupée par l'un ou l'autre joueur
static inline bool isEmpty(const BitBoard& b, int row, int col) {
    int pos = idx(row, col);
    return !getBit(b.white, pos) && !getBit(b.black, pos);
}

// Génère le bitboard des cases "à portée" d'une pièce existante
static void computeInRange(const BitBoard& b, uint64_t* inRange) {
    // Reset
    for (int i = 0; i < 6; i++) inRange[i] = 0ULL;

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            int pos = idx(row, col);
            if (getBit(b.white, pos) || getBit(b.black, pos)) {
                for (int dr = -RANGE; dr <= RANGE; dr++) {
                    for (int dc = -RANGE; dc <= RANGE; dc++) {
                        int r = row + dr;
                        int c = col + dc;
                        if (r >= 0 && r < SIZE && c >= 0 && c < SIZE)
                            setBit(inRange, idx(r, c));
                    }
                }
            }
        }
    }
}

Gomoku::Gomoku() {
    for (int i = 0; i < 6; i++) {
        board.white[i] = 0ULL;
        board.black[i] = 0ULL;
    }
}

// Vérifie si une séquence de cases (donnée par ses positions) forme un free-three :
// les formes valides sont _XXX_ , _XX_X_ , _X_XX_
static int countFreeThrees(const BitBoard& board, int row, int col, Cell player) {
    static const int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};

    const uint64_t* myBits = (player == WHITE) ? board.white : board.black;
    const uint64_t* oppBits = (player == WHITE) ? board.black : board.white;

    int freeThrees = 0;

    for (int d = 0; d < 4; d++) {
        int dr = dirs[d][0];
        int dc = dirs[d][1];

        // Forme 1 : _XXX_ (fenêtre de 5, les 3 pierres sont consécutives au centre)
        // Pattern : [0]=libre [1]=X [2]=X [3]=X [4]=libre
        // On cherche toutes les positions de départ où (row,col) est l'une des cases 1,2,3
        for (int offset = 1; offset <= 3; offset++) {
            int sr = row - offset * dr;
            int sc = col - offset * dc;
            int er = sr + 4 * dr;
            int ec = sc + 4 * dc;

            if (sr < 0 || sr >= SIZE || sc < 0 || sc >= SIZE) continue;
            if (er < 0 || er >= SIZE || ec < 0 || ec >= SIZE) continue;

            // Cases [0] et [4] doivent être libres
            int p0 = idx(sr, sc);
            int p4 = idx(er, ec);
            if (getBit(myBits, p0) || getBit(oppBits, p0)) continue;
            if (getBit(myBits, p4) || getBit(oppBits, p4)) continue;

            // Cases [1],[2],[3] doivent être du joueur
            bool valid = true;
            for (int i = 1; i <= 3; i++) {
                int r = sr + i * dr;
                int c = sc + i * dc;
                if (!getBit(myBits, idx(r, c))) { valid = false; break; }
            }
            if (valid) { freeThrees++; break; } // break pour ne pas compter 2x la même ligne
        }

        // Forme 2 : _XX_X_ et _X_XX_ (fenêtre de 6, trou à l'intérieur)
        // Pattern A : [0]=libre [1]=X [2]=X [3]=libre [4]=X [5]=libre
        // Pattern B : [0]=libre [1]=X [2]=libre [3]=X [4]=X [5]=libre
        // (row,col) peut être l'une des cases X dans ces patterns
        // Positions des X dans A : 1,2,4 ; dans B : 1,3,4
        static const int patternsX[2][3] = {{1,2,4},{1,3,4}};
        static const int patternsEmpty[2][3] = {{0,3,5},{0,2,5}};

        for (int p = 0; p < 2; p++) {
            bool found = false;
            // (row,col) doit être l'une des positions X du pattern
            for (int xi = 0; xi < 3 && !found; xi++) {
                int xpos = patternsX[p][xi];
                int sr = row - xpos * dr;
                int sc = col - xpos * dc;
                int er = sr + 5 * dr;
                int ec = sc + 5 * dc;

                if (sr < 0 || sr >= SIZE || sc < 0 || sc >= SIZE) continue;
                if (er < 0 || er >= SIZE || ec < 0 || ec >= SIZE) continue;

                // Vérifier les cases X
                bool validX = true;
                for (int i = 0; i < 3 && validX; i++) {
                    int r = sr + patternsX[p][i] * dr;
                    int c = sc + patternsX[p][i] * dc;
                    if (!getBit(myBits, idx(r, c))) validX = false;
                }
                // Vérifier les cases libres
                bool validE = true;
                for (int i = 0; i < 3 && validE; i++) {
                    int r = sr + patternsEmpty[p][i] * dr;
                    int c = sc + patternsEmpty[p][i] * dc;
                    int pos = idx(r, c);
                    if (getBit(myBits, pos) || getBit(oppBits, pos)) validE = false;
                }
                if (validX && validE) { freeThrees++; found = true; }
            }
        }
    }

    return freeThrees;
}

bool Gomoku::isLegalMove(const BitBoard& board, int row, int col, Cell player) {
    return true;
    if (!isEmpty(board, row, col))
        return false;

    // On simule le coup pour évaluer les double-trois
    BitBoard tmp = board;
    int pos = idx(row, col);
    if (player == WHITE)
        setBit(tmp.white, pos);
    else
        setBit(tmp.black, pos);

    // Règle des double-trois : interdit si le coup crée >= 2 trois libres simultanément
    int freeThrees = countFreeThrees(tmp, row, col, player);
    if (freeThrees >= 2)
        return false;

    return true;
}

std::vector<Move> Gomoku::generateMoves(const BitBoard& board, const Cell player) {
    std::vector<Move> moves;
    uint64_t inRange[6] = {};

    computeInRange(board, inRange);

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            int pos = idx(row, col);
            if (isEmpty(board, row, col) && getBit(inRange, pos) && isLegalMove(board, row, col, player))
                moves.push_back({row, col, 0, 0});
        }
    }

    if (moves.empty())
        moves.push_back({SIZE / 2, SIZE / 2, 0, 0});

    return moves;
}

static int computeLineScore(const BitBoard& board, int row, int col, int dr, int dc) {
    int code = 0;
    for (int i = 0; i < 9; i++) {
        int r = row + ((i - 4) * dr);
        int c = col + ((i - 4) * dc);
        int val;
        if (r < 0 || r >= SIZE || c < 0 || c >= SIZE)
            val = 3; // hors-plateau = blockage
        else {
            int pos = idx(r, c);
            if      (getBit(board.white, pos)) val = 1;
            else if (getBit(board.black, pos)) val = 2;
            else                               val = 0;
        }
        code = code * 4 + val;
    }
    return code;
}

/*
#define GET_WHITE_CAPTURES(f)  (((f) >> 0) & 0x7)
#define GET_BLACK_CAPTURES(f)  (((f) >> 3) & 0x7)
#define GET_WHITE_THREES(f)    (((f) >> 6) & 0x7)
#define GET_BLACK_THREES(f)    (((f) >> 9) & 0x7)
*/

static void undoMove(BitBoard& board, const Move& move, Cell player) {
    int pos = idx(move.row, move.col);
    if (player == WHITE)
        clearBit(board.white, pos);
    else
        clearBit(board.black, pos);
}

int makeMove(BitBoard& board, const Move& move, Cell player) {
    static const int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    int pos = idx(move.row, move.col);
    int threesBefore = 0;
    int threesAfter = 0;
    int scoreAfter = board.score;
    int scoreBefore = board.score;
    int toRemove[16][2]; // max 8 captures (2 points chacune)
    int removeCount = 0;

    for (int di = 0; di < 4; di++) {
        int code = computeLineScore(board, move.row, move.col, dirs[di][0], dirs[di][1]);
        if (player == WHITE)
            threesBefore += GET_WHITE_THREES(evalTable[code][1]);
        else
            threesBefore += GET_BLACK_THREES(evalTable[code][1]);
        scoreBefore -= evalTable[code][0];
    }

    if (player == WHITE)
        setBit(board.white, pos);
    else
        setBit(board.black, pos);

    scoreAfter = scoreBefore;
    for (int di = 0; di < 4; di++) {
        int dr = dirs[di][0], dc = dirs[di][1];
        int code = computeLineScore(board, move.row, move.col, dr, dc);
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
    if (threesAfter - threesBefore >= 2) {
        removeCount = 0;
        undoMove(board, move, player);
        return 1;
    }

    if (removeCount > 0) {
        scoreAfter = scoreBefore;
        if (player == WHITE)
            clearBit(board.white, pos);
        else
            clearBit(board.black, pos);
        for (int i = 0; i < removeCount; i++)
        {
            for (int di = 0; di < 4; di++) {
                int code = computeLineScore(board, move.row, move.col, dirs[di][0], dirs[di][1]);
                scoreAfter -= evalTable[code][0];
            }
        }
        for (int i = 0; i < removeCount; i++)
        {
            scoreAfter += MANUAL_CAPTURE_SCORE;
            board.set(toRemove[i][0], toRemove[i][1], EMPTY);
        }
        if (player == WHITE) {
            board.whiteCaptures += static_cast<uint8_t>(removeCount);
            setBit(board.white, pos);
        }
        else {
            board.whiteCaptures += static_cast<uint8_t>(removeCount);
            setBit(board.black, pos);
        }
        for (int i = 0; i < removeCount; i++)
        {
            for (int di = 0; di < 4; di++) {
                int code = computeLineScore(board, move.row, move.col, dirs[di][0], dirs[di][1]);
                scoreAfter += evalTable[code][0];
            }
        }
        for (int di = 0; di < 4; di++) {
            int dr = dirs[di][0], dc = dirs[di][1];
            int code = computeLineScore(board, move.row, move.col, dr, dc);
            scoreAfter += evalTable[code][0];
        }
    }
    board.score = scoreAfter;
    return 0;
}

Move Gomoku::minimax(int depth, BitBoard& board, Cell player, int alpha, int beta) {
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
    
    if ((int)moves.size() > MOVE_LIMIT)
        moves.resize(MOVE_LIMIT);

    if (player == WHITE) {
        Move best = {-1, -1, std::numeric_limits<int>::min(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, WHITE) == 1)
                continue ; // coup illégal
            Move eval = minimax(depth + 1, board, opponent, alpha, beta);
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
            if (alpha >= beta)
                break; // coupure bêta
        }
        return best;
    } else {
        Move best = {-1, -1, std::numeric_limits<int>::max(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, BLACK) == 1)
                continue ; // coup illégal
            Move eval = minimax(depth + 1, board, opponent, alpha, beta);
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
            if (alpha >= beta)
                break; // coupure alpha
        }
        return best;
    }
}

Move Gomoku::minimax2(int depth, BitBoard& board, Cell player, int alpha, int beta) {
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

    // Move ordering: score chaque coup avec une évaluation shallow, puis trier
    int savedScore = board.score;
    for (Move& move : moves) {
        if (makeMove(board, move, player) == 0)
            move.score = board.score;
        else
            move.score = (player == WHITE) ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
        std::memcpy(board.black, black_board, sizeof(black_board));
        std::memcpy(board.white, white_board, sizeof(white_board));
        board.whiteCaptures = whiteCapturesBefore;
        board.blackCaptures = blackCapturesBefore;
        board.score = savedScore;
    }
    if (player == WHITE)
        std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) { return a.score > b.score; });
    else
        std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) { return a.score < b.score; });

    if ((int)moves.size() > MOVE_LIMIT)
        moves.resize(MOVE_LIMIT);

    if (player == WHITE) {
        Move best = {-1, -1, std::numeric_limits<int>::min(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, WHITE) == 1)
                continue ; // coup illégal
            Move eval = minimax(depth + 1, board, opponent, alpha, beta);
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
            if (alpha >= beta)
                break; // coupure bêta
        }
        return best;
    } else {
        Move best = {-1, -1, std::numeric_limits<int>::max(), 0};
        for (Move& move : moves) {
            int scoreBefore = board.score;
            if (makeMove(board, move, BLACK) == 1)
                continue ; // coup illégal
            Move eval = minimax(depth + 1, board, opponent, alpha, beta);
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
            if (alpha >= beta)
                break; // coupure alpha
        }
        return best;
    }
}

Move Gomoku::getBestMove(BitBoard& board, Cell player) {
    // TODO : implémenter minimax avec évaluation heuristique
  struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    Move bestMove = minimax(0, board, player, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                   (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("get_best_move: %.3f ms, move.score : %d, board.score : %d\n", elapsed, bestMove.score, board.score);
  
    return bestMove;
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
