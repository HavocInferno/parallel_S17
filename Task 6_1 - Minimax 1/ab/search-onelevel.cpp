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
class OneLevelStrategy: public SearchStrategy
{
 public:
    // Defines the name of the strategy
    OneLevelStrategy(): SearchStrategy("OneLevel") {}

    // Factory method: just return a new instance of this class
    SearchStrategy* clone() { return new OneLevelStrategy(); }

 private:

    /**
     * Implementation of the strategy.
     */
    void searchBestMove();
};


void OneLevelStrategy::searchBestMove()
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
	eval = evaluate();
	takeBack();
	
	if (eval > bestEval) {
	    bestEval = eval;
	    foundBestMove(0, m, eval);
	}
    }

    finishedNode(0,0);
}

// register ourselve as a search strategy
OneLevelStrategy oneLevelStrategy;
