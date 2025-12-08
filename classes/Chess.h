#pragma once

#include "Game.h"
#include "Grid.h"
#include <vector>
#include "ChessSquare.h"

class ChessAI;

constexpr int pieceSize = 80;

enum ChessPiece
{
    NoPiece,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

constexpr int VAL_PAWN   = 100;
constexpr int VAL_KNIGHT = 320;
constexpr int VAL_BISHOP = 330;
constexpr int VAL_ROOK   = 500;
constexpr int VAL_QUEEN  = 900;
constexpr int VAL_KING   = 20000;

struct Move
{
    public: 
        int startSquare;
        int targetSquare;
        int capturedPiece; // store captured piece id here

        Move() : startSquare(-1), targetSquare(-1), capturedPiece(0) {}
        Move(int s, int t) : startSquare(s), targetSquare(t), capturedPiece(0) {}
};

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

    std::vector<Move> moves;
    std::vector<Move> GenerateMoves();

    int applyMoveToInternalBoard(const Move &m);
    void undoMoveInInternalBoard(const Move &m, int captured);

    int materialScore();
    bool isWhiteToMove() const { return _whiteToMoveInternal; }

    void makeAIMove(int depth = 3);

    bool gameHasAI() override;

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    int _boardArray[64];
    bool _whiteToMoveInternal;

    void buildInternalBoardFromGrid();
    void syncGridFromInternalBoard();

    Grid* _grid;

    ChessAI* _ai = nullptr;
};