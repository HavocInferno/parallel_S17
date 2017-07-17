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
class MinimaxStrategyMPI: public SearchStrategy
{
 public:
    // Defines the name of the strategy
    MinimaxStrategyMPI(): SearchStrategy("Minimax with MPI") {}

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new MinimaxStrategyMPI(); }

 private:

    /**
     * Implementation of the strategy.
     */
    void searchBestMove();
    int minimax (int depth);
    void enterSlave();
};


void MinimaxStrategyMPI::searchBestMove()
{
  // loop over all moves
  
  int nprocs = _sc->getnprocs();
  int myid = _sc->getmyid();
    
  if (myid==0)
    {
      int eval=0;
      Move m;
      MoveList list;
      MPI_Status status;
      char board [500];
      
      sprintf(board, "%s\n", _board->getState());
      // we need to distribute the board to all processes
      for (int i=1; i<nprocs;i++)
	{MPI_Send (board, 500, MPI_CHAR, i, 0, MPI_COMM_WORLD);}


      int color = _board->actColor();
      // generate list of allowed moves, put them into <list>
      generateMoves(list);
      int bestEval;
      if (color==_board->color1) 
	bestEval = minEvaluation();
      else
	bestEval = maxEvaluation();
      printf("There are %d Moves\n", list.count());
      int ctr=0;
      // distribute list.count() moves over nodes
      while(list.getNext(m)) 
	{
	  if (ctr%nprocs==myid||true)
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
	}// end of loop
      // merge results
      finishedNode(0,&m);
    }
  
  else if (1<0)
    {
      // endless loop, so that it works for multiple turns
      while (1)
	{
	  int eval=0;
	  int nprocs = _sc->getnprocs();
	  int myid = _sc->getmyid();
	  Move m;
	  MoveList list;
	  MPI_Status status;
	  char board [500];
	  
	  MPI_Recv (board, 500, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	  // receive boards, set board to val
	  _board->setState(board);
	  
	  int color = _board->actColor();
	  // generate list of allowed moves, put them into <list>
	  generateMoves(list);
	  int bestEval;
	  if (color==_board->color1) 
	    bestEval = minEvaluation();
	  else
	    bestEval = maxEvaluation();
	  printf("There are %d Moves\n", list.count());
	  int ctr=0;
	  // distribute list.count() moves over nodes
	  while(list.getNext(m)) 
	    {
	      if (ctr%nprocs==myid)
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
	    }// end of loop
	  //finishedNode(0,&m);
	}
    }
  // all processes will execute
}

int MinimaxStrategyMPI::minimax (int depth)
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
void MinimaxStrategyMPI::enterSlave()
{
  // entry point for slave processes
  
  this->searchBestMove();
  
  
}
// register ourself as a search strategy
MinimaxStrategyMPI minimaxStrategyMPI;
