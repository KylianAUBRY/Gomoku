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

/* Suggestion d'opti V2


Pourquoi la conception actuelle est déconseillée
La malédiction du 19x19 dans un registre 64-bits :
Calculer index = y * 19 + x dans un grand sac de bits
linéaire avec state[index / 64] et index % 64 est non
seulement lourd à la lecture, mais c'est un format où les
 lignes de votre plateau (19 bits) se chevauchent sur les
  octets du système (64 bits).
Le potentiel du Bitboard est gâché Dans votre fichier Rules.cpp,
vous vérifiez les victoires avec des boucles imbriquées
for(x) for(y) suivies d'une boucle while (board.get_bit(x, y)
) pour compter les pierres unes à unes. Règle d'or de l'optimisation
: Si l'on itère sur des bits un par un comme dans un tableau normal,
un "Bitboard" est inutile. Autant utiliser un tableau normal comme
uint8_t board[19 * 19];, ce sera 10 fois plus lisible, et le
processeur mettra autant ou moins de temps pour lire l'endroit
en O(1) (pas de modulo % 64 ni divisions ni décalages binaires
à calculer).
Comment structurer votre code (Les Alternatives)
L'IA requiert une performance brute exceptionnelle si
on veut chercher à la profondeur 10. Voici deux manières
d'aborder votre structure de données.


Option 1 : La lisibilité avec la structure par ligne (Le Bitboard Intuitif)
Si vous voulez allier manipulations bit-à-bit (pour que ce soit rapide) ET
lisibilité, abandonnez le tableau global et considérez le fait que 19 bits
rentrent naturellement dans un type de base (uint32_t). L'astuce est de donner
un registre par ligne :

struct Bitboard {
    // 1 ligne = 1 registre 32-bits (dont seuls 19 bits sont utilisés)
    std::array<uint32_t, 19> rows = {0};

    inline void set_bit(int x, int y) {
        rows[y] |= (1U << x);
    }
    inline void clear_bit(int x, int y) {
        rows[y] &= ~(1U << x);
    }
    inline bool get_bit(int x, int y) const {
        return (rows[y] >> x) & 1U;
    }
};

*/