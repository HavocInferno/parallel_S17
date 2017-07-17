/*
 * Classes
 * - Move: a move on the game board
 * - MoveTypeCounter, InARowCounter: evaluation helpers
 * - MoveList: stores list of moves allowed from a position
 * - Variation: stores move sequences
 *
 * (c) 1997-2005, Josef Weidendorfer
 */

#include <assert.h>
#include <stdio.h>

#include "move.h"
#include "board.h"


static const char* nameOfDir(int dir)
{
  dir = dir % 6;
  return 
    (dir == 1) ? "Right" :
    (dir == 2) ? "RightDown" :
    (dir == 3) ? "LeftDown" :
    (dir == 4) ? "Left" :
    (dir == 5) ? "LeftUp" :
    (dir == 0) ? "RightUp" : "??";
}

static char* nameOfPos(int p)
{
  static char tmp[3];
  tmp[0] = (char)('A' + (p-12)/11);
  tmp[1] = (char)('1' + (p-12)%11);
  tmp[2] = 0;

  return tmp;
}


/// Move

char* Move::name() const
{
  static char s[30];
  int pos;

  /* sideway moves... */
  if (type == left3 || type == right3) {
    int f1, f2, df;
    
    f1 = f2 = field;
    df = 2* Board::fieldDiffOfDir(direction);
    if (df > 0)
      f2 += df;    
    else
      f1 += df;
    
    pos = sprintf(s, "%s-", nameOfPos( f1 ));
    const char* dir = (type == left3) ? nameOfDir(direction-1): nameOfDir(direction+1);
    sprintf(s+pos, "%s/%s", nameOfPos( f2 ), dir);	    
  }
  else if ( type == left2 || type == right2) {
    int f1, f2, df;

    f1 = f2 = field;
    df = Board::fieldDiffOfDir(direction);
    if (df > 0)
      f2 += df;    
    else
      f1 += df;

    pos = sprintf(s, "%s-", nameOfPos( f1 ));
    const char* dir = (type == left2) ? nameOfDir(direction-1): nameOfDir(direction+1);
    sprintf(s+pos, "%s/%s", nameOfPos( f2 ), dir);
  }
  else if (type == none) {
    sprintf(s, "(none)");
  }
  else {
    int p = sprintf(s, "%s/%s",
		    nameOfPos( field ), nameOfDir(direction));
    
    if (type<3) sprintf(s+p,"/Out");
    else if (type<6) sprintf(s+p,"/Push");
  }
  return s;
}

const char* Move::typeName() const
{
    switch(type) {
	case out2:       return "Out, pushing 2 opponents with 3";
	case out1with3:  return "Out, pushing 1 opponent with 3";
	case out1with2:  return "Out, pushing 1 opponent with 2";
	case push2:      return "Pushing 2 opponents with 3";
	case push1with3: return "Pushing 1 opponent with 3";
	case push1with2: return "Pushing 1 opponent with 2";
	case move3:      return "Moving 3 in a row";
	case left3:      return "Moving 3 left sideways";
	case right3:     return "Moving 3 right sideways";
	case left2:      return "Moving 2 left sideways";
	case right2:     return "Moving 2 right sideways";
	case move2:      return "Moving 2 in a row";
	case move1:      return "Moving 1";
	default:
	    break;
    }
    return "Invalid type";
}

void Move::print() const
{
  printf("%s", name() );
}




/// MoveList

MoveList::MoveList()
{
	clear();
}

void MoveList::clear()
{
	int i;
	
	for(i=0;i<Move::typeCount;i++)
		first[i] = actual[i] = -1;
	
	nextUnused = 0;
	actualType = -1;
}

void MoveList::insert(Move m)
{
	int t = m.type;
	
	/* valid and possible ? */
	if (t <0 || t >= Move::typeCount) return;
	//if (nextUnused == MaxMoves) return;

	assert( nextUnused < MaxMoves );

	/* adjust queue */
	if (first[t] == -1) {
		first[t] = last[t] = nextUnused;
	}
	else {
	        assert( last[t] < nextUnused );
		next[last[t]] = nextUnused;
		last[t] = nextUnused;
	}
	
	next[nextUnused] = -1;	
	move[nextUnused] = m;
	nextUnused++;
}

bool MoveList::isElement(int f)
{
	int i;
	
	for(i=0; i<nextUnused; i++) 
	  if (move[i].field == f) 
		return true;

	return false;
}


bool MoveList::isElement(Move &m, int startType, bool del)
{
	int i;
	
	for(i=0; i<nextUnused; i++) {
	  Move& mm = move[i];
	  if (mm.field != m.field) 
	    continue;

	  /* if direction is supplied it has to match */
	  if ((m.direction > 0) && (mm.direction != m.direction))
	    continue;

	  /* if type is supplied it has to match */	    
	  if ((m.type != Move::none) && (m.type != mm.type))
	    continue;
				      
	  if (m.type == mm.type) {
	    /* exact match; eventually supply direction */
	    m.direction = mm.direction;
	    if (del) mm.type = Move::none;
	    return true;
	  }

	  switch(mm.type) {
	  case Move::left3:
	  case Move::right3:
	    if (startType == start3 || startType == all) {
	      m.type = mm.type;
	      m.direction = mm.direction;
	      if (del) mm.type = Move::none;
	      return true;
	    }
	    break;
	  case Move::left2:
	  case Move::right2:
	    if (startType == start2 || startType == all) {
	      m.type = mm.type;
	      m.direction = mm.direction;
	      if (del) mm.type = Move::none;
	      return true;
	    }
	    break;
	  default:
	    if (startType == start1 || startType == all) {
	      /* unexact match: supply type */
	      m.type = mm.type;
	      m.direction = mm.direction;
	      if (del) mm.type = Move::none;
	      return true;
	    }
	  }
	}
	return false;
}


bool MoveList::getNext(Move& m, int maxType)
{
	if (actualType == Move::typeCount) return false;

	while(1) {
	  while(actualType < 0 || actual[actualType] == -1) {
	    actualType++;
	    if (actualType == Move::typeCount) return false;
	    actual[actualType] = first[actualType];
	    if (actualType > maxType) return false;
	  }
	  m = move[actual[actualType]];
	  actual[actualType] = next[actual[actualType]];
	  if (m.type != Move::none) break;
	}

	return true;
}

int MoveList::count(int maxType)
{
    int c = 0;
    int type = actualType;    
    int act = 0;
    if (type>=0) act = actual[type];

    while(1) {
	while(type < 0 || act == -1) {
	    type++;
	    if (type == Move::typeCount) return c;
	    act = first[type];
	    if (type > maxType) return c;
	}
	c++;
	act = next[act];
    }
    return c;
}

/// Variation

void Variation::clear(int d)
{
  int i,j;

  for(i=0;i<maxDepth;i++)
    for(j=0;j<maxDepth;j++) {
      move[i][j].type = Move::none;
    }
  actMaxDepth = (d<maxDepth) ? d:maxDepth-1;
}

void Variation::update(int d, Move& m)
{
  int i;
  
  if (d>actMaxDepth) return;
  for(i=d+1;i<=actMaxDepth;i++) {
    move[d][i]=move[d+1][i];
    move[d+1][i].type = Move::none;
  }
  move[d][d]=m;
}
