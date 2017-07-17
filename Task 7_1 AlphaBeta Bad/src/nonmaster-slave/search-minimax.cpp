#/**
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
    int myid=_sc->getmyid();
    int nprocs=_sc->getnprocs();
    if (myid==0)
      {


	MPI_Status status;
	int eval=0;
	char board [500];
	Move m;
	MoveList list;
	strncpy(board, _board->getState(), 500);
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
	int ctr=0;
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
	  }
	// recv bestmove from slaves. field, dir, type, value
	int eval2, type;
	int leaves, nodes;
	short field;
	unsigned char dir;
	printf("I am p%d, my best eval is %d\n", myid, bestEval);
	for (int i=1; i<nprocs; i++)
	  {
	    MPI_Recv(&eval2, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&type, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&field, 1, MPI_SHORT, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&dir, 1, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&leaves, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
	    MPI_Recv(&nodes, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
	    _sc->addLeavesvisited(leaves);
	    _sc->addNodesvisited(nodes);
	    leaves=nodes=0;
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
	printf("I am p%d after merge, my best eval is %d\n", myid, bestEval);
	finishedNode(0,&m);
      } 
    else // slave process
      {
	MPI_Status status;
	int eval=0;
	int ctr=0;
	while (1) // once process breaks out of this loop, the whole player terminates!
	  {
	    _sc->clear();
	    // init board
	    char board [500];
	    MPI_Recv(board, 500, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
	    Move m;
	    MoveList list;
	    if (strncmp(board, "exit", 4)==0) {
	      break;
	    }
	    bool check=_board->setState(board);
	    int color = _board->actColor();
	    int bestEval;
	    if (color==_board->color1) 
	      bestEval = minEvaluation();
	    else
	      bestEval = maxEvaluation();
	    
	    generateMoves(list);
	    while(list.getNext(m))
	      {
		if (ctr%nprocs==myid){
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
	    printf("I am p%d, my best eval is %d\n", myid, bestEval);
	    finishedNode(0,&m);
	    int leaves=_sc->getLeavesVisited();
	    int nodes=_sc->getNodesVisited();
	    // send best move to root. dir, type, field, value
	    MPI_Send(&bestEval, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
	    MPI_Send(&_bestMove.type, 1, MPI_INT, 0, 0, MPI_COMM_WORLD); // enum
	    MPI_Send(&_bestMove.field, 1, MPI_SHORT, 0, 0, MPI_COMM_WORLD);
	    MPI_Send(&_bestMove.direction, 1, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
	    MPI_Send(&leaves, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
	    MPI_Send(&nodes, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
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
