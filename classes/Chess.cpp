#include "Chess.h"
#include <limits>
#include <cmath>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include "Bitboard.h"
#include "ChessHelpers.h"
#include <cstdint>

// Helper: mask for a square index
inline uint64_t SquareMask(int idx) { return 1ULL << idx; }

// Helpers: get file and rank of square index (0..63)
inline int SquareFile(int sq) { return sq % 8; }
inline int SquareRank(int sq) { return sq / 8; }
inline bool SquareValid(int sq) { return sq >= 0 && sq < 64; }

// pop least-significant bit from uint64_t and return its index (-1 if zero)
static inline int pop_lsb(uint64_t &bb) {
    if (bb == 0) return -1;
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    bb &= bb - 1;
    return (int)idx;
#else
    int idx = __builtin_ctzll(bb);
    bb &= bb - 1;
    return idx;
#endif
}

// Convert your BitboardElement to raw uint64_t quickly
static inline uint64_t bb_get(const BitBoard &b) { return b.getData(); }
static inline void bb_set(BitBoard &b, uint64_t v) { b.setData(v); }

// Test "is white" for a Bit (gameTag < 128 = white)
static inline bool isWhitePiece(const Bit* bit) { return bit->gameTag() < 128; }
static inline bool isBlackPiece(const Bit* bit) { return bit->gameTag() >= 128; }

// Helper to convert a ChessSquare* to index 0..63
static inline int squareIndexFromSquare(const ChessSquare* sq) {
    if (!sq) return -1;

    return sq->getSquareIndex();
}

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);
    bit->setGameTag((playerNumber == 0 ? 0 : 128) + piece);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    startGame();
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)

    std::stringstream fenS(fen);
    std::string row;
    std::vector <std::string> board;
    while(std::getline(fenS, row, '/')) {
        board.push_back(row);
    }

    std::string pieces = "0pnbrqk";
    for(int i = 0; i < board.size(); i++) {
        int x = 0; // x position in the actual grid
        for(int j = 0; j < board[i].size(); j++) {
            int a = board[i][j];
            if(isdigit(a)) {
                x += (a - '0');
            }
            else {
                bool isWhite = isupper(a);
                char lower = tolower(a);
                int p = pieces.find(lower);
                int y = 7 - i;
                ChessPiece piece = ChessPiece(p);
                Bit* bit = PieceForPlayer(isWhite, piece);
                ChessSquare* sq = _grid->getSquare(x, i);
                sq->setBit(bit);
                bit->setParent(sq);       
                bit->moveTo(sq->getPosition()); 
                bit->setPickedUp(false); 
                x++;
            }
        }
    }
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    Player* cur = getCurrentPlayer();
    if (!cur) return false;
    int curPlayerNumber = cur->playerNumber(); // 0 or 1

    bool pieceIsWhite = bit.gameTag() < 128;
    if (curPlayerNumber == 0 && !pieceIsWhite) return false; // white to move, piece must be white
    if (curPlayerNumber == 1 && pieceIsWhite)  return false; // black to move, piece must be black

    return true;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* fromSq = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* toSq   = dynamic_cast<ChessSquare*>(&dst);
    if (!fromSq || !toSq) return false;

    // Build the full move list for the side to move, then check membership.
    auto allMoves = GenerateMoves();

    int fromIndex = fromSq->getSquareIndex();
    int toIndex = toSq->getSquareIndex();

    for (const Move &m : allMoves) {
        if (m.startSquare == fromIndex && m.targetSquare == toIndex) return true;
    }
    return false;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}

std::vector<Move> Chess::GenerateMoves() {
    std::vector<Move> moves;
    moves.reserve(64);

    // Build occupancy bitboards from the grid
    BitBoard whiteBB(0), blackBB(0), allBB(0);

    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        Bit* b = square->bit();
        if (!b) return;
        int idx = square->getSquareIndex();
        if (b->gameTag() < 128) {
            whiteBB |= SquareMask(idx);
        } else {
            blackBB |= SquareMask(idx);
        }
        allBB |= SquareMask(idx);
    });

    // Which side to move? player 0 -> white, player 1 -> black
    Player* cur = getCurrentPlayer();
    if (!cur) return moves;
    bool whiteToMove = (cur->playerNumber() == 0);

    // iterate all squares; generate moves only for pieces of the moving side
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        Bit* b = square->bit();
        if (!b) return;

        bool pieceIsWhite = b->gameTag() < 128;
        if (whiteToMove != pieceIsWhite) return; // skip opponent pieces

        int fromIdx = square->getSquareIndex();
        int fromFile = SquareFile(fromIdx);
        int fromRank = SquareRank(fromIdx);
        int tag = b->gameTag() % 128; // piece id (1..6) ignoring color

        uint64_t wmask = bb_get(whiteBB);
        uint64_t bmask = bb_get(blackBB);
        uint64_t omask = bb_get(allBB);

        // Helper lambda to push move only if destination not occupied by friendly
        auto tryAddMove = [&](int toIdx) {
            if (!SquareValid(toIdx)) return;
            uint64_t m = SquareMask(toIdx);
            // friendly piece block?
            if (pieceIsWhite) {
                if (wmask & m) return;
            } else {
                if (bmask & m) return;
            }
            moves.emplace_back(fromIdx, toIdx);
        };

        // Pawn
        if (tag == Pawn) {
            if (pieceIsWhite) {
                // single push
                int to = fromIdx + 8;
                if (SquareValid(to) && !(omask & SquareMask(to))) {
                    moves.emplace_back(fromIdx, to);
                    // double push from rank 2 (rank index 1)
                    if (fromRank == 1) {
                        int to2 = fromIdx + 16;
                        if (SquareValid(to2) && !(omask & SquareMask(to2))) {
                            moves.emplace_back(fromIdx, to2);
                        }
                    }
                }
                // captures
                if (fromFile > 0) {
                    int cap = fromIdx + 7;
                    if (SquareValid(cap) && (bmask & SquareMask(cap))) moves.emplace_back(fromIdx, cap);
                }
                if (fromFile < 7) {
                    int cap = fromIdx + 9;
                    if (SquareValid(cap) && (bmask & SquareMask(cap))) moves.emplace_back(fromIdx, cap);
                }
            } else { // black pawn
                int to = fromIdx - 8;
                if (SquareValid(to) && !(omask & SquareMask(to))) {
                    moves.emplace_back(fromIdx, to);
                    // double push from rank 7 (rank index 6)
                    if (fromRank == 6) {
                        int to2 = fromIdx - 16;
                        if (SquareValid(to2) && !(omask & SquareMask(to2))) {
                            moves.emplace_back(fromIdx, to2);
                        }
                    }
                }
                // captures
                if (fromFile > 0) {
                    int cap = fromIdx - 9;
                    if (SquareValid(cap) && (wmask & SquareMask(cap))) moves.emplace_back(fromIdx, cap);
                }
                if (fromFile < 7) {
                    int cap = fromIdx - 7;
                    if (SquareValid(cap) && (wmask & SquareMask(cap))) moves.emplace_back(fromIdx, cap);
                }
            }
        }
        // Knight
        else if (tag == Knight) {
            constexpr int kNOff[8] = { +17, +15, +10, +6, -6, -10, -15, -17 };
            for (int off : kNOff) {
                int to = fromIdx + off;
                if (!SquareValid(to)) continue;
                // file-check: valid knight moves never change file by more than 2
                int tf = SquareFile(to);
                if (std::abs(fromFile - tf) > 2) continue;
                // don't capture friendly
                if (pieceIsWhite) {
                    if (wmask & SquareMask(to)) continue;
                } else {
                    if (bmask & SquareMask(to)) continue;
                }
                moves.emplace_back(fromIdx, to);
            }
        }
        // King
        else if (tag == King) {
            constexpr int kKOff[8] = { +1, -1, +8, -8, +9, +7, -9, -7 };
            for (int off : kKOff) {
                int to = fromIdx + off;
                if (!SquareValid(to)) continue;
                // king cannot wrap files more than 1
                int tf = SquareFile(to);
                if (std::abs(fromFile - tf) > 1) continue;
                // block by friendly
                if (pieceIsWhite) {
                    if (wmask & SquareMask(to)) continue;
                } else {
                    if (bmask & SquareMask(to)) continue;
                }
                moves.emplace_back(fromIdx, to);
            }
        }

    });
    return moves;
}