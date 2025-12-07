#pragma once

#include "Game.h"
#include "Grid.h"
#include <vector>
#include "ChessSquare.h"

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

struct Move
{
    public: 
        int startSquare; // might have to add const later
        int targetSquare;

        Move() : startSquare(-1), targetSquare(-1) {}
        Move(int s, int t) : startSquare(s), targetSquare(t) {}
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


private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    Grid* _grid;
};