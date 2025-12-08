#include "ChessAI.h"
#include "Chess.h"
#include <limits>
#include <algorithm>

/* // piece values (tunable)
static constexpr int VAL_PAWN = 100;
static constexpr int VAL_KNIGHT = 320;
static constexpr int VAL_BISHOP = 330;
static constexpr int VAL_ROOK = 500;
static constexpr int VAL_QUEEN = 900;
static constexpr int VAL_KING = 20000; */

ChessAI::ChessAI(Chess* game)
    : _game(game), _searchDepth(3)
{
}

Move ChessAI::findBestMove(int depth)
{
    _searchDepth = depth;
    std::vector<Move> moves = _game->GenerateMoves();
    if (moves.empty()) return Move();

    int bestScore = std::numeric_limits<int>::min();
    Move best = moves[0];

    for (const Move &m : moves)
    {
        int captured = _game->applyMoveToInternalBoard(m);
        int score = -negamax(_searchDepth - 1, std::numeric_limits<int>::min()/2, std::numeric_limits<int>::max()/2, false);
        _game->undoMoveInInternalBoard(m, captured);
        if (score > bestScore)
        {
            bestScore = score;
            best = m;
        }
    }
    return best;
}

int ChessAI::negamax(int depth, int alpha, int beta, bool maximizingPlayer)
{
    if (depth == 0)
    {
        return evaluateBoard();
    }

    std::vector<Move> moves = _game->GenerateMoves();
    if (moves.empty())
    {
        return 0;
    }

    int best = std::numeric_limits<int>::min();

    for (const Move &m : moves)
    {
        int captured = _game->applyMoveToInternalBoard(m);
        int val = -negamax(depth - 1, -beta, -alpha, !maximizingPlayer);
        _game->undoMoveInInternalBoard(m, captured);

        if (val > best) best = val;
        if (val > alpha) alpha = val;
        if (alpha >= beta) break;
    }
    return best;
}

int ChessAI::evaluateBoard()
{
    // simple material + mobility
    int material = evaluateMaterial();
    int mobility = evaluateMobility();
    return material + mobility;
}

int ChessAI::evaluateMaterial() const
{
    return _game->materialScore();
}

int ChessAI::evaluateMobility() const
{
    std::vector<Move> moves = _game->GenerateMoves();
    int mcount = (int)moves.size();
    return mcount * 2; 
}