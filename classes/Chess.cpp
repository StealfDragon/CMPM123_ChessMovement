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
#include "ChessAI.h"

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
    _ai = new ChessAI(this);

    for (int i = 0; i < 64; ++i) _boardArray[i] = 0;
    _whiteToMoveInternal = true;
}

Chess::~Chess()
{
    delete _ai;
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

    setAIPlayer(1);

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
    moves.reserve(128);

    // Choose whether to use internal board (if internal inited) or grid:
    bool useInternal = true;
    // If internal board is all zeros then fall back to grid
    bool anyPiece = false;
    for (int i=0;i<64;i++) if (_boardArray[i] != 0) { anyPiece = true; break; }
    if (!anyPiece) useInternal = false;

    auto pieceAt = [&](int idx)->int {
        if (useInternal) return _boardArray[idx];
        ChessSquare* s = _grid->getSquareByIndex(idx);
        if (!s) return 0;
        Bit* b = s->bit();
        return b ? b->gameTag() : 0;
    };

    auto pushMove = [&](int from, int to) {
        Move m;
        m.startSquare = from;
        m.targetSquare = to;
        moves.push_back(m);
    };

    // Iterate board squares
    for (int sq = 0; sq < 64; ++sq)
    {
        int tag = pieceAt(sq);
        if (tag == 0) continue;
        bool white = tag < 128;
        int pid = tag % 128;
        // Respect side-to-move: if using internal board, consult _whiteToMoveInternal
        if (useInternal) {
            if (_whiteToMoveInternal != white) continue;
        } else {
            Player* cur = getCurrentPlayer();
            if (cur) {
                bool whiteToMove = cur->playerNumber() == 0;
                if (whiteToMove != white) continue;
            }
        }

        int file = SquareFile(sq);
        int rank = SquareRank(sq);

        auto isFriendly = [&](int idx)->bool {
            int t = pieceAt(idx);
            if (t == 0) return false;
            return (t < 128) == white;
        };
        auto isEnemy = [&](int idx)->bool {
            int t = pieceAt(idx);
            if (t == 0) return false;
            return (t < 128) != white;
        };

        // Pawn
        if (pid == Pawn) {
            if (white) {
                int one = sq + 8;
                if (SquareValid(one) && pieceAt(one) == 0) {
                    pushMove(sq, one);
                    // double from rank 1
                    if (rank == 1) {
                        int two = sq + 16;
                        if (SquareValid(two) && pieceAt(two) == 0) pushMove(sq, two);
                    }
                }
                // captures
                if (file > 0) { int c = sq + 7; if (SquareValid(c) && isEnemy(c)) pushMove(sq, c); }
                if (file < 7) { int c = sq + 9; if (SquareValid(c) && isEnemy(c)) pushMove(sq, c); }
            } else { // black
                int one = sq - 8;
                if (SquareValid(one) && pieceAt(one) == 0) {
                    pushMove(sq, one);
                    if (rank == 6) {
                        int two = sq - 16;
                        if (SquareValid(two) && pieceAt(two) == 0) pushMove(sq, two);
                    }
                }
                if (file > 0) { int c = sq - 9; if (SquareValid(c) && isEnemy(c)) pushMove(sq, c); }
                if (file < 7) { int c = sq - 7; if (SquareValid(c) && isEnemy(c)) pushMove(sq, c); }
            }
        }
        // Knight
        else if (pid == Knight) {
            constexpr int kNOff[8] = { +17, +15, +10, +6, -6, -10, -15, -17 };
            for (int off : kNOff) {
                int to = sq + off;
                if (!SquareValid(to)) continue;
                int tf = SquareFile(to);
                if (std::abs(file - tf) > 2) continue;
                if (isFriendly(to)) continue;
                pushMove(sq, to);
            }
        }
        // King
        else if (pid == King) {
            constexpr int kKOff[8] = { +1, -1, +8, -8, +9, +7, -9, -7 };
            for (int off : kKOff) {
                int to = sq + off;
                if (!SquareValid(to)) continue;
                int tf = SquareFile(to);
                if (std::abs(file - tf) > 1) continue;
                if (isFriendly(to)) continue;
                pushMove(sq, to);
            }
        }
        // Bishop / Rook / Queen (ray attacks)
        else {
            auto ray = [&](int dir) {
                int cur = sq;
                while (true) {
                    int f = SquareFile(cur);
                    int r = SquareRank(cur);
                    int n = cur + dir;
                    if (!SquareValid(n)) break;
                    // file wrap checks for left/right moves
                    int nf = SquareFile(n);
                    if (std::abs(nf - f) > 2 && (dir == 1 || dir == -1 || dir == 7 || dir == -9 || dir == 9 || dir == -7)) {
                        // wrap; break (defensive)
                        break;
                    }
                    if (isFriendly(n)) break;
                    pushMove(sq, n);
                    if (isEnemy(n)) break;
                    cur = n;
                }
            };
            if (pid == Bishop || pid == Queen) {
                // bishop directions: +9, +7, -7, -9 (on 0..63)
                ray(+9); ray(+7); ray(-7); ray(-9);
            }
            if (pid == Rook || pid == Queen) {
                // rook directions: +1, -1, +8, -8
                ray(+1); ray(-1); ray(+8); ray(-8);
            }
        }
    }

    return moves;
}

void Chess::buildInternalBoardFromGrid()
{
    for (int i = 0; i < 64; ++i)
        _boardArray[i] = 0;

    _grid->forEachSquare([&](ChessSquare* square, int, int) {
        int idx = square->getSquareIndex();
        if (square->bit())
            _boardArray[idx] = square->bit()->gameTag();
    });

    Player* cur = getCurrentPlayer();
    _whiteToMoveInternal = (cur && cur->playerNumber() == 0);
}

// sync internal board back to the grid (after AI chooses move)
void Chess::syncGridFromInternalBoard()
{
    for (int i = 0; i < 64; ++i)
    {
        ChessSquare* sq = _grid->getSquareByIndex(i);
        int tag = _boardArray[i];
        // Destroy whatever is in square and replace with correct piece
        sq->destroyBit();
        if (tag != 0)
        {
            // FIX START
            // Tag < 128 is White (Player 0). Tag >= 128 is Black (Player 1).
            int playerNumber = tag < 128 ? 0 : 1; 
            int pieceId = tag % 128;
            
            // Pass playerNumber, not "isWhite" boolean logic
            Bit* b = PieceForPlayer(playerNumber, (ChessPiece)pieceId);
            // FIX END
            
            sq->setBit(b);
            b->setParent(sq);
            b->moveTo(sq->getPosition());
            b->setPickedUp(false);
            b->setGameTag(tag);
        }
    }
}

int Chess::applyMoveToInternalBoard(const Move &m)
{
    int from = m.startSquare;
    int to   = m.targetSquare;

    int captured = _boardArray[to];
    _boardArray[to]   = _boardArray[from];
    _boardArray[from] = 0;
    
    // FIX: Toggle the internal turn so the next depth of recursion 
    // generates moves for the opponent.
    _whiteToMoveInternal = !_whiteToMoveInternal; 

    return captured;
}

void Chess::undoMoveInInternalBoard(const Move &m, int captured)
{
    int from = m.startSquare;
    int to   = m.targetSquare;

    _boardArray[from] = _boardArray[to];
    _boardArray[to]   = captured;

    // FIX: Toggle it back when undoing
    _whiteToMoveInternal = !_whiteToMoveInternal;
}

int Chess::materialScore()
{
    int whiteScore = 0;
    int blackScore = 0;
    for (int i = 0; i < 64; ++i)
    {
        int tag = _boardArray[i];
        if (tag == 0) continue;
        bool white = tag < 128;
        int id = tag % 128;
        int val = 0;
        switch (id) {
            case Pawn: val = VAL_PAWN; break;
            case Knight: val = VAL_KNIGHT; break;
            case Bishop: val = VAL_BISHOP; break;
            case Rook: val = VAL_ROOK; break;
            case Queen: val = VAL_QUEEN; break;
            case King: val = VAL_KING; break;
            default: val = 0; break;
        }
        if (white) whiteScore += val; else blackScore += val;
    }
    return whiteScore - blackScore;
}

void Chess::makeAIMove(int depth)
{
    if (!_ai) return;

    // 1. Check turn and player type
    Player* cur = getCurrentPlayer();
    if (!cur || !cur->isAIPlayer()) return;

    // 2. Sync internal board so AI thinks based on current reality
    buildInternalBoardFromGrid();

    // 3. Find the best move
    Move bestMove = _ai->findBestMove(depth);

    // Safety: If AI returns invalid move, abort
    if (bestMove.startSquare < 0 || bestMove.targetSquare < 0) return;

    // 4. Update Internal State (for logic)
    applyMoveToInternalBoard(bestMove);

    // 5. Update Visuals (The "Vanish" Fix)
    ChessSquare* startSq = _grid->getSquareByIndex(bestMove.startSquare);
    ChessSquare* endSq   = _grid->getSquareByIndex(bestMove.targetSquare);

    // Safety check: ensure start square actually has a piece
    Bit* piece = startSq->bit();
    if (piece) {
        // A. Handle Capture (Destroy enemy piece if present)
        if (endSq->bit()) {
            endSq->destroyBit();
        }

        // B. CRITICAL: Move the piece visually *before* changing its parent.
        // This ensures the move uses the piece's current world coordinate context.
        piece->moveTo(endSq->getPosition());
        
        // C. Update the Transform/Parenting
        piece->setParent(endSq);
        
        // D. Update the Logical Pointers (Who holds what)
        startSq->setBit(nullptr);
        endSq->setBit(piece);

        // E. Ensure the engine knows the piece is "dropped" and valid
        piece->setPickedUp(false);
    }
    
    // 6. End the turn
    endTurn();
}

bool Chess::gameHasAI()
{
	return _gameOptions.AIPlaying;
}