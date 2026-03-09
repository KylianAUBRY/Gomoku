#pragma once

#include <cstdint>

// 19x19 board requires 361 bits.
// We use an array of 6 uint64_t to store 384 bits per board.

struct Bitboard {
    uint64_t state[6] = {0};

    // Calculate the integer index (0-5) and bit index (0-63)
    // for a given (x,y) where x in [0, 18] and y in [0, 18].
    // Index = y * 19 + x
    
    inline void set_bit(int x, int y) {
        int index = y * 19 + x;
        state[index / 64] |= (1ULL << (index % 64));
    }

    inline void clear_bit(int x, int y) {
        int index = y * 19 + x;
        state[index / 64] &= ~(1ULL << (index % 64));
    }

    inline bool get_bit(int x, int y) const {
        int index = y * 19 + x;
        return (state[index / 64] >> (index % 64)) & 1ULL;
    }
};
