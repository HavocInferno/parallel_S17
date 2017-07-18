/*
 * Classes
 * - Board: represents a game state
 * - EvalScheme: evaluation scheme
 *
 * (c) 1997-2005, Josef Weidendorfer
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "search.h"
#include "board.h"


#if 0
#include <assert.h>
#define CHECK(b)  assert(b)
#else
#define CHECK(b)
#endif


/// MoveCounter

MoveCounter::MoveCounter()
{
  init();
}

void MoveCounter::init()
{
  for(int i=0;i < Move::typeCount;i++)
    _moveCount[i] = 0;
  for(int i=0;i < inARowCount;i++)
    _rowCount[i] = 0;
}

int MoveCounter::moveSum()
{
  int sum = _moveCount[0];

  for(int i=1;i < Move::typeCount;i++)
    sum += _moveCount[i];

  return sum;
}




/****************************** Class Board ****************************/


/* Board for start of a game */
int Board::startBoard[]={
                       10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	             10,  1,  1,  1,  1,  1, 10, 10, 10, 10, 10,
	           10,  1,  1,  1,  1,  1,  1, 10, 10, 10, 10,
                 10,  0,  0,  1,  1,  1,  0,  0, 10, 10, 10,
	       10,  0,  0,  0,  0,  0,  0,  0,  0, 10, 10,
             10,  0,  0,  0,  0,  0,  0,  0,  0,  0, 10,
           10, 10,  0,  0,  0,  0,  0,  0,  0,  0, 10,
         10, 10, 10,  0,  0,  2,  2,  2,  0,  0, 10,
       10, 10, 10, 10,  2,  2,  2,  2,  2,  2, 10,
     10, 10, 10, 10, 10,  2,  2,  2,  2,  2, 10,
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10          };


/* first centrum of board, then rings around (numbers are indexes) */
int Board::order[]={
          60,
	  61,72,71,59,48,49,
	  62,73,84,83,82,70,58,47,36,37,38,50,
	  63,74,85,96,95,94,93,81,69,57,46,35,24,25,26,27,39,51,
	  64,75,86,97,108,107,106,105,104,92,80,68,56,45,34,23,12,
	  13,14,15,16,28,40,52 };

int Board::direction[]= { -11,1,12,11,-1,-12,-11,1 };


Board::Board()
{
  clear();
  _verbose = 0;
  _ev = 0;
}


void Board::begin(int startColor)
{
  int i;

  for(i=0;i<AllFields;i++)
    field[i] = startBoard[i];
  storedFirst = storedLast = 0;
  color = startColor;
  color1Count = color2Count = 14;
  _msecsToPlay[color1] = _msecsToPlay[color2] = 0;

  ::srand(0); // Initialize random sequence
}

void Board::clear()
{
  int i;

  for(i=0;i<AllFields;i++)
    field[i] = (startBoard[i] == out) ? out: free;
  storedFirst = storedLast = 0;
  color1Count = color2Count = 0;
  _msecsToPlay[color1] = _msecsToPlay[color2] = 0;
}

/** countFrom
 *
 * Used for board evaluation to count allowed move types and
 * connectiveness. VERY similar to move generation.
 */
void Board::countFrom(int startField, int color,
		     MoveCounter& MCounter)
{
  int d, dir, c, actField, c2;
  bool left, right;

  /* 6 directions	*/
  for(d=1;d<7;d++) {
    dir = fieldDiffOfDir(d);

    /* 2nd field */
    c = field[actField = startField+dir];
    if (c == free) {
      MCounter.incType( Move::move1 );
      continue;
    }

    if (c != color)
      continue;

    /* 2nd == color */

    MCounter.incRow( MoveCounter::inARow2 );

    /* left side move 2 */
    left = (field[startField + fieldDiffOfDir(d-1)] == free);
    if (left) {
      left = (field[actField + fieldDiffOfDir(d-1)] == free);
      if (left)
	MCounter.incType( Move::left2 );
    }

    /* right side move 2 */
    right = (field[startField + fieldDiffOfDir(d+1)] == free);
    if (right) {
      right = (field[actField + fieldDiffOfDir(d+1)] == free);
      if (right)
	MCounter.incType( Move::right2 );
    }

    /* 3rd field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c .) */
      MCounter.incType( Move::move2 );
      continue;
    }
    else if (c == out) {
      continue;
    }
    else if (c != color) {

      /* 4th field */
      c = field[actField += dir];
      if (c == free) {
	/* (c c o .) */
	MCounter.incType( Move::push1with2 );
      }
      else if (c == out) {
	/* (c c o |) */
	MCounter.incType( Move::out1with2 );
      }
      continue;
    }

    /* 3nd == color */

    MCounter.incRow( MoveCounter::inARow3 );

    /* left side move 3 */
    if (left) {
      left = (field[actField + fieldDiffOfDir(d-1)] == free);
      if (left)
	MCounter.incType( Move::left3 );
    }

    /* right side move 3 */
    if (right) {
      right = (field[actField + fieldDiffOfDir(d+1)] == free);
      if (right)
	MCounter.incType( Move::right3 );
    }

    /* 4th field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c .) */
      MCounter.incType( Move::move3 );
      continue;
    }
    else if (c == out) {
      continue;
    }
    else if (c != color) {

      /* 4nd == opponent */

      /* 5. field */
      c2 = field[actField += dir];
      if (c2 == free) {
	/* (c c c o .) */
	MCounter.incType( Move::push1with3 );
	continue;
      }
      else if (c2 == out) {
	/* (c c c o |) */
	MCounter.incType( Move::out1with3 );
	continue;
      }
      if (c2 != c)
	continue;

      /* 5nd == opponent */

      /* 6. field */
      c2 = field[actField += dir];
      if (c2 == free) {
	/* (c c c o o .) */
	MCounter.incType( Move::push2 );
      }
      else if (c2 == out) {
	/* (c c c o o |) */
	MCounter.incType( Move::out2 );
      }

      continue;
    }

    /* 4nd == color */

    MCounter.incRow( MoveCounter::inARow4 );

    /* 5th field */
    c = field[actField += dir];
    if (c != color)
      continue;

    /* 4nd == color */

    MCounter.incRow( MoveCounter::inARow5 );
  }
}


/* generate moves starting at field <startField> */
void Board::generateFieldMoves(int startField, MoveList& list)
{
  int d, dir, c, actField;
  bool left, right;
  int opponent = (color == color1) ? color2 : color1;

  assert( field[startField] == color );

  /* 6 directions	*/
  for(d=1;d<7;d++) {
    dir = direction[d];

    /* 2nd field */
    c = field[actField = startField+dir];
    if (c == free) {
      /* (c .) */
      list.insert(startField, d, Move::move1);
      continue;
    }
    if (c != color)
      continue;

    /* 2nd == color */

    left = (field[startField+direction[d-1]] == free);
    if (left) {
      left = (field[actField+direction[d-1]] == free);
      if (left)
	/* 2 left */
	list.insert(startField, d, Move::left2);
    }

    right = (field[startField+direction[d+1]] == free);
    if (right) {
      right = (field[actField+direction[d+1]] == free);
      if (right)
	/* 2 right */
	list.insert(startField, d, Move::right2);
    }

    /* 3rd field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c .) */
      list.insert(startField, d, Move::move2);
      continue;
    }
    else if (c == opponent) {

      /* 4th field */
      c = field[actField += dir];
      if (c == free) {
	/* (c c o .) */
	list.insert(startField, d, Move::push1with2);
      }
      else if (c == out) {
	/* (c c o |) */
	list.insert(startField, d, Move::out1with2);
      }
      continue;
    }
    if (c != color)
      continue;

    /* 3nd == color */

    if (left) {
      if (field[actField+direction[d-1]] == free)
	/* 3 left */
	list.insert(startField, d, Move::left3);
    }

    if (right) {
      if (field[actField+direction[d+1]] == free)
	/* 3 right */
	list.insert(startField, d, Move::right3);
    }

    /* 4th field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c .) */
      list.insert(startField, d, Move::move3);
      continue;
    }
    if (c != opponent)
      continue;

    /* 4nd == opponent */

    /* 5. field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c o .) */
      list.insert(startField, d, Move::push1with3);
      continue;
    }
    else if (c == out) {
      /* (c c c o |) */
      list.insert(startField, d, Move::out1with3);
      continue;
    }
    if (c != opponent)
      continue;

    /* 5nd == opponent */

    /* 6. field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c o o .) */
      list.insert(startField, d, Move::push2);
    }
    else if (c == out) {
      /* (c c c o o |) */
      list.insert(startField, d, Move::out2);
    }
  }
}


void Board::generateMoves(MoveList& list)
{
	int actField, f;

	for(f=0;f<RealFields;f++) {
		actField = order[f];
		if ( field[actField] == color)
		   generateFieldMoves(actField, list);
	}
}


Move Board::moveToReach(Board* b, bool fuzzy)
{
    Move m;

    /* can not move from invalid position */
    int state = validState();
    if ((state != valid1) && (state != valid2))
	return m;
    
    if (!fuzzy) {
	if (b->moveNo() != _moveNo+1) {
	    if (_verbose)
		printf("Board::moveToReach: moveNo %d => %d ?!\n",
		       _moveNo, b->moveNo());
	    return m;
	}
	/* only time left for player can have decreased */
	int opponent = (color == color1) ? color2 : color1;
	if (_msecsToPlay[opponent] != b->msecsToPlay(opponent)) {
	    if (_verbose)
		printf("Board::moveToReach: Opponent time changed ?!\n");
	    return m;
	}
	if (_msecsToPlay[color] < b->msecsToPlay(color)) {
	    if (_verbose)
		printf("Board::moveToReach: Player time increased ?!\n");
	    return m;
	}
    }

    /* detect move drawn */
    MoveList l;
    generateMoves(l);

    if (_verbose) {
	printf("Board::moveToReach - %d allowed moves:\n",
	       l.getLength());
	Move found;
	int type = Move::none;
	while(l.getNext(m, Move::maxMoveType)) {
	    playMove(m);

	    bool isSame = hasSameFields(b);
	    takeBack();
	    if (isSame) found = m;

	    if (m.type != type) {
		type = m.type;
		printf(" %s:\n", m.typeName());
	    }
	    printf("  %s%s\n", m.name(), isSame ? " <== Choosen":"");
	}
	m = found;
    }
    else {
	while(l.getNext(m, Move::maxMoveType)) {
	    playMove(m);

	    bool isSame = hasSameFields(b);
	    takeBack();
	    if (isSame) break;
	}
    }

    return m;
}

bool Board::hasSameFields(Board* b)
{
    int f, actField;

    for(f=0;f<RealFields;f++) {
	actField = order[f];
	if ( field[actField] != (*b)[actField] )
	    return false;
    }

    return true;
}


void Board::playMove(const Move& m, int msecs)
{
	int f, dir, dir2;
	int opponent = (color == color1) ? color2:color1;

	CHECK( isConsistent() );

	if (++storedLast == MvsStored) storedLast = 0;

	/* Buffer full -> delete oldest entry */
	if (storedLast == storedFirst)
	  	if (++storedFirst == MvsStored) storedFirst = 0;

	storedMove[storedLast] = m;

	f = m.field;
	CHECK( (m.type >= 0) && (m.type < Move::none));
	CHECK( field[f] == color );
	field[f] = free;
	dir = direction[m.direction];

	switch(m.type) {
	 case Move::out2:        /* (c c c o o |) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == opponent );
		CHECK( field[f + 5*dir] == out );
		field[f + 3*dir] = color;
		break;
	 case Move::out1with3:   /* (c c c o |)   */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == out );
		field[f + 3*dir] = color;
		break;
	 case Move::move3:       /* (c c c .)     */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == free );
		field[f + 3*dir] = color;
		break;
	 case Move::out1with2:   /* (c c o |)     */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == opponent );
		CHECK( field[f + 3*dir] == out );
		field[f + 2*dir] = color;
		break;
	 case Move::move2:       /* (c c .)       */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == free );
		field[f + 2*dir] = color;
		break;
	 case Move::push2:       /* (c c c o o .) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == opponent );
		CHECK( field[f + 5*dir] == free );
		field[f + 3*dir] = color;
		field[f + 5*dir] = opponent;
		break;
	 case Move::left3:
		dir2 = direction[m.direction-1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		CHECK( field[f + 2*dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::right3:
		dir2 = direction[m.direction+1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		CHECK( field[f + 2*dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::push1with3:   /* (c c c o .) => (. c c c o) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == free );
		field[f + 3*dir] = color;
		field[f + 4*dir] = opponent;
		break;
	 case Move::push1with2:   /* (c c o .) => (. c c o) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == opponent );
		CHECK( field[f + 3*dir] == free );
		field[f + 2*dir] = color;
		field[f + 3*dir] = opponent;
		break;
	 case Move::left2:
		dir2 = direction[m.direction-1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::right2:
		dir2 = direction[m.direction+1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::move1:       /* (c .) => (. c) */
		CHECK( field[f + dir] == free );
		field[f + dir] = color;
		break;
	default:
	  break;
	}

	if (m.isOutMove()) {
		if (color == color1)
		  color2Count--;
		else
		  color1Count--;
	}

	/* adjust move number and time */
	_moveNo++;
	if ((_msecsToPlay[color]>0) && (msecs>0)) {
	    if (_msecsToPlay[color] > msecs)
		_msecsToPlay[color] -= msecs;
	    else
		_msecsToPlay[color] = 0;
	}

	/* change actual color */
	color = opponent;

	CHECK( isConsistent() );

}

bool Board::takeBack()
{
  int f, dir, dir2;
  int opponent = color;
  Move& m = storedMove[storedLast];

  CHECK( isConsistent() );

  if (storedFirst == storedLast) return false;

  /* change actual color */
  color = (color == color1) ? color2:color1;

  if (m.isOutMove()) {
    if (color == color1)
      color2Count++;
    else
      color1Count++;
  }

  f = m.field;
  CHECK( field[f] == free );
  field[f] = color;
  dir = direction[m.direction];

  switch(m.type) {
  case Move::out2:        /* (. c c c o |) => (c c c o o |) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == opponent );
    CHECK( field[f + 5*dir] == out );
    field[f + 3*dir] = opponent;
    break;
  case Move::out1with3:   /* (. c c c |) => (c c c o |) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == out );
    field[f + 3*dir] = opponent;
    break;
  case Move::move3:       /* (. c c c) => (c c c .)     */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    field[f + 3*dir] = free;
    break;
  case Move::out1with2:   /* (. c c | ) => (c c o |)     */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == out );
    field[f + 2*dir] = opponent;
    break;
  case Move::move2:       /* (. c c) => (c c .)       */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    field[f + 2*dir] = free;
    break;
  case Move::push2:       /* (. c c c o o) => (c c c o o .) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == opponent );
    CHECK( field[f + 5*dir] == opponent );
    field[f + 3*dir] = opponent;
    field[f + 5*dir] = free;
    break;
  case Move::left3:
    dir2 = direction[m.direction-1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + 2*dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    CHECK( field[f + 2*dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::right3:
    dir2 = direction[m.direction+1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + 2*dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    CHECK( field[f + 2*dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::push1with3:   /* (. c c c o) => (c c c o .) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == opponent );
    field[f + 3*dir] = opponent;
    field[f + 4*dir] = free;
    break;
  case Move::push1with2:   /* (. c c o) => (c c o .) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == opponent );
    field[f + 2*dir] = opponent;
    field[f + 3*dir] = free;
    break;
  case Move::left2:
    dir2 = direction[m.direction-1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::right2:
    dir2 = direction[m.direction+1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::move1:       /* (. c) => (c .) */
    CHECK( field[f + dir] == color );
    field[f + dir] = free;
    break;
  default:
    break;
  }

  if (--storedLast < 0) storedLast = MvsStored-1;

  /* adjust move number. Time is intentionally not reset */
  _moveNo--;

  CHECK( isConsistent() );

  return true;
}

int Board::movesStored()
{
  int c = storedLast - storedFirst;
  if (c<0) c+= MvsStored;
  return c;
}

/** validState
 *
 * Check for a valid board position to play from:
 * (1) Number of balls for each color has to be between 9 and 14
 * (2) There must be a move possible for actual color
 */
int Board::validState()
{
    MoveCounter mc;
    int c1 = 0, c2 = 0;
    int i,j, moveCount, res;
    int color = actColor();

    for(i=0;i<AllFields;i++) {
	j=field[i];
	if (j == color1) c1++;
	if (j == color2) c2++;
	if (j == color)
	    countFrom( i, color, mc);
    }

    color1Count = c1;
    color2Count = c2;
    moveCount = mc.moveSum();

    if (c1==0 && c2==0)
	res = empty;
    // impossible token counts
    else if (c1>14 || c2>14 || (c1<9 && c2<9))
	res = invalid;
    else if (c1<9)
	res = win2;
    else if (c2<9)
	res = win1;
    // counts are in valid range...
    else if (moveCount>0) {
	if (color == color1) {
	    if (_msecsToPlay[color1]>0 && _msecsToPlay[color2]<=0)
		res = timeout2;
	    else
		res = valid1;
	}
	else {
	    if (_msecsToPlay[color2]>0 && _msecsToPlay[color1]<=0)
		res = timeout1;
	    else
		res = valid2;
	}
    }
    // color which has to draw can not do any move... win for opponent!
    else if (color == color1)
	res = win2;
    else
	res = win1;
    
#ifdef MYTRACE
    if (spyLevel>2) {
	indent(spyDepth);
	printf("Valid: %s (Color1 %d, Color2 %d, moveCount of %d: %d)\n",
	       (res == empty) ? "empty" : (res==valid) ? "valid":"invalid",
	       c1,c2,color,moveCount);
    }
#endif

    return res;
}


const char* Board::stateDescription(int s)
{
    switch(s) {
	case empty:    return "Empty board";
	case invalid:  return "Invalid board";
	case valid1:   return "O about to move...";
	case valid2:   return "X about to move...";
	case timeout1: return "O timed out. X wins!";
	case timeout2: return "X timed out. O wins!";
	case win1:     return "O wins the game!";
	case win2:     return "X wins the game!";
	default: break;
    }
    return "Unknown state?";
}


bool Board::isConsistent()
{
  int c1 = 0, c2 = 0;
  int i,j;

  for(i=0;i<RealFields;i++) {
    j=field[order[i]];
    if (j == color1) c1++;
    if (j == color2) c2++;
  }
  return (color1Count == c1 && color2Count == c2);
}


void Board::setSearchStrategy(SearchStrategy* ss)
{
    _ss = ss; 
}

void Board::setDepth(int d)
{
    if (!_ss) return;
    _ss->setMaxDepth(d+1); 
}

Move& Board::bestMove()
{
    static Move best;

    best = _ss->bestMove(this);
    return best;
}

Move& Board::nextMove()
{
    return _ss->nextMove(); 
}

void Board::stopSearch()
{
    _ss->stopSearch(); 
}

Move Board::randomMove()
{
	Move m;
	MoveList list;

	generateMoves(list);
	int l = list.getLength();

	int j = (::rand() % l) +1;

	while(j != 0) {
		list.getNext(m, Move::none);
		j--;
	}

	return m;
}


void Board::print()
{
    printf( "%s", getState() );
}

char* Board::getShortState()
{
    static char b[256];
    int pos = 0;

    pos += sprintf(b+pos, "#%d", _moveNo);
    pos += sprintf(b+pos, "  O: %d", color1Count);
    if (_msecsToPlay[color1]>0)
      pos += sprintf(b+pos, " (%d.%03d s)",
		     _msecsToPlay[color1]/1000, _msecsToPlay[color1]%1000);
    pos += sprintf(b+pos, ",  X: %d", color2Count);
    if (_msecsToPlay[color2]>0)
      pos += sprintf(b+pos, " (%d.%03d s)",
		     _msecsToPlay[color2]/1000, _msecsToPlay[color2]%1000);
    return b;
}

/* Returns a board human readable board representation.
 * The returned string will be overwritten in later calls.
 */
char* Board::getState()
{
    static char b[1024];
    int pos = 0;

    int row,i;
    char spaces[]="      ";
    const char *z[]={". ","O ","X ", "o ", "x "};

    
    pos = sprintf(b, "\n");
    if (_moveNo>=0)
      pos += sprintf(b+pos, "%s\n", getShortState());

    pos += sprintf(b+pos, "       -----------\n");
    for(row=0;row<4;row++) {
	pos += sprintf(b+pos, "%s/ ",spaces+row);
	for(i=0;i<5+row;i++)
	    pos += sprintf(b+pos, "%s",z[field[row*11+12+i]]);
	pos += sprintf(b+pos, "\\\n");
    }
    pos += sprintf(b+pos, "  | ");
    for(i=0;i<9;i++)
	pos += sprintf(b+pos, "%s", z[field[56+i]]);
    pos += sprintf(b+pos, "|\n");
    for(row=0;row<4;row++) {
	pos += sprintf(b+pos, "%s\\ ",spaces+3-row);
	for(i=0;i<8-row;i++)
	    pos += sprintf(b+pos, "%s",z[field[68+row*12+i]]);
	pos += sprintf(b+pos, "/\n");
    }
    pos += sprintf(b+pos, "       -----------\n");

    return b;
}

bool Board::setState(char* s)
{
  int row = 0, column = 0;
  bool found = false;

  color1Count = 0;
  color2Count = 0;
  _msecsToPlay[color1] = 0;
  _msecsToPlay[color2] = 0;
 
  if (s == 0) return false;

  while(*s) {
      if (*s == '#') {
	  int m = 0;
	  int color = -1;
	  s++;
	  // parse "#<moveNo> - [O: <count> / <secsToPlay>, X:]"
	  while((*s >= '0') && (*s <= '9')) m = 10*m + (*s++ -'0');
	  _moveNo = m;

	  while(*s) {
	      if (*s == '\n') break;
	      if (*s == 'O') { color = color1; }
	      if (*s == 'X') { color = color2; }
	      if ((*s == '(') && (color>-1)) {
		  s++;
		  int secs=0, msecs=0;
		  while(*s==' ') s++;
		  while((*s >= '0') && (*s <= '9')) secs = 10*secs + (*s++ -'0');
		  if (*s=='.') {
		      int val = 100;
		      s++;
		      while((*s >= '0') && (*s <= '9')) {
			  msecs += val * (*s++ -'0');
			  val /= 10;
		      }
		  }
		  _msecsToPlay[color] = 1000 * secs + msecs;
		  color = -1;
	      }
	      s++;
	  }
      }
      if (row*11+12+column > AllFields) break;
      if (*s == '.' || *s == 'X' || *s == 'O' ||
	  *s == 'x' || *s == 'o') {
	  found = true;
	  field[row*11+12+column] =
	      (*s == 'O') ? color1 :
	      (*s == 'o') ? color1 :
	      (*s == 'X') ? color2 :
	      (*s == 'x') ? color2 : free;
	  if (field[row*11+12+column] == color1) color1Count++;
	  if (field[row*11+12+column] == color2) color2Count++;
	  column++;
      }
      if (found && (*s == '\n')) {
	  row++;
	  if (row < 5) column = 0;
	  else column = row-4;
	  if (row >8) break;
      }
      s++;
  }
  if (row <9) return false;

  color = ((_moveNo%2)==0) ? color1 : color2;

  return true;
}


void Board::setSpyLevel(int level)
{
  spyLevel = level;
}

