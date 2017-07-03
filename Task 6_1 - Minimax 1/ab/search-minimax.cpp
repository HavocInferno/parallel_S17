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

 private:

    /**
     * Implementation of the strategy.
     */
    void searchBestMove();
    int minimax (int depth);
};


void MinimaxStrategy::searchBestMove()
{
    // we try to maximize bestEvaluation

    int bestEval = minEvaluation();
    int eval;

    Move m;
    MoveList list;

    // generate list of allowed moves, put them into <list>
    generateMoves(list);

    // loop over all moves
    while(list.getNext(m)) {


        // draw move, evalute, and restore position
	playMove(m);
	minimax(0);
	//eval = evaluate();   // replace by call to minimax
	takeBack();
	
	if (eval > bestEval) {
	    bestEval = eval;
	    foundBestMove(0, m, eval);
	}
    }

    finishedNode(0,0);
}
int MinimaxStrategy::minimax (int depth)
{
	
	//even depths try to maximize current values, odd try to minimize current value
	bool isBlack = (depth % 2) == 0;
	
	if(isBlack) 
	else
		
  int eval;
  Move m;
  MoveList list;
  
  // if at maximum depth, just evaluate current position
  if (depth>=_maxDepth)
    return evaluate();

  // generate all possible moves
  generateMoves(list);
  
  // evaluate each generated move
  while(list.getNext(m)){
    playMove(m);
    eval=minimax(depth+1);
    takeBack();
    if ((isBlack && eval>bestEval) || (!isBlack && eval<bestEval))  // Black maximized, red minimizes
      {
      bestEval=eval;
      foundBestMove(depth, m, eval); //<- do we actually need this? we are not necessarily trying to find the best move for some matchposition in the future, are we?
      }
  }
  //finishedNode(depth,0); // wrong arguments obviously
  return bestEval;
}
// register ourself as a search strategy
MinimaxStrategy minimaxStrategy;
