/* #pragma once

#include "Game.h"
#include "Grid.h"
#include <vector>

inline bool isWhitePiece(const Bit* bit) { return bit->gameTag() < 128; }
inline bool isBlackPiece(const Bit* bit) { return bit->gameTag() >= 128; }

inline int sqIndex(ChessSquare* sq) { return sq->getSquareIndex(); }
inline uint64_t sqMask(int idx) { return 1ULL << idx; }

// clamp: square must remain on board
inline bool validSquare(int sq) { return sq >= 0 && sq < 64; } */