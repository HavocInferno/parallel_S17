/**
 * Referee
 *
 * Send a start position into 2 game communication channels,
 * observe positions sent in one channel, and if valid, broadcast
 * them into other channel.
 * For a time limited game, times are updated by referee itself.
 *
 * (C) 2005, Josef Weidendorfer
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "board.h"
#include "network.h"

/* Global, static vars */
static NetworkLoop l;
static Board b;

/* Time of last draw */
static struct timeval t1;

class MyDomain;
static MyDomain *d1 = 0, *d2 = 0;

/*
 * Do other members in the channels exist at all?
 * Only start time if there are players.
 *
 * Note: We can not distinguish between observers and players,
 * so game always start when another member is seen.
 */
static bool d1MemberExists = false;
static bool d2MemberExists = false;

/* time limit in seconds (0: no time limit) */
static int secsToPlay = 0;
static int msecsToPlay[3] = {0,0,0};

/* to set verbosity of NetworkLoop implementation */
extern int verbose;

/* show board on stdout after each move? */
static int showBoard = 1;

#define DEFAULT_DOMAIN_PORT 23412
#define DEFAULT_DOMAIN_DIFF 50

/* remote channel */
static char* host[2] = {0,0};       /* not used per default */
static int rport[2] = {DEFAULT_DOMAIN_PORT, DEFAULT_DOMAIN_PORT + DEFAULT_DOMAIN_DIFF};

/* local channel */
static int lport[2] = {DEFAULT_DOMAIN_PORT, DEFAULT_DOMAIN_PORT + DEFAULT_DOMAIN_DIFF};

/* Where to read position to broadcast from? (0: start position) */
static FILE* file = 0;

class MyDomain: public NetworkDomain
{
public:
    MyDomain(int p) : NetworkDomain(p) {}

    void sendBoard();

protected:
    void received(char* str);
    void newConnection(Connection*);
};

void MyDomain::sendBoard()
{
    static char tmp[500];
    sprintf(tmp, "pos %s\n", b.getState());
    if (verbose) {
	printf("%s", tmp+4);
	int state = b.validState();
	printf("%s\n", Board::stateDescription(state));
    }
    broadcast(tmp);
}

void MyDomain::received(char* str)
{
    if (strncmp(str, "quit", 4)==0) {
	l.exit();
	return;
    }

    if (strncmp(str, "pos ", 4)!=0) return;

    if (b.validState() != Board::empty) {

	Board newBoard;
	newBoard.setState(str+4);

	Move m = b.moveToReach(&newBoard, false);
	if (m.type == Move::none) {
	    printf("WARNING: Got a board which is not reachable via a valid move !?\n");
	    return;
	}
	else {
	    struct timeval t2;
	    gettimeofday(&t2,0);
	    int msecsPassed =
		(1000* t2.tv_sec + t2.tv_usec / 1000) -
		(1000* t1.tv_sec + t1.tv_usec / 1000);
	    t1 = t2;

	    int* pMSecs;
	    if (b.actColor() == Board::color1) {
		pMSecs = &(msecsToPlay[Board::color1]);
		printf("O");
	    }
	    else {
		pMSecs = &(msecsToPlay[Board::color2]);
		printf("X");
	    }
	    printf(" draws '%s' (after %d.%03d secs)...\n",
		   m.name(), msecsPassed/1000, msecsPassed%1000);

	    if (*pMSecs > msecsPassed)
		*pMSecs -= msecsPassed;
	    else
		*pMSecs = 0;
	}
    }

    b.setState(str+4);
    /* force our objective view regarding time */
    b.setMSecsToPlay(Board::color1, msecsToPlay[Board::color1] );
    b.setMSecsToPlay(Board::color2, msecsToPlay[Board::color2] );

    int state = b.validState();
    if (showBoard)
      printf("%s%s\n", b.getState(), Board::stateDescription(state));
    else
      printf("%s - %s\n", b.getShortState(), Board::stateDescription(state));

    switch(state) {
	case Board::timeout1:
	case Board::timeout2:
	case Board::win1:
	case Board::win2:
	    l.exit();
	default:
	    break;
    }

    /* send to other domain */
    if (d1 == this) if (d2) d2->sendBoard();
    if (d2 == this) if (d1) d1->sendBoard();
}

void MyDomain::newConnection(Connection* c)
{
    NetworkDomain::newConnection(c);

    static char tmp[500];
    int len = sprintf(tmp, "pos %s\n", b.getState());
    c->sendString(tmp, len);

    /* adjust time if this is first connection for the channel */
    if (!d1MemberExists && (this == d1)) {
	d1MemberExists = true;
	gettimeofday(&t1,0);
    }
    if (!d2MemberExists && (this == d2)) {
	d2MemberExists = true;
	gettimeofday(&t1,0);
    }
}

static void printHelp(char* prg, bool printHeader)
{
    if (printHeader)
	printf("Referee V 0.1 - (C) 2005 Josef Weidendorfer\n"
	       "Broadcast a game position into 2 domains, observe moves played in one domain,\n"
	       "and if valid, broadcast to other domain. Stops playing times itself.\n\n");

    printf("Usage: %s [options] [<file>|-]\n\n"
	   "  <file>           File containing game position (default: start position)\n"
	   "  -                Position is read from standard input\n\n",
	   prg);
    printf(" Options:\n"
	   "  -h / --help      Print this help text\n"
	   "  -v / -vv         Be verbose / more verbose\n"
	   "  -n               Do not show board\n"
	   "  -t <timeToPlay>  Start in tournament modus (limited time)\n"
	   "  -p [host:][port] Connection to first (second) broadcast channel\n"
	   "                   (default: %d / %d)\n\n",
	   DEFAULT_DOMAIN_PORT, DEFAULT_DOMAIN_PORT + DEFAULT_DOMAIN_DIFF);
    exit(1);
}

static void parseArgs(int argc, char* argv[])
{
    int domainsSet = 0;

    int arg=0;
    while(arg+1<argc) {
	arg++;
	if (argv[arg][0] == '-') {
	    if (strcmp(argv[arg],"-h")==0 ||
		strcmp(argv[arg],"--help")==0) printHelp(argv[0], true);
	    if (strcmp(argv[arg],"-v")==0)	{
		verbose = 1;
		continue;
	    }
	    if (strcmp(argv[arg],"-vv")==0)	{
		verbose = 2;
		continue;
	    }
	    if (strcmp(argv[arg],"-n")==0) {
	        showBoard = 0;
		continue;
	    }
	    if ((strcmp(argv[arg],"-t")==0) && (arg+1<argc)) {
		arg++;
		secsToPlay = atoi(argv[arg]);
		if (secsToPlay == 0) {
		    printf("%s: WARNING - Ignoring tournament; %d secs to play\n",
			   argv[0], secsToPlay);
		}
		continue;
	    }
	    if ((strcmp(argv[arg],"-p")==0) && (arg+1<argc)) {
		arg++;
		if (domainsSet>1) {
		    printf("%s: WARNING - Domain specification %s ignored.\n",
			   argv[0], argv[arg]);
		    continue;
		}
		if (argv[arg][0]>'0' && argv[arg][0]<='9') {
		    lport[domainsSet] = atoi(argv[arg]);
		    domainsSet++;
		    continue;
		}
		char* c = strrchr(argv[arg],':');
		int p = 0;
		if (c != 0) {
		    *c = 0;
		    p = atoi(c+1);
		}
		host[domainsSet] = argv[arg];
		if (p) rport[domainsSet] = p;
		domainsSet++;
		continue;
	    }
	    if (strcmp(argv[arg],"-")==0) {
		file = stdin;
		continue;
	    }
	    printf("%s: ERROR - Unknown option %s\n", argv[0], argv[arg]);
	    printHelp(argv[0], false);
	}

	file = fopen(argv[arg], "r");
	if (!file) {
	    printf("%s: ERROR - Can not open '%s' for reading start position\n",
		   argv[0], argv[arg]);
	    printHelp(argv[0], false);
	}
	break;
    }

    if (lport[0] == lport[1]) {
	lport[1] = lport[0] + DEFAULT_DOMAIN_DIFF;
	printf("Local port for domain 2 set to %d\n", lport[1]);
    }
}

int main(int argc, char* argv[])
{
    parseArgs(argc, argv);
    b.setVerbose(verbose);

    if (file) {
	char tmp[500];
	int len = 0, c;
	while( len<499 && (c=fgetc(file)) != EOF)
	    tmp[len++] = (char) c;
	tmp[len++]=0;

	if (!b.setState(tmp)) {
	    printf("%s: WARNING - Can not parse given position; using start position\n", argv[0]);
	    b.begin(Board::color1);
	}
    }
    else
	b.begin(Board::color1);

    if (secsToPlay >= 0) {
	msecsToPlay[Board::color1] = 1000 * secsToPlay;
	msecsToPlay[Board::color2] = 1000 * secsToPlay;
    }
    else {
	msecsToPlay[Board::color1] = b.msecsToPlay(Board::color1);
	msecsToPlay[Board::color2] = b.msecsToPlay(Board::color2);
    }
    b.setMSecsToPlay(Board::color1, msecsToPlay[Board::color1] );
    b.setMSecsToPlay(Board::color2, msecsToPlay[Board::color2] );

    /*
     * Register domains at NetworkLoop. Existing members will
     * get sent the board via MyDomain::newConnection
     */
    d1 = new MyDomain(lport[0]);
    l.install(d1);
    d2 = new MyDomain(lport[1]);
    l.install(d2);

    if (host[0]) d1->addConnection(host[0], rport[0]);
    if (host[1]) d2->addConnection(host[1], rport[1]);

    d1MemberExists = d1->count() >0;
    d2MemberExists = d2->count() >0;

    gettimeofday(&t1,0);

    return l.run();
}
