#pragma once
#include <vector>
#include <cstdint>
#include "Bitboard.h"

struct Move;

class Chess;

class ChessAI {
public:
    ChessAI(Chess* game);

    Move findBestMove(int depth);

    int evaluateBoard();

    void setSearchDepth(int d) { _searchDepth = d; }

private:
    Chess* _game;
    int _searchDepth;

    int negamax(int depth, int alpha, int beta, bool maximizingPlayer);
    int evaluateMaterial() const;
    int evaluateMobility() const;
};