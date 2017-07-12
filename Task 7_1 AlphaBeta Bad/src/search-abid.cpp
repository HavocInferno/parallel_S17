/**
 * A real world, sequential strategy:
 * Alpha/Beta with Iterative Deepening (ABID)
 *
 * (c) 2005, Josef Weidendorfer
 */

#include <stdio.h>

#include "search.h"
#include "board.h"
#include "mpi.h"

class ABIDStrategy: public SearchStrategy
{
 public:
    ABIDStrategy(): SearchStrategy("ABID", 2) {}
    SearchStrategy* clone() { return new ABIDStrategy(); }

    Move& nextMove() { return _pv[1]; }
  void enterSlave(){
	  while(true)
		this->searchBestMove();
  // entry point for slave processes
}
 private:
    void searchBestMove();
    /* recursive alpha/beta search */
    int alphabeta(int depth, int alpha, int beta);

    /* prinicipal variation found in last search */
    Variation _pv;
    Move _currentBestMove;
    bool _inPV;
    int _currentMaxDepth;
};


/**
 * Entry point for search
 *
 * Does iterative deepening and alpha/beta width handling, and
 * calls alpha/beta search
 */
 
void ABIDStrategy::searchBestMove()
{    
    int alpha = -15000, beta = 15000;
    int nalpha, nbeta, currentValue = 0;

	int nprocs = _sc->getnprocs();
	int myid = _sc->getmyid();
	
	MPI_Status status;


	
	if (myid==0)
    {
		printf("Total num of porcs: %d\n", nprocs);
		char board [500];
      
		sprintf(board, "%s\n", _board->getState());
		// we need to distribute the board to all processes
		for (int i=1; i<nprocs;i++)
		{
			printf("Proc %d: Sending board to %d\n", myid, i);
			MPI_Send (board, 500, MPI_CHAR, i, 0, MPI_COMM_WORLD);
		}
	}
	else
	{
		
		printf("Proc %d: Receiving board\n", myid);
		char board [500];
		MPI_Recv (board, 500, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		// receive boards, set board to val
		_board->setState(board);
	}
	int color = _board->actColor();
	
	int bestValue;
	if (color==_board->color1)
		bestValue = alpha;
	else
		bestValue = beta;
	
	Move m;
    MoveList list;
	char tmp[100];
	//sprintf(tmp, "Proc %d: Generating moves list\n",myid);
	//_sc->substart(tmp);
	_board->generateMoves(list);
	int ctr=0;
	printf("Proc %d: Starting alpha beta\n",myid);
	while(list.getNext(m)) 
	{
		if (ctr%nprocs==myid)
		{
			playMove(m);
			
			_pv.clear(_maxDepth);
			_currentBestMove.type = Move::none;
			_currentMaxDepth=1;
			/* iterative deepening loop */
			do {

			/* searches on same level with different alpha/beta windows */
				while(1) {

					nalpha = alpha, nbeta = beta;
					_inPV = (_pv[0].type != Move::none);

					if (_sc && _sc->verbose()) {
					
					//sprintf(tmp, "Proc %d: Alpha/Beta [%d;%d] with max depth %d",myid, alpha, beta, _currentMaxDepth);
			
					}
					
					currentValue = alphabeta(0, alpha, beta);

					/* stop searching if a win position is found */
					if (currentValue > 14900 || currentValue < -14900)
					_stopSearch = true;

					/* Don't break out if we haven't found a move */
					if (_currentBestMove.type == Move::none)
					_stopSearch = false;

					if (_stopSearch) break;

					/* if result is outside of current alpha/beta window,
					 * the search has to be rerun with widened alpha/beta
					 */
					if (currentValue <= nalpha) {
					alpha = -15000;
					if (beta<15000) beta = currentValue+1;
					continue;
					}
					if (currentValue >= nbeta) {
					if (alpha > -15000) alpha = currentValue-1;
					beta=15000;
					continue;
					}
					break;
				}
				
				
				/* Window in both directions cause of deepening */
				alpha = currentValue - 200, beta = currentValue + 200;
			
				if (_stopSearch) break;

				_currentMaxDepth++;
			}
			while(_currentMaxDepth <= _maxDepth);
			takeBack();
			if (color==_board->color1)
			{
				if(currentValue>bestValue)
				{
					bestValue = currentValue;
					_bestMove = _currentBestMove;
				}
			}
			else
			{
				if(currentValue<bestValue)
				{
					bestValue = currentValue;
					_bestMove = _currentBestMove;
				}
			}
	
			
			
		}
		ctr++;
	}
	
	printf("Proc %d: Done with everything\n",myid);
	if(myid==0)
	{
		
		printf("Proc %d: Receiving\n",myid);
		//_sc->substart(tmp);
		int eval2, type;
		int leaves, nodes;
		short field;
		unsigned char dir;
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
				if (eval2 > bestValue) 
				{
					bestValue = eval2;
					Move::MoveType type = (Move::MoveType) type;
					Move newMove (field, dir, type);
					foundBestMove(0,newMove,eval2);
				}
			}
			else // color 2
			{
				if (eval2<bestValue)
				{
					bestValue = eval2;
					Move::MoveType type = (Move::MoveType) type;
					Move newMove (field, dir, type);
					foundBestMove(0,newMove,eval2);
				}
			}
	  }
	}
	else
	{
		printf("Proc %d: Sending\n",myid);
		//_sc->substart(tmp);
		int leaves=_sc->getLeavesVisited();
		int nodes=_sc->getNodesVisited();
		// send best move to root. dir, type, field, value
		MPI_Send(&bestValue, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(&_bestMove.type, 1, MPI_INT, 0, 0, MPI_COMM_WORLD); // enum
		MPI_Send(&_bestMove.field, 1, MPI_SHORT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(&_bestMove.direction, 1, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
		MPI_Send(&leaves, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(&nodes, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
	}

}


/*
 * Alpha/Beta search
 *
 * - first, start with principal variation
 * - depending on depth, we only do depth search for some move types
 */
int ABIDStrategy::alphabeta(int depth, int alpha, int beta)
{
    int currentValue = -14999+depth, value;
    Move m;
    MoveList list;
    bool depthPhase, doDepthSearch;
	int nprocs = _sc->getnprocs();
	int myid = _sc->getmyid();

    /* We make a depth search for the following move types... */
    int maxType = (depth < _currentMaxDepth-1)  ? Move::maxMoveType :
	          (depth < _currentMaxDepth)    ? Move::maxPushType :
	                                          Move::maxOutType;

    _board->generateMoves(list);

    if (_sc && _sc->verbose()) {
	    char tmp[100];
	    sprintf(tmp, "Proc %d: Alpha/Beta [%d;%d], %d moves (%d depth)",myid, alpha, beta,list.count(Move::none), list.count(maxType));
	  //  _sc->startedNode(depth, tmp);
    }

    /* check for an old best move in principal variation */
    if (_inPV) {
	m = _pv[depth];

	if ((m.type != Move::none) &&
	    (!list.isElement(m, 0, true)))
	    m.type = Move::none;

	if (m.type == Move::none) _inPV = false;
    }

    // first, play all moves with depth search
    depthPhase = true;

    while (1) {

	// get next move
	if (m.type == Move::none) {
            if (depthPhase)
		depthPhase = list.getNext(m, maxType);
            if (!depthPhase)
		if (!list.getNext(m, Move::none)) break;
	}
	// we could start with a non-depth move from principal variation
	doDepthSearch = depthPhase && (m.type <= maxType);

	_board->playMove(m);

	/* check for a win position first */
	if (!_board->isValid()) {

	    /* Shorter path to win position is better */
	    value = 14999-depth;
	}
	else {

            if (doDepthSearch) {
		/* opponent searches for its maximum; but we want the
		 * minimum: so change sign (for alpha/beta window too!)
		 */
		value = -alphabeta(depth+1, -beta, -alpha);
            }
            else {
		value = evaluate();
	    }
	}

	_board->takeBack();

	/* best move so far? */
	if (value > currentValue) {
	    currentValue = value;
	    _pv.update(depth, m);

	    if (_sc) _sc->foundBestMove(depth, m, currentValue);
	    if (depth == 0)
		    _currentBestMove = m;

	    /* alpha/beta cut off or win position ... */
	    if (currentValue>14900 || currentValue >= beta) {
		if (_sc) _sc->finishedNode(depth, _pv.chain(depth));
		return currentValue;
	    }

	    /* maximize alpha */
	    if (currentValue > alpha) alpha = currentValue;
	}

	if (_stopSearch) break; // depthPhase=false;
	m.type = Move::none;
    }
    
    if (_sc) _sc->finishedNode(depth, _pv.chain(depth));

    return currentValue;
}

// register ourself
ABIDStrategy abidStrategy;
