/*
 * Classes
 * - Move: a move on the game board
 * - MoveTypeCounter, InARowCounter: evaluation helpers
 * - MoveList: stores list of moves allowed from a position
 * - Variation: stores move sequences
 *
 * (c) 1997-2005, Josef Weidendorfer
 */

#ifndef MOVE_H
#define MOVE_H

/**
 * Class Move
 *
 * A move on the game board.
 * A move is given by a start position number on the board
 * (for board numbering, see board.h), a direction (out of 6),
 * and the type of the move. Type includes the number of own
 * and opponents tokens taken part in the move, and side-way
 * moves.
 */
class Move
{
 public:

  /* Directions */
  enum { Right=1, RightDown, LeftDown, Left, LeftUp, RightUp };

  /* Type of move: Moves are searched in this order */
  enum MoveType { out2 = 0, out1with3, out1with2, push2,
		  push1with3, push1with2, move3, left3, right3,
		  left2, right2, move2, move1, none };
  /* Constants to specify move type ranges. For this to work,
   * the enum order in MoveType must not be changed!
   */
  enum { typeCount = none,
	 maxOutType = out1with2,
	 maxPushType = push1with2,
	 maxMoveType = move1 };

  /* Constructor for an invalid move */
  Move() { type = none; field = 0; direction = 0; }
  /* Move starting at f, direction d, and Type t */
  Move(short f, char d, MoveType t)
    { field = f; direction = d, type = t; }		
	
  /* Does this move push out an opponents token? */
  bool isOutMove() const { return type <= out1with2; }

  /* Does this move push opponent tokens? */
  bool isPushMove() const { return type <= push1with2; }

  /* for communication to outer world */
  char* name() const;
  const char* typeName() const;
  /* debugging output */
  void print() const;

  /* Allow public R/W access... */
  short field;
  unsigned char direction;
  MoveType type;
};


/**
 * Class MoveList
 *
 * Stores a fixed number of moves (for efficince) 
 * <getNext> returns reference of next move ordered according to type
 * <insert> does nothing if there isn't enough free space
 * 
 * Recommend usage (* means 0 or more times):
 *   [ clear() ; insert() * ; isElement() * ; getNext() * ] *
 */
class MoveList
{
 public:
	MoveList();
	
	enum { MaxMoves = 150 };

	/* for isElement: search for moves starting with 1/2/3 fields */
	enum { all , start1, start2, start3 };
		
	void clear();
	void insert(Move);
	bool isElement(int f);
	bool isElement(Move&, int startType, bool del=false);
	void insert(short f, char d, Move::MoveType t)
	  { insert( Move(f,d,t) ); }
	int getLength()
	  { return nextUnused; }
	int count(int maxType = Move::typeCount);
		  
	/**
	 * Get next move from the list into the Move instance
	 * given by first arg which is passed by reference.
	 *
	 * Move types are sorted, and you can specify the maximal
	 * type allowed to be returned. Default is to return all moves.
	 *
	 * Return false if no more moves (with given type constrain)
	 */
	bool getNext(Move&, int maxType = Move::none);

 private:
	Move move[MaxMoves];
	int  next[MaxMoves];
	int  first[Move::typeCount];
	int  last[Move::typeCount];
	int  actual[Move::typeCount];
	int  nextUnused, actualType;
};


/**
 * Stores best move sequence = principal variation
 *
 * Implementation uses upper left triangle of a matrix,
 * with row 0 holding best sequence found so far from
 * real current game position, row 1 holding best sequence
 * starting from currently searched depth 1 in game tree,
 * row 2 from depth 2 and so on.
 *
 * Uses:
 * - A new best move sequence at search depth d was found.
 *   copy current move of depth d into [d][0], and the rest from row d+1.
 * - Read principal variation on starting a new alpha/beta search.
 *   This heuristic is assumed to lead to a lot of cutoffs happening.
 * - Read finally found best move from [0][0]
 * - Allow for computer hints to the opponent by looking at [0][1]
 */
class Variation
{
 public:
  enum { maxDepth = 10 };

  Variation() { clear(1); }

  /* Does the best sequence have a move for depth d ? */
  bool hasMove(int d)
    {  return (d>actMaxDepth) ?
	 false : (move[0][d].type != Move::none); }

  /* Get move at index i from best move chain */
  Move& operator[](int i)
    { return (i<0 || i>=maxDepth) ? move[0][0] : move[0][i]; }

  /* Get move chain at depth d */
  Move* chain(int d)
      { return (d<0 || d>=maxDepth) ? &(move[0][0]) : &(move[d][d]); }

  /* Update best move from depth d, starting with move m at this depth */
  void update(int d, Move& m);

  /* Clear sequence storage for moves from depth d */
  void clear(int d);

  /* Set maximum supported depth */
  void setMaxDepth(int d)
    { actMaxDepth = (d>maxDepth) ? maxDepth-1 : d; }

 private:
  Move move[maxDepth][maxDepth];
  int actMaxDepth;
};


#endif /* _MOVE_H_ */

