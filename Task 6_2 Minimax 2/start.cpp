/**
 * Send a start position into a game communication channel,
 * and observe positions sent in the channel
 *
 * (C) 2005, Josef Weidendorfer
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "board.h"
#include "network.h"

/* Global, static vars */
static NetworkLoop l;
static Board b;

/* Start game? */
static bool start = true;

/* only show reachable positions with valid moves */
static bool onlyReachable = true;

/* time limit in seconds (0: no time limit, -1: not set) */
static int secsToPlay = -1;

/* to set verbosity of NetworkLoop implementation */
extern int verbose;

#define DEFAULT_DOMAIN_PORT 23412

/* remote channel */
static char* host = 0;       /* not used on default */
static int rport = DEFAULT_DOMAIN_PORT;

/* local channel */
static int lport = DEFAULT_DOMAIN_PORT;

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
    printf("%s", tmp+4);
    int state = b.validState();
    printf("%s\n", Board::stateDescription(state));
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
	    if (onlyReachable) return;
	}
	else {
	    if (b.actColor() == Board::color1)
		printf("O draws move '%s'...\n", m.name());
	    else
		printf("X draws move '%s'...\n", m.name());
	}
    }

    b.setState(str+4);
    printf("%s", str+4);
    int state = b.validState();
    printf("%s\n", Board::stateDescription(state));

    switch(state) {
	case Board::timeout1:
	case Board::timeout2:
	case Board::win1:
	case Board::win2:
	    l.exit();
	default:
	    break;
    }
}

void MyDomain::newConnection(Connection* c)
{
    NetworkDomain::newConnection(c);

    static char tmp[500];
    int len = sprintf(tmp, "pos %s\n", b.getState());
    c->sendString(tmp, len);
}

static void printHelp(char* prg, bool printHeader)
{
    if (printHeader)
	printf("Start V 0.2 - (C) 2005 Josef Weidendorfer\n"
	       "Broadcast a game position, observe the game and quit in winning state.\n\n");
    
    printf("Usage: %s [options] [<file>|-]\n\n"
	   "  <file>           File containing start position (default: start position)\n"
	   "  -                Position is read from standard input\n\n",
	   prg);
    printf(" Options:\n"
	   "  -h / --help      Print this help text\n"
	   "  -v / -vv         Be verbose / more verbose\n"
	   "  -o               Only observe, no start\n"
	   "  -t <timeToPlay>  Start in tournament modus (limited time)\n"
	   "  -a               Show all positions (default: only via valid moves)\n"
	   "  -p [host:][port] Connection to broadcast channel\n"
	   "                   (default: %d)\n\n", DEFAULT_DOMAIN_PORT);
    exit(1);
}

static void parseArgs(int argc, char* argv[])
{
    int arg=0;
    while(arg+1<argc) {
	arg++;
	if (argv[arg][0] == '-') {
	    if (strcmp(argv[arg],"-h")==0 ||
		strcmp(argv[arg],"--help")==0) printHelp(argv[0], true);
	    if (strcmp(argv[arg],"-v")==0) {
		verbose = 1;
		continue;
	    }
	    if (strcmp(argv[arg],"-vv")==0) {
		verbose = 2;
		continue;
	    }
	    if (strcmp(argv[arg],"-o")==0) {
		start = false;
		continue;
	    }
	    if (strcmp(argv[arg],"-a")==0) {
		onlyReachable = false;
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
		if (argv[arg][0]>'0' && argv[arg][0]<='9') {
		    lport = atoi(argv[arg]);
		    continue;
		}
		char* c = strrchr(argv[arg],':');
		int p = 0;
		if (c != 0) {
		    *c = 0;
		    p = atoi(c+1);
		}
		host = argv[arg];
		if (p) rport = p;
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
}

int main(int argc, char* argv[])
{
    parseArgs(argc, argv);
    b.setVerbose(verbose);

    MyDomain d(lport);
    l.install(&d);

    if (host) d.addConnection(host, rport);

    if (start) {
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
	    b.setMSecsToPlay(Board::color1, 1000 * secsToPlay);
	    b.setMSecsToPlay(Board::color2, 1000 * secsToPlay);
	}
	d.sendBoard();
    }
    return l.run();
}
