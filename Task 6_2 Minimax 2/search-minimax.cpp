/**
 * Very simple example strategy:
 * Search all possible positions reachable via one move,
 * and return the move leading to best position
 *
 * (c) 2006, Josef Weidendorfer
 */

#include "search.h"
#include "board.h"
#include "eval.h"
#include <stdio.h>
#include <string.h>
#include "mpi.h"
/**
 * To create your own search strategy:
 * - copy this file into another one,
 * - change the class name one the name given in constructor,
 * - adjust clone() to return an instance of your class
 * - adjust last line of this file to create a global instance
 *   of your class
 * - adjust the Makefile to include your class in SEARCH_OBJS
 * - implement searchBestMove()
 *
 * Advises for implementation of searchBestMove():
 * - call foundBestMove() when finding a best move since search start
 * - call finishedNode() when finishing evaluation of a tree node
 * - Use _maxDepth for strength level (maximal level searched in tree)
 */
class MinimaxStrategy: public SearchStrategy
{
 public:
    // Defines the name of the strategy
    MinimaxStrategy(): SearchStrategy("Minimax") {}

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new MinimaxStrategy(); }
  void enterSlave();
private:
  
  /**
   * Implementation of the strategy.
   */
  void searchBestMove();
  int minimax (int depth);
};
void MinimaxStrategy::enterSlave()
{
  this->searchBestMove();
  // entry point for slave processes
}

void MinimaxStrategy::searchBestMove()
{
    // we try to maximize bestEvaluation

    int myid=_sc->getmyid();
    int nprocs=_sc->getnprocs();
    printf("P %d of %d", myid, nprocs);
    if (myid==0)
      {


	MPI_Status status;
	int eval=0;
	char board [500];
	Move m;
	MoveList list;
	//sprintf(board, "%s\n", _board->getState());
	strncpy(board, _board->getState(), 500);
	printf("Length: %d", strlen(board));
	// distribute board to procs 1..nprocs
	for (int i=1; i<nprocs; i++)
	  {
	    MPI_Send(board, 500, MPI_CHAR, i, 0, MPI_COMM_WORLD);
	  }
	
	int color = _board->actColor();
	// generate list of allowed moves, put them into <list>
	generateMoves(list);
	int bestEval;
	if (color==_board->color1) 
	  bestEval = minEvaluation();
	else
	  bestEval = maxEvaluation();
	// loop over all moves
	printf("There are %d Moves\n", list.count());
	int ctr=0;
	while(list.getNext(m)) 
	  {
	    if (myid%nprocs==ctr)
	    {
	      
	      // draw move, evalute, and restore position
	      playMove(m);
	      eval=minimax(0);
	      takeBack();
	      if (color==_board->color1)
		{
		  if (eval > bestEval) 
		    {
		      bestEval = eval;
		      foundBestMove(0,m,eval);
		  }
		}
	      else // color 2
		{
		  if (eval<bestEval)
		    {
		      bestEval=eval;
		      foundBestMove(0,m,eval);
		    }
		}
	    }
	    ctr++;
	  }
	// recv bestmove from slaves. field, dir, type, value
	int eval2, type;
	short field;
	unsigned char dir;
	for (int i=1; i<nprocs; i++)
	  {
	    MPI_Recv(&eval2, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&type, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&field, 1, MPI_SHORT, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&dir, 1, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, &status);
	    // cmp with own bestmove, update if needed
	      if (color==_board->color1)
		{
		  if (eval2 > bestEval) 
		    {
		      bestEval = eval2;
		      Move::MoveType type = (Move::MoveType) type;
		      Move newMove (field, dir, type);
		      foundBestMove(0,newMove,eval2);
		    }
		}
	      else // color 2
		{
		  if (eval2<bestEval)
		    {
		      bestEval = eval2;
		      Move::MoveType type = (Move::MoveType) type;
		      Move newMove (field, dir, type);
		      foundBestMove(0,newMove,eval2);
		    }
		}
	  }
		
	finishedNode(0,&m);
      } 
    else // slave process
      {
	MPI_Status status;
	int eval=0;
	char board [500];
	Move m;
	MoveList list;
	int ctr=0;
	while (1) // once process breaks out of this loop, the whole player terminates!
	  {
	    // init board
	    MPI_Recv(board, 500, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);

	    if (strncmp(board, "quit", 4)==0) {
	      break;
	    }
	    printf("DEBUG: %s\nDEBUG\n", board);	    
	    bool check=_board->setState(board);
	    printf("Has set state\n");
	    int color = _board->actColor();
	    int bestEval;
	    if (color==_board->color1) 
	      bestEval = minEvaluation();
	    else
	      bestEval = maxEvaluation();
	    
	    generateMoves(list);
	    while(list.getNext(m))
	      {
		if (myid%nprocs==ctr){
		  // draw move, evalute, and restore position
		  playMove(m);
		  eval=minimax(0);
		  takeBack();
		  if (color==_board->color1)
		    {
		      if (eval > bestEval) 
			{
			  bestEval = eval;
			  foundBestMove(0,m,eval);
			}
		}
		  else // color 2
		    {
		      if (eval<bestEval)
			{
			  bestEval=eval;
			  foundBestMove(0,m,eval);
			}
		    }
		}
		ctr++;
	      }
	    // send best move to root. dir, type, field, value
	    MPI_Send(&eval, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
	    MPI_Send(&_bestMove.type, 1, MPI_INT, 0, 0, MPI_COMM_WORLD); // enum
	    MPI_Send(&_bestMove.field, 1, MPI_SHORT, 0, 0, MPI_COMM_WORLD);
	    MPI_Send(&_bestMove.direction, 1, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);

	  }
      }
}
int MinimaxStrategy::minimax (int depth)
{
	int bestEval;
	
	int color = _board->actColor();
	if(color==_board->color1) 
	  bestEval = minEvaluation();
	else
	  bestEval = maxEvaluation();
	
	int eval;
	Move m;
	MoveList list;
	
	// if at maximum depth, just evaluate current position
	if (depth>=_maxDepth)
	  {
	    return evaluate();
	  }
	// generate all possible moves
	generateMoves(list);
	
	// evaluate each generated move
	while(list.getNext(m)){
	  playMove(m);
	  eval=minimax(depth+1);
	  takeBack();
	  if (color==_board->color1)  // Black maximized, red minimizes
	    // color 1
	    {
	      if (eval>bestEval)
		{
		  bestEval=eval;
		  //foundBestMove(depth, m, eval); //<- do we actually need this? we are not necessarily trying to find the best move for some matchposition in the future, are we?
		}
	    }
	  else // color2
	    if (eval<bestEval)
	      {
		bestEval=eval;
	      }
	}
	finishedNode(depth,&m);
	return bestEval;
}
// register ourself as a search strategy
MinimaxStrategy minimaxStrategy;
