/*
 * Search the game play tree for move leading to
 * position with highest evaluation
 *
 * (c) 2005, Josef Weidendorfer
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include "board.h"
#include "search.h"
#include "eval.h"



/// SearchCallbacks

static struct timeval t1, t2;
static int kevalsPerSec = 30;
static int evalCounter = 0;

void SearchCallbacks::start(int msecsForSearch)
{
    _leavesVisited = 0;
    _nodesVisited = 0;
    gettimeofday(&t1,0);

    evalCounter = 0;
    _msecsForSearch = msecsForSearch;

    if (!_verbose) return;

    if (_msecsForSearch>0)
	printf(" Search started (ca. %d.%03d secs) ...\n",
	       _msecsForSearch / 1000, _msecsForSearch % 1000);
    else
	printf(" Search started ...\n");
}

void SearchCallbacks::substart(char* s)
{
    if (!_verbose) return;

    printf(" Subsearch %s ...\n", s);
}

void SearchCallbacks::finished(Move& m)
{
    gettimeofday(&t2,0);
    _msecsPassed =
	(1000* t2.tv_sec + t2.tv_usec / 1000) -
	(1000* t1.tv_sec + t1.tv_usec / 1000);

    if (!_verbose) return;

    if (_msecsPassed <1) _msecsPassed = 1;
    if (_nodesVisited<1) _nodesVisited = 1;

    printf(" Search finished after %d.%03d secs\n",
	   _msecsPassed / 1000, _msecsPassed % 1000);
    printf("  Found move '%s'\n", m.name());
    printf("  Leaves visited: %d (%d k/s) \n",
	   _leavesVisited, _leavesVisited /_msecsPassed);
    printf("  Nodes visited: %d (%d leaves per node)\n",
	   _nodesVisited, _leavesVisited/_nodesVisited );
    
    int eps = _leavesVisited /_msecsPassed;
    if (eps>kevalsPerSec) kevalsPerSec = eps;
}

bool SearchCallbacks::afterEval()
{
    _leavesVisited++;

    int ms = _msecsForSearch / 3;
    if (ms > 500) ms = 500;
    if (ms < 200) ms = 200;

    evalCounter++;

    if (evalCounter < ms * kevalsPerSec) return false;
    
    // FIXME: Check for network events

    evalCounter = 0;

    gettimeofday(&t2,0);
    _msecsPassed =
	(1000* t2.tv_sec + t2.tv_usec / 1000) -
	(1000* t1.tv_sec + t1.tv_usec / 1000);

    if (_msecsPassed <1) _msecsPassed = 1;
    if (_nodesVisited<1) _nodesVisited = 1;

    int eps = _leavesVisited / _msecsPassed;
    if (_verbose)
	printf(" EvalRate %d k/s (%d evals, %d msecs)\n",
	       eps, _leavesVisited, _msecsPassed);
    if (eps>kevalsPerSec) kevalsPerSec = eps;

    if ((_msecsForSearch > 0) && (_msecsPassed > _msecsForSearch)) {
	printf(" Stop!\n");
	return true;
    }

    return false;
}

void SearchCallbacks::foundBestMove(int d, const Move& m, int value)
{
    if (d+2 >= _verbose) return;

    static const char* spaces = "                     ";
    printf(" %sNew best move '%s', value %d ...\n",
	   spaces+20-d, m.name(), value);
}

int _leaveStart[10], _nodeStart[10];

void SearchCallbacks::startedNode(int d, char* s)
{
    if (d+1 >= _verbose) return;

    if (d<10) {
	_leaveStart[d] = _leavesVisited;
	_nodeStart[d] = _nodesVisited;
    }

    static const char* spaces = "                     ";
    printf(" %sStarted node at depth %d (%s)\n", spaces+20-d, d, s);
}

void SearchCallbacks::finishedNode(int d, Move* chain)
{
    _nodesVisited++;

    if (d+1 >= _verbose) return;

    static const char* spaces = "                     ";
    printf(" %sFinished node at depth %d", spaces+20-d, d);
    if (chain) {
	int c = 0;
	while( (c<4) && (chain->type != Move::none)) {
	    printf("%s%s", (c==0) ? " (" : ", ", chain->name());
	    chain++;
	    c++;
	}
	printf(")");
    }
    printf("\n");

    if (d<10) {
	int l = _leavesVisited - _leaveStart[d];
	int n = _nodesVisited - _nodeStart[d];
	if (n<1) n=1;
	printf(" %s %d leaves, %d nodes visited (%d leaves per node)\n", spaces+20-d,
	       l,n, l/n);
    }
}





/// SearchStrategy


static SearchStrategy* searchStrategies = 0;

SearchStrategy::SearchStrategy(const char* n, int prio)
{
    _maxDepth = 1;
    _sc = 0;
    _ev = 0;
    _name = n;
    _next = 0;
    _prio = prio;

    // register if not already registered
    SearchStrategy *ss = searchStrategies, *prev = 0;
    while(ss) {
	if (strcmp(ss->_name, n) == 0) return;	
	if (ss->_prio > prio) break;
	prev = ss;
	ss = ss->_next;
    }

    if (prev) {
	_next = prev->_next;
	prev->_next = this;
    }
    else {
	_next = searchStrategies;
	searchStrategies = this;
    }
}

const char** SearchStrategy::strategies()
{
    static const char** sslist = 0;

    int count = 0;
    SearchStrategy* ss = searchStrategies;
    while(ss) {
	count++;
	ss = ss->_next;
    }

    sslist = (const char**) realloc(sslist, sizeof(char*) * (count+1) );

    count = 0;
    ss = searchStrategies;
    while(ss) {
	sslist[count] = ss->_name;
	count++;
	ss = ss->_next;
    }
    sslist[count] = 0;

    return sslist;
}

SearchStrategy* SearchStrategy::create(char* n)
{
    SearchStrategy* ss = searchStrategies;
    while(ss) {
	if (strcmp(ss->_name, n)==0) return ss;
	ss = ss->_next;
    }
    return 0;
}

SearchStrategy* SearchStrategy::create(int i)
{
    int count = 0;
    SearchStrategy* ss = searchStrategies;
    while(ss) {	
	if (count == i) return ss;
	count++;
	ss = ss->_next;
    }
    return 0;
}

Move& SearchStrategy::bestMove(Board* b)
{
    if (_sc) {
	int ms = b->msecsToPlay(b->actColor());
	if (ms>0) {
	    int minTokens = b->getColor1Count();
	    int tokens = b->getColor2Count();
	    if (tokens < minTokens) minTokens = tokens;
	    int mvs = 60 - 10*(14-minTokens);
	    ms = ms/mvs;
	}
	_sc->start(ms);
    }

    _board = b;
    _bestMove.type = Move::none;
    _stopSearch = false;

    searchBestMove();

    if (_sc) _sc->finished(_bestMove);

    return _bestMove;
}

Move& SearchStrategy::nextMove()
{
    static Move m;
    return m; // returns invalid
}

int SearchStrategy::evaluate()
{
    int v = _ev->calcEvaluation(_board); 
    if (_sc) _stopSearch = _sc->afterEval();

    return v;
}

int SearchStrategy::minEvaluation()
{
    return _ev ? _ev->minValue() : -1; 
}

int SearchStrategy::maxEvaluation()
{
    return _ev ? _ev->maxValue() : -1; 
}

void SearchStrategy::generateMoves(MoveList& list)
{
    if (_board)
	_board->generateMoves(list); 
}

void SearchStrategy::playMove(const Move& m)
{
    if (_board)
	_board->playMove(m);
}

bool SearchStrategy::takeBack()
{
    return _board ? _board->takeBack() : false; 
}

void SearchStrategy::foundBestMove(int d, const Move& m, int eval)
{
    if (d==0) _bestMove = m;
    if (_sc)
	_sc->foundBestMove(d, m, eval);
}

void SearchStrategy::finishedNode(int d, Move* bestList)
{
    if (_sc)
	_sc->finishedNode(d, bestList);
}
