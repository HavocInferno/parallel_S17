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

#define WORK 1
#define DIE 2
#define MAXPVDEPTH 10
class ABIDStrategy: public SearchStrategy
{
 public:
  ABIDStrategy(): SearchStrategy("ABID", 2) {}
  SearchStrategy* clone() { return new ABIDStrategy(); }

  Move& nextMove() { return _pv[1]; }

 private:
  void searchBestMove();
  /* recursive alpha/beta search */
  int alphabeta(int depth, int alpha, int beta);
  int alphabetaworker(int depth, int alpha, int beta);
  void enterSlave();
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
void ABIDStrategy::enterSlave()
{
  MPI_Status status;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // init local values needed for calculation
  char board [500];
  int depth = 0;
  int maxType;
  int value;
  bool lol;

  int alpha, beta;
  int type;
  short field;
  unsigned char dir;
  bool doDepthSearch;

  while (1)
  {
    MPI_Recv(&board, 500, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    
    if (status.MPI_TAG==DIE)
      break; // kills process
    _board->setState(board);
    MPI_Recv(&depth, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    MPI_Recv(&_currentMaxDepth, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    MPI_Recv(&maxType, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    
    
    
    while(1)
    {
      
      // set move to move to be evaluated
      MPI_Recv(&type, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      if (status.MPI_TAG==DIE)
      {
        // send back best move
        break; // only breaks out of loop for this board position, process still alive and awaits next board
      }
      MPI_Recv(&dir, 1, MPI_UNSIGNED_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      MPI_Recv(&field, 1, MPI_SHORT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

      Move::MoveType type = (Move::MoveType) type; 
      Move move (field, dir , type);

      // receive alpha beta

      MPI_Recv(&alpha, 1, MPI_INT, 0, WORK, MPI_COMM_WORLD, &status);
      MPI_Recv(&beta, 1, MPI_INT, 0, WORK, MPI_COMM_WORLD, &status);
      MPI_Recv(&value, 1, MPI_INT, 0, WORK, MPI_COMM_WORLD, &status);
      MPI_Recv(&doDepthSearch, 1, MPI::BOOL, 0, WORK, MPI_COMM_WORLD, &status);
      
      _board->playMove(move);
      
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
  
      // send new alpha beta (this should be able to update? LOOK UP) to root. Update: Eval should do it, as root is able to update alpha beta itself.
      // TODO: send values expected from master. 
    }
  }
}

/* this function manages mainly the iterative deepening. only master process needs to execute this.  */
void ABIDStrategy::searchBestMove()
{    
  int alpha = -15000, beta = 15000;
  int nalpha, nbeta, currentValue = 0;

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
        char tmp[100];
        sprintf(tmp, "Alpha/Beta [%d;%d] with max depth %d", alpha, beta, _currentMaxDepth);
        _sc->substart(tmp);
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
    
  _bestMove = _currentBestMove;
}


/*
 * Alpha/Beta search
 *
 * - first, start with principal variation
 * - depending on depth, we only do depth search for some move types
 */

// we need two versions of this function. one for the root process, which distributes jobs, and one for the workers, who must not distribute jobs. you could either use if (myid==0) rootprocess else worker or 
// use two different functions, which i decided to do.

// implementation for master process. distributes work to workers.
int ABIDStrategy::alphabeta(int depth, int alpha, int beta)
{
  int currentValue = -14999+depth, value;
  Move m;
  MoveList list;
  bool depthPhase, doDepthSearch;
    
  /* We make a depth search for the following move types... */
  int maxType = (depth < _currentMaxDepth-1)  ? Move::maxMoveType :
      (depth < _currentMaxDepth)    ? Move::maxPushType :
      Move::maxOutType;
    
  _board->generateMoves(list);
    
  if (_sc && _sc->verbose()) {
    char tmp[100];
    sprintf(tmp, "Alpha/Beta [%d;%d], %d moves (%d depth)", alpha, beta,
            list.count(Move::none), list.count(maxType));
    _sc->startedNode(depth, tmp);
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
  // evaluation of first child by master, so that we move down the tree to start pvsplit. adjustments needed?
  
  
  // get next move
  if (m.type == Move::none) {
    if (depthPhase)
      depthPhase = list.getNext(m, maxType);
    if (!depthPhase)
    {
      list.getNext(m, Move::none);
      fprintf(stderr, "Reaches nodepthphase for pv.\n");
      //if (!list.getNext(m, Move::none)) break;
      // no loop to break out. does code actually reach this? we need a new move, though, so this might work.
    }
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
  
  // end of evaluation of first element, we are now at the bottom of the tree and can start pvsplit
  

  
  // seed processes
  int i;
  char board [500];
  MPI_Status status;
  Move tmpMoveChain[MAXPVDEPTH];// maximum size according to variation
  Move* WorkerMoves  = new Move [_sc->getnprocs()];
  bool* activeWorkers = new bool [_sc->getnprocs()]; // in case there are less workers than moves, we want to break early when gathering the final values
  strncpy(board, _board->getState(), 500);
  for (i = 1; i < _sc->getnprocs(); i++) {
    // distribute board
    MPI_Send(&board, 500, MPI_CHAR, i, WORK, MPI_COMM_WORLD);

    // send depth and maxdepth
    MPI_Send(&depth, 1, MPI_INT, i, WORK, MPI_COMM_WORLD);
    MPI_Send(&_currentMaxDepth, 1, MPI_INT, i, WORK, MPI_COMM_WORLD);

    // send maxtype

    MPI_Send(&maxType, 1, MPI_INT, i, WORK, MPI_COMM_WORLD);

    // get next move
    if (depthPhase)
      depthPhase = list.getNext(m, maxType);
    if (!depthPhase)
      if (!list.getNext(m, Move::none))
      {
        // send all remaining processes stop signal, get out of seeding loop
        for (; i<_sc->getnprocs();i++)
          MPI_Send (0, 1, MPI_INT, i, DIE, MPI_COMM_WORLD);
        break;
      }
    // send to process i
    activeWorkers[i]=true;
    WorkerMoves[i] = m; // store distributed move, might be best move
    MPI_Send(&m.type, 1, MPI_INT, i, WORK, MPI_COMM_WORLD);
    MPI_Send(&m.direction, 1, MPI_UNSIGNED_CHAR, 0, WORK, MPI_COMM_WORLD);
    MPI_Send(&m.field, 1, MPI_SHORT, 0, WORK, MPI_COMM_WORLD);
    MPI_Send(&alpha, 1, MPI_INT, i, WORK, MPI_COMM_WORLD);
    MPI_Send(&beta, 1, MPI_INT, i, WORK, MPI_COMM_WORLD);
    MPI_Send(&value, 1, MPI_INT, i, WORK, MPI_COMM_WORLD);
    MPI_Send(&doDepthSearch, 1, MPI::BOOL, i, WORK, MPI_COMM_WORLD);
  }


  
  while (1) {
    
    // get next move
    if (depthPhase)
      depthPhase = list.getNext(m, maxType);
    if (!depthPhase)
      if (!list.getNext(m, Move::none)) {fprintf(stderr, "Exiting\n"); break;}
      

    
    // we could start with a non-depth move from principal variation
    doDepthSearch = depthPhase && (m.type <= maxType);

   // at this point, every process does some work. 1. listen on input. 2. send new move back
    
    MPI_Recv(&value, 1, MPI_INT, MPI_ANY_SOURCE, WORK, MPI_COMM_WORLD, &status);
    MPI_Recv(&tmpMoveChain, sizeof(Move)*MAXPVDEPTH, MPI_BYTE, status.MPI_SOURCE, WORK, MPI_COMM_WORLD, &status);
    
    // replace by work distribution
 
    MPI_Send(&m.type, 1, MPI_INT, status.MPI_SOURCE, WORK, MPI_COMM_WORLD);
    MPI_Send(&m.direction, 1, MPI_UNSIGNED_CHAR, status.MPI_SOURCE, WORK, MPI_COMM_WORLD);
    MPI_Send(&m.field, 1, MPI_SHORT, status.MPI_SOURCE, WORK, MPI_COMM_WORLD);
    MPI_Send(&alpha, 1, MPI_INT, status.MPI_SOURCE, WORK, MPI_COMM_WORLD);
    MPI_Send(&beta, 1, MPI_INT, status.MPI_SOURCE, WORK, MPI_COMM_WORLD);
    MPI_Send(&value, 1, MPI_INT, status.MPI_SOURCE, WORK, MPI_COMM_WORLD);
    MPI_Send(&doDepthSearch, 1, MPI::BOOL, status.MPI_SOURCE, WORK, MPI_COMM_WORLD);
    
    
    // currently, value is at correct value, but move is the new distributed move. replace by evaluated move(chain)
    m = WorkerMoves[status.MPI_SOURCE];
    
    
    /* best move so far? */
    if (value > currentValue) {
      currentValue = value;
      _pv.update(depth, m);
      memcpy(&(_pv[depth]), &tmpMoveChain, sizeof(Move) * MAXPVDEPTH);
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


  // gather all values
  for (i = 1; i < _sc->getnprocs(); i++)
  {
    // only gather values from active workers obviously
    if (activeWorkers[i])
    {
      MPI_Recv(&value, 1, MPI_INT, i, WORK, MPI_COMM_WORLD, &status);
      MPI_Recv(&tmpMoveChain, sizeof(Move)*MAXPVDEPTH, MPI_BYTE, i, WORK, MPI_COMM_WORLD, &status);
      m = WorkerMoves[i];
          /* best move so far? */
      if (value > currentValue) {
        currentValue = value;
        _pv.update(depth, m);
        memcpy(&(_pv[depth]), &tmpMoveChain, sizeof(Move) * MAXPVDEPTH);
        if (_sc) _sc->foundBestMove(depth, m, currentValue);
        if (depth == 0)
          _currentBestMove = m;
        
        /* No need to cut off, as values are already calculated anyway
        // alpha/beta cut off or win position ... 
        if (currentValue>14900 || currentValue >= beta) {
          if (_sc) _sc->finishedNode(depth, _pv.chain(depth));
        return currentValue;
        }
        
        // maximize alpha 
        if (currentValue > alpha) alpha = currentValue;
        */
      }
      
      
    }
  }
  if (_sc) _sc->finishedNode(depth, _pv.chain(depth));
  delete[] WorkerMoves;
  delete[] activeWorkers;
  return currentValue;
}

// alphabeta for workers. does evaluate a given board, to be called from slave process. board is set within slaveprocess, just as the move.
int ABIDStrategy::alphabetaworker(int depth, int alpha, int beta)
{
  int currentValue = -14999+depth, value;
  Move m;
  MoveList list;
  bool depthPhase, doDepthSearch;
    
  /* We make a depth search for the following move types... */
  int maxType = (depth < _currentMaxDepth-1)  ? Move::maxMoveType :
      (depth < _currentMaxDepth)    ? Move::maxPushType :
      Move::maxOutType;
    
  _board->generateMoves(list);
    
  if (_sc && _sc->verbose()) {
    char tmp[100];
    sprintf(tmp, "Alpha/Beta [%d;%d], %d moves (%d depth)", alpha, beta,
            list.count(Move::none), list.count(maxType));
    _sc->startedNode(depth, tmp);
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
