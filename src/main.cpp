#include "engine/GameState.hpp"
#include "front/GameUI.hpp"
#include "engine/Gomoku.hpp"
#include <time.h>
#include <stdio.h>

int16_t evalTable[19683]; // définition uniques

typedef struct {
    int pattern[7];
    int length;
    int score;
} Pattern;

static Pattern patterns[] = {
    {{1,1,1,1,1},       5, 1000000},
    {{0,1,1,1,1,0},     6, 100000},
    {{1,0,1,1,1,0},     6, 50000},
    {{0,1,1,1,0,1},     6, 50000},
    {{0,1,1,0,1,1,0},   7, 40000},
    {{2,1,1,1,0,1},     6, 10000},
    {{1,0,1,1,1,2},     6, 10000},
    {{2,1,1,1,1,0},     6, 10000},
    {{0,1,1,1,1,2},     6, 10000},
    {{0,1,1,1,0},       5, 5000},
    {{0,1,0,1,1,0},     6, 2000},
    {{0,1,1,0,1,0},     6, 2000},
    {{2,1,1,1,0},       5, 1000},
    {{0,1,1,1,2},       5, 1000},
    {{2,1,1,0,1,0},     6, 500},
    {{0,1,0,1,1,2},     6, 500},
    {{0,1,1,0},         4, 300},
    {{0,1,0,1,0},       5, 200},
    {{2,1,1,0},         4, 80},
    {{0,1,1,2},         4, 80},
    {{2,1,0,1,0},       5, 40},
    {{0,1,0,1,2},       5, 40},
    {{0,1,0},           3, 15},
    {{2,1,1,2},         4, 10},
    {{2,1,1,1,2},       5, 50},
    {{2,1,1,1,1,2},     6, 100},
};
/*
0 = vide
1 = joueur
2 = adversaire ou hors du plateau

11111  = 1000000
011110 = 100000
101110  = 50000
011101  = 50000
0110110 = 40000
211101 = 10000
101112 = 10000
211110 = 10000
011112 = 10000
01110  = 5000
010110 = 2000
011010 = 2000
21110  = 1000
01112  = 1000
211010 = 500
010112 = 500
0110  = 300
01010 = 200
2110  = 80
0112  = 80
21010 = 40
01012 = 40
010   = 15
2112  = 10
21112 = 50
211112 = 100
*/

void decode3(int code, int* cells) {
    for (int i = 0; i < 9; i++) {
        cells[i] = code % 3;
        code /= 3;
    }
}

int encode3(const int* cells, int length) {
    int code = 0;
    for (int i = length - 1; i >= 0; i--) {
        code = code * 3 + cells[i];
    }
    return code;
}

static int numPatterns = sizeof(patterns) / sizeof(patterns[0]);

void initEvalTable() {
    for (int i = 0; i < 19683; i++) {
        int line[9];
        decode3(i, line);
        int score = 0;

        for (int p = 0; p < numPatterns; p++) {
            int plen = patterns[p].length;
            for (int start = 0; start <= 9 - plen; start++) {
                int match = 1;
                for (int k = 0; k < plen; k++) {
                    if (line[start + k] != patterns[p].pattern[k]) {
                        match = 0;
                        break;
                    }
                }
                if (match)
                    score += patterns[p].score;
            }
        }
        evalTable[i] = (int16_t)score;
    }
}

int main() {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    initEvalTable();
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("initEvalTable: %.3f ms\n", elapsed);

    Gomoku game;
    GameState state;
    GameUI ui;

    ui.run(state);

    return 0;
}
