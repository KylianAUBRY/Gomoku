#include "engine/GameState.hpp"
#include "engine/Gomoku.hpp"
#include "front/GameUI.hpp"
#include <stdio.h>
#include <time.h>

int evalTable[262144]; // définition uniques

typedef struct {
  int pattern[7];
  int length;
  int score;
} Pattern;

static Pattern patterns_white[] = {
    {{1, 1, 1, 1, 1}, 5, 1000000},
    {{0, 1, 1, 1, 1, 0}, 6, 100000},
    {{1, 0, 1, 1, 1, 0}, 6, 50000},
    {{0, 1, 1, 1, 0, 1}, 6, 50000},
    {{0, 1, 1, 0, 1, 1, 0}, 7, 40000},
    {{3, 1, 1, 1, 0, 1}, 6, 10000},
    {{1, 0, 1, 1, 1, 3}, 6, 10000},
    {{3, 1, 1, 1, 1, 0}, 6, 10000},
    {{0, 1, 1, 1, 1, 3}, 6, 10000},
    {{0, 1, 1, 1, 0}, 5, 5000},
    {{0, 1, 0, 1, 1, 0}, 6, 3000},
    {{0, 1, 1, 0, 1, 0}, 6, 3000},
    {{3, 1, 1, 1, 0}, 5, 1000},
    {{0, 1, 1, 1, 3}, 5, 1000},
    {{3, 1, 1, 0, 1, 0}, 6, 500},
    {{0, 1, 0, 1, 1, 3}, 6, 500},
    {{0, 1, 1, 0}, 4, 300},
    {{0, 1, 0, 1, 0}, 5, 300},
    {{3, 1, 1, 0}, 4, 80},
    {{0, 1, 1, 3}, 4, 80},
    {{3, 1, 0, 1, 0}, 5, 40},
    {{0, 1, 0, 1, 3}, 5, 40},
    {{0, 1, 0}, 3, 20},
    {{3, 1, 0}, 3, 10},
    {{0, 1, 3}, 3, 10},
    {{3, 1, 1, 3}, 4, 10},
    {{3, 1, 1, 1, 3}, 5, 50},
    {{3, 1, 1, 1, 1, 3}, 6, 100},
};
static Pattern patterns_black[] = {
    {{2, 2, 2, 2, 2}, 5, -1000000},
    {{0, 2, 2, 2, 2, 0}, 6, -100000},
    {{2, 0, 2, 2, 2, 0}, 6, -50000},
    {{0, 2, 2, 2, 0, 2}, 6, -50000},
    {{0, 2, 2, 0, 2, 2, 0}, 7, -40000},
    {{3, 2, 2, 2, 0, 2}, 6, -10000},
    {{2, 0, 2, 2, 2, 3}, 6, -10000},
    {{3, 2, 2, 2, 2, 0}, 6, -10000},
    {{0, 2, 2, 2, 2, 3}, 6, -10000},
    {{0, 2, 2, 2, 0}, 5, -5000},
    {{0, 2, 0, 2, 2, 0}, 6, -3000},
    {{0, 2, 2, 0, 2, 0}, 6, -3000},
    {{3, 2, 2, 2, 0}, 5, -1000},
    {{0, 2, 2, 2, 3}, 5, -1000},
    {{3, 2, 2, 0, 2, 0}, 6, -500},
    {{0, 2, 0, 2, 2, 3}, 6, -500},
    {{0, 2, 2, 0}, 4, -300},
    {{0, 2, 0, 2, 0}, 5, -300},
    {{3, 2, 2, 0}, 4, -80},
    {{0, 2, 2, 3}, 4, -80},
    {{3, 2, 0, 2, 0}, 5, -40},
    {{0, 2, 0, 2, 3}, 5, -40},
    {{0, 2, 0}, 3, -20},
    {{3, 2, 0}, 3, -10},
    {{0, 2, 3}, 3, -10},
    {{3, 2, 2, 3}, 4, -10},
    {{3, 2, 2, 2, 3}, 5, -50},
    {{3, 2, 2, 2, 2, 3}, 6, -100},
};
/*
0 = vide
1 = blanc
2 = noir
3 = blockage

11111  = 1000000
011110 = 100000
101110  = 50000
011101  = 50000
0110110 = 40000
311101 = 10000
101113 = 10000
311110 = 10000
011113 = 10000
01110  = 5000
010110 = 3000
011010 = 3000
31110  = 1000
01113  = 1000
311010 = 500
010113 = 500
0110  = 300
01010 = 300
3110  = 80
0113  = 80
31010 = 40
01013 = 40
010   = 15
3113  = 10
31113 = 50
311113 = 100
*/

void decode4(int code, int *cells) {
  for (int i = 0; i < 9; i++) {
    cells[i] = code % 4;
    code /= 4;
  }
}

int encode4(const int *cells, int length) {
  int code = 0;
  for (int i = length - 1; i >= 0; i--) {
    code = code * 4 + cells[i];
  }
  return code;
}

static int numPatterns_white =
    sizeof(patterns_white) / sizeof(patterns_white[0]);
static int numPatterns_black =
    sizeof(patterns_black) / sizeof(patterns_black[0]);

void initEvalTable() {
  for (int i = 0; i < 262144; i++) {
    int line[9];
    decode4(i, line);
    int score = 0;

    for (int p = 0; p < numPatterns_white; p++) {
      int plen = patterns_white[p].length;
      for (int start = 0; start <= 9 - plen; start++) {
        int match = 1;
        for (int k = 0; k < plen; k++) {
          if (line[start + k] != patterns_white[p].pattern[k]) {
            if (line[start + k] == 2 &&
                patterns_white[p].pattern[k] == 3) // Noir bloque
              continue;
            match = 0;
            break;
          }
        }
        if (match)
          score += patterns_white[p].score;
      }
    }
    for (int p = 0; p < numPatterns_black; p++) {
      int plen = patterns_black[p].length;
      for (int start = 0; start <= 9 - plen; start++) {
        int match = 1;
        for (int k = 0; k < plen; k++) {
          if (line[start + k] != patterns_black[p].pattern[k]) {
            if (line[start + k] == 1 &&
                patterns_black[p].pattern[k] == 3) // Blanc bloque
              continue;
            match = 0;
            break;
          }
        }
        if (match)
          score += patterns_black[p].score;
      }
    }
    // ici seron traiter les paterne exeptionelle

    evalTable[i] = score;
  }
}

int main() {
  initEvalTable();


  Gomoku game;
  GameState state;
  GameUI ui;

  ui.run(state, game);

  return 0;
}
