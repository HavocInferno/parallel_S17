/*
 * Classes
 * - Board: represents a game state
 * - EvalScheme: evaluation scheme
 *
 * (c) 1997-2005, Josef Weidendorfer
*/

#ifndef BOARD_H
#define BOARD_H

#include "move.h"

class SearchStrategy;
class Evaluator;


/**
 * Class MoveCounter
 *
 * Helper class for board characteristics:
 * Counter for move types and connectivity
 * See Board::countFrom().
 */
class MoveCounter
{
 public:
  enum InARowType { inARow2 = 0, inARow3, inARow4, inARow5, inARowCount };

  MoveCounter();

  void init();
  int  moveCount(int t) { return _moveCount[t]; }
  void incType(int t) { _moveCount[t]++; }
  int  moveSum();
  int  rowCount(int r) { return _rowCount[r]; }
  void incRow(int r) { _rowCount[r]++; }

 private:
  int _moveCount[Move::typeCount];
  int _rowCount[inARowCount];
};



/**
 * Board: represents a game state
 *
 * Includes methods for
 * - play/take back moves
 * - generate allowed moves
 * - calculate rating for position
 * - search for best move
 */
class Board
{
    friend class Evaluator;

 public:
  Board();
  ~Board() {};

  /* different states of one field */
  enum {
    out = 10, free = 0,
    color1, color2, color1bright, color2bright
  };
  enum { AllFields = 121, /* visible + ring of unvisible around */
         RealFields = 61, /* number of visible fields */
         MvsStored = 100 };
  enum { empty=0, 
	 valid1,   // valid state with color1 to draw
	 valid2,   // valid state with color2 to draw
	 timeout1, // time out for color1 -> win for color2
	 timeout2, // time out for color2 -> win for color1
	 win1,     // color1 won
	 win2,     // color2 won
	 invalid };

  /* fill Board with defined values */
  void begin(int startColor);  /* start of a game */
  void clear();                /* empty board     */

  /* fields can't be changed ! */
  int operator[](int no) const;

  int actColor() const
    { return color; }

  /* helper in evaluation: calculate move type counts */
  void countFrom(int startField, int color, MoveCounter&);

  /* Generate list of allowed moves for player with <color>
   * Returns a calculated value for actual position */
  void generateMoves(MoveList& list);

  /* Check if a game position is reachable from the current one.
   * If <fuzzy> is false, this check includes times and move number.
   * Returns move which was played. Returns move of type Move::none if not.
   */
  Move moveToReach(Board*, bool fuzzy);

  /** Check if another board has same tokens set */
  bool hasSameFields(Board*);


  /* Play the given move.
   * Played moves can be taken back (<MvsStored> moves are remembered)
   * Time to play is adjusted if msecs > 0.
   *
   * Warning: Only moves that are generated with Board::generateMoves()
   * should be passed. If the move cannot be played, an assertion is raised.
   */
  void playMove(const Move& m, int msecs = 0);
  bool takeBack();    /* if not remembered, do nothing */
  int movesStored();  /* return how many moves are remembered */

  Move& lastMove()
    { return storedMove[storedLast]; }

  void showHist();
  
  /* Evaluator to use */
  void setEvaluator(Evaluator* ev) { _ev = ev; }

  void setActColor(int c) { color=c; }
  void setColor1Count(int c) { color1Count = c; }
  void setColor2Count(int c) { color2Count = c; }
  void setField(int i, int v) { field[i] = v; }

  void setSpyLevel(int);

  int getColor1Count() 	  { return color1Count; }
  int getColor2Count() 	  { return color2Count; }

  bool isValid() { return (color1Count>8 && color2Count>8); }

  /* Is this position valid, a winner position or invalid? */
  int validState();

  /* returns a string for the valid state */
  static const char* stateDescription(int);

  /* Check that color1Count & color2Count is consistent with board */
  bool isConsistent();

  /* Searching best move */
  void setSearchStrategy(SearchStrategy* ss);
  void setDepth(int d);
  Move& bestMove();
  /* next move in prinipal variation */
  Move& nextMove();

  Move randomMove();
  void stopSearch();

  void setMoveNo(int n) { _moveNo = n; }
  void setMSecsToPlay(int c, int s) { _msecsToPlay[c] = s; }
  int moveNo() { return _moveNo; }
  int msecsToPlay(int c) { return _msecsToPlay[c]; }

  /* Readable ASCII representation */
  char* getState();
  char* getShortState();
  /* Returns true if new state was set */
  bool setState(char*);

  void setVerbose(int v) { _verbose = v; }

  void updateSpy(bool b) { bUpdateSpy = b; }

  /* simple terminal view of position */
  void print();

  static int fieldDiffOfDir(int d) { return direction[d]; }

 private:
  void setFieldValues();

  /* helper function for generateMoves */
  void generateFieldMoves(int, MoveList&);

  // random seed
  int seed;

  int field[AllFields];         /* actual board */
  int color1Count, color2Count;
  int color;                    /* actual color */
  Move storedMove[MvsStored];   /* stored moves */
  int storedFirst, storedLast;  /* stored in ring puffer manner */
  int _moveNo;                   /* move number in current game */
  int _msecsToPlay[3];            /* time in seconds to play */

  bool show, bUpdateSpy;

  int spyLevel, spyDepth;
  SearchStrategy* _ss;
  Evaluator* _ev;
  int _verbose;

  /* constant arrays */
  static int startBoard[AllFields];
  static int order[RealFields];
  static int direction[8];

 public:
  /* for fast evaluation */
  int* fieldArray() { return field; }
};


inline int Board::operator[](int no) const
{
  return (no<12 || no>120) ? out : field[no];
}

#endif
