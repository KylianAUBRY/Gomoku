#include "Gomoku.hpp"
#include <climits>
#include <vector>

// Bitboard helpers: 361 bits = 6 x uint64_t (bits 0..360, bit 361..383 inutilisés)
static constexpr int TOTAL_BITS = SIZE * SIZE; // 361

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
    return 0;
}

Move Gomoku::getBestMove(const BitBoard& board, Cell player) {
    // TODO : implémenter minimax avec évaluation heuristique
    return {SIZE / 2, SIZE / 2, 0, 0};
}

bool Gomoku::isLegalMoove(const BitBoard& board, int row, int col, Cell player) {
    // TODO: règles de capture, double-trois, etc.
    return isEmpty(board, row, col);
}

std::vector<Move> Gomoku::generateMoves(const BitBoard& board, const Cell player) {
    std::vector<Move> moves;
    uint64_t inRange[6] = {};

    computeInRange(board, inRange);

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            int pos = idx(row, col);
            if (isEmpty(board, row, col) && getBit(inRange, pos) && isLegalMoove(board, row, col, player))
                moves.push_back({row, col, 0, 0});
        }
    }

    if (moves.empty())
        moves.push_back({SIZE / 2, SIZE / 2, 0, 0});

    return moves;
}

static void makeMove(BitBoard& board, const Move& move, Cell player) {
    int pos = idx(move.row, move.col);
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