#include "Gomoku.hpp"
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

static int evaluate(const BitBoard& board, Cell player) {
    // TODO: évaluation heuristique
    (void) board; (void) player;
    return 0;
}

Move Gomoku::getBestMove(const BitBoard& board, Cell player) {
    // TODO : implémenter minimax avec évaluation heuristique
    (void) board; (void) player;
    return {SIZE / 2, SIZE / 2, 0, 0};
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

static void makeMove(BitBoard& board, const Move& move, Cell player) {
    int pos = idx(move.row, move.col);
//ici


    if (player == WHITE)
        setBit(board.white, pos);
    else
        setBit(board.black, pos);
}

static void undoMove(BitBoard& board, const Move& move, Cell player) {
    int pos = idx(move.row, move.col);
    if (player == WHITE)
        clearBit(board.white, pos);
    else
        clearBit(board.black, pos);
}

Move Gomoku::minimax(int depth, BitBoard& board, Cell player) {
    if (depth > DEPTH_LIMIT)
        return {-1, -1, evaluate(board, player), 0};

    std::vector<Move> moves = generateMoves(board, player);
    Cell opponent = (player == WHITE) ? BLACK : WHITE;

    if (player == WHITE) {
        Move best = {-1, -1, std::numeric_limits<int>::min(), 0};
        for (const Move& move : moves) {
            makeMove(board, move, WHITE);
            Move eval = minimax(depth + 1, board, opponent);
            undoMove(board, move, WHITE);
            if (eval.score > best.score) {
                best = {move.row, move.col, eval.score, 0};
            }
        }
        return best;
    } else {
        Move best = {-1, -1, std::numeric_limits<int>::max(), 0};
        for (const Move& move : moves) {
            makeMove(board, move, BLACK);
            Move eval = minimax(depth + 1, board, opponent);
            undoMove(board, move, BLACK);
            if (eval.score < best.score) {
                best = {move.row, move.col, eval.score, 0};
            }
        }
        return best;
    }
}