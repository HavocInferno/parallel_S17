#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "network.h"

// set to 1 to get debug messages
int verbose = 0;

// NetworkLoop

NetworkLoop::NetworkLoop()
{
    domainList = 0;
    timerList = 0;

    max_readfd = -1;
    FD_ZERO(&readfds);
}

bool NetworkLoop::install(NetworkDomain* d)
{
    if (d->isListening()) return false;

    int fd = d->startListening(this);

    if (fd>=0) {
	FD_SET(fd, &readfds);
	if (fd>max_readfd) max_readfd=fd;
    }

    if (verbose>1)
	printf("NetworkLoop::install: Callbacks for NetworkDomain %d\n",
	       d->ID());

    d->next = domainList;
    domainList = d;

    return true;
}

bool NetworkLoop::install(NetworkTimer* t)
{
    t->reset();
    t->next = timerList;
    timerList = t;

    if (verbose>1)
	printf("NetworkLoop::install: Timer with %d msecs\n", t->msecs());

    return true;
}

void NetworkLoop::remove(NetworkDomain* domain)
{
    NetworkDomain *d, *prev=0;
    for(d=domainList; d!=0; prev=d, d=d->next)
	if (d == domain) break;
    if (d==0) return;

    int fd = d->listeningFD();
    if (fd>=0) {
	FD_CLR(fd, &readfds);

	while(max_readfd>=0) {
	    if (FD_ISSET(max_readfd, &readfds)) break;
	    max_readfd--;
	}
    }	

    d->close();
    if (prev) prev->next = d->next;
    else domainList = d->next;
    d->next = 0;
}

/* subtrace tv2 from tv1 */
void subTimeval(struct timeval* tv1, struct timeval* tv2)
{
    tv1->tv_sec -= tv2->tv_sec;
    if (tv1->tv_usec < tv2->tv_usec) {
	tv1->tv_sec --;
	tv1->tv_usec += 1000000 - tv2->tv_usec;
    }
    else
	tv1->tv_usec -= tv2->tv_usec;
}    

int NetworkLoop::run()
{
    struct timeval tv, *ptv, tv2;
    NetworkTimer *t, *tprev, *tnext;

    tv2.tv_sec = 0;
    tv2.tv_usec = 0;
    exit_loop = false;

    if (verbose>1)
	printf("NetworkLoop::run: Waiting for events\n");

    while(!exit_loop) {
	if (timerList==0) ptv = 0;
	else {
	    ptv = &tv;
	    timerList->set(&tv);
	    t = timerList->next;
	    for(;t!=0;t=t->next)
		t->minLeft(&tv);
	    tv2 = tv;
	}

	fd_set rfds = readfds;
	select(max_readfd+1, &rfds, NULL, NULL, ptv);

	if (verbose>1) printf("NetworkLoop::run: Got a event...\n");

	NetworkDomain* d = domainList;
	for(;d!=0;d = d->next)
	    d->check(&rfds);

	if (ptv) {
	    // Warning: Linux specific
	    subTimeval(&tv2, ptv);

	    tprev = 0;
	    t = timerList;
	    for(;t!=0;tprev=t,t=tnext) {
		tnext = t->next;
		if (t->subLeft(&tv2)) {
		    // remove timer, it could be added again in timeout()
		    if (tprev) tprev->next = tnext;
		    else timerList = tnext;

		    if (verbose>1)
			printf("NetworkLoop::run: Timeout\n");
		    t->timeout(this);
		}
	    }
	}
    }

    return exit_value;
}

void NetworkLoop::exit(int v)
{
    exit_loop = true;
    exit_value = v;
}

bool NetworkLoop::pending()
{
    fd_set rfds = readfds;
    select(max_readfd+1, &rfds, NULL, NULL, NULL);

    for(int i = 0;i<=max_readfd;i++)	
	if (FD_ISSET(i, &rfds)) return true;

    return false;
}

void NetworkLoop::processPending()
{
    fd_set rfds = readfds;
    select(max_readfd+1, &rfds, NULL, NULL, NULL); 

    NetworkDomain* d = domainList;
    for(;d!=0;d = d->next)
	d->check(&rfds);
}


/// NetworkTimer

NetworkTimer::NetworkTimer(int msecs)
{
    _msecs = msecs;
    next = 0;
}

void NetworkTimer::timeout(NetworkLoop*)
{
    printf("Timeout after %d.%03d secs!\n", _msecs/1000, _msecs%1000);
}   

void NetworkTimer::reset()
{
    _left.tv_sec = _msecs/1000;
    _left.tv_usec = (_msecs%1000)*1000;
}

void NetworkTimer::set(struct timeval* tv)
{
    tv->tv_sec = _left.tv_sec;
    tv->tv_usec = _left.tv_usec;
}

void NetworkTimer::minLeft(struct timeval* tv)
{
    if (_left.tv_sec < tv->tv_sec) {
	tv->tv_sec = _left.tv_sec;
	tv->tv_usec = _left.tv_usec;
	return;
    }
    if (_left.tv_sec > tv->tv_sec) return;
    if (_left.tv_usec > tv->tv_usec) return;
    tv->tv_usec = _left.tv_usec;
}

bool NetworkTimer::subLeft(struct timeval* tv)
{
    if (_left.tv_sec < tv->tv_sec) return true;
    if (_left.tv_sec == tv->tv_sec)
	if (_left.tv_usec <= tv->tv_usec) return true;
 
    subTimeval(&_left, tv);

    return false;
}

/// Connection

Connection::Connection(NetworkDomain* d, Connection* n,
		       const char* h, int p, struct sockaddr_in s,bool r)
{
    setHost(h);
    port = p;
    sin = s;
    reachable = r;

    domain = d;
    next = n;
}

Connection::~Connection()
{
    if (reachable) {
	char tmp[50];
	int len = sprintf(tmp, "unreg %d", domain->listeningPort());
	sendString(tmp, len);
    }
}

void Connection::setHost(const char* h)
{
  if (h==0)
    host[0]=0;
  else {
    int l = strlen(h);
    if (l>99) l=99;
    strncpy(host, h, l);
    host[l] = 0;
  }
}

char* Connection::addr()
{
    static char tmp[256];

    sprintf(tmp, "%s:%d", host[0] ? host:inet_ntoa(sin.sin_addr),
	    ntohs(sin.sin_port));
    return tmp;
}

bool Connection::sendString(const char* str, int len)
{
    if (!reachable) return false;

    int s = ::socket (PF_INET, SOCK_STREAM, 0);
    if (s<0) {
	printf("Connection::sendString: Error in socket()\n");
	return false;
    }
    if (::connect (s, (struct sockaddr *)&sin, sizeof (sin)) <0) {
	if (verbose)
	    printf("Connection::sendString: Cannot connect to %s\n", addr());

	reachable = false;
	return false;
    }
    
    while(len>0) {
	int written = write(s, str, len);
	if (written <= 0) {
		printf("Connection::sendString: Error in write()\n");
		break;
	}
	str += written;
	len -= written;
    }
    ::close(s);
    
    if (verbose>1)
	printf("Connection::sendString: Sent to %s: '%s'\n", addr(), str);
    
    return true;
}

bool Connection::start()
{
    if (verbose)
	printf("Connection::start: %s\n", addr());

    if (domain->listeningPort() == -1) {
	printf("Connection::start: Error: aborted connecting without listening\n");
        return false;
    }

    char tmp[50];
    int len = sprintf(tmp, "reg %d", domain->listeningPort());

    reachable = true;
    if (!sendString(tmp, len)) {
	reachable = false;
	return false;
    }

    domain->newConnection(this);

    return true;
}


/// NetworkDomain

NetworkDomain::NetworkDomain(int id)
{
    myID = id;
    myPort = -1;
    fd = -1;
    loop = 0;
    connectionList = 0;
}

NetworkDomain::~NetworkDomain()
{
  if (loop)
      loop->remove(this);
}

int NetworkDomain::startListening(NetworkLoop* l)
{
    struct sockaddr_in name;
    int i,j;

    if (loop) return -1;
    loop = l;

    fd = ::socket (PF_INET, SOCK_STREAM, 0);
    if (fd<0) return fd;
 
    for(i = 0; i<5;i++) {
	myPort = myID + i;
	name.sin_family = AF_INET;
	name.sin_port = htons (myPort);
	name.sin_addr.s_addr = htonl (INADDR_ANY);
	if (bind (fd, (struct sockaddr *) &name, sizeof (name)) >= 0)
	    break;
	if (verbose)
	    printf("NetworkDomain::startListening: Port %d in use\n", myPort);
    }
    mySin = name;
    if (verbose)
	printf("NetworkDomain::startListening: Using Port %d\n", myPort);

    if (i==5) {
	printf("NetworkDomain::startListening: Error starting domain %d\n",
	       myID);
	::close(fd);
	fd = -1;
	myPort = -1;
	return fd;
    }

    // connect to all instances of this domain on localhost
    for(j = 0; j<5;j++) {
	if (j == i) continue;
	prepareConnection("localhost", myID+j);
    }

    if (::listen(fd,5)<0) {
	printf("NetworkDomain::startListening: Error in listen\n");
	::close(fd);
	fd = -1;
	return fd;
    }

    Connection *c;
    for(c=connectionList; c!=0; c=c->next)
	if (!c->reachable)
	    c->start();

    return fd;
}

void NetworkDomain::close()
{
  if (fd<0) return;
  ::close(fd);
  
  Connection *l, *lnext;
  for(l=connectionList; l!=0; l=lnext) {
      lnext = l->next;
      delete l;
  }

  loop = 0;
}

void NetworkDomain::check(fd_set* set)
{
    if (FD_ISSET(fd, set)) gotConnection();
}

Connection* NetworkDomain::getNewConnection(const char* h,
					    struct sockaddr_in sin)
{
    Connection* c;

    for(c=connectionList; c!=0; c=c->next)
	if (c->sin.sin_addr.s_addr == sin.sin_addr.s_addr &&
	    c->sin.sin_port == sin.sin_port) break;

    if (c) {
	if (h) c->setHost(h);
	c->reachable = false;
    }
    else {
	c = new Connection(this, connectionList,
			   h, ntohs(sin.sin_port),
			   sin, false);
	connectionList = c;
    }

    return c;
}
    


void NetworkDomain::gotConnection()
{
  static char tmp[1024];
  int len=0;
  struct sockaddr_in sin;
  socklen_t sz = sizeof (sin);

  if (verbose>1)
      printf("NetworkDomain::GotConnection:\n");

  int s = accept(fd,(struct sockaddr *)&sin, &sz);
  if (s<0) {
    printf(" Error in accept\n");
    return;
  }
  while(read(s, tmp+len, 1)==1) len++;
  ::close(s);
  tmp[len]=0; len++;

  if (verbose>1)
      printf(" Got from %s:%d : '%s'\n",
	     inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), tmp);

  if (strncmp(tmp,"reg ",4)==0) {
    char* col = strrchr(tmp+4,':');
    if (col) {
	*col = 0;
	int port = atoi(col+1);
	if (verbose)
            printf(" Reg of remote connection %s:%d\n", tmp+4, port);
        addConnection(tmp+4, port);
	return;
    }	
    int port = atoi(tmp+4);
    sin.sin_port = htons( port );
    Connection *c = getNewConnection(0, sin);
    c->reachable = true;

    if (verbose)
        printf(" Reg of %s\n", c->addr());

    newConnection(c);

    for(Connection* cc=connectionList; cc!=0; cc=cc->next) {
        if (cc == c) continue;
	if (!cc->reachable) continue;

        char tmp[50];
        int len = sprintf(tmp, "reg %s", cc->addr());
	c->sendString(tmp, len);
    }

    return;
  }

  if (strncmp(tmp,"unreg ",6)==0) {
    int port = atoi(tmp+6);
    sin.sin_port = htons( port );
    Connection *c, *cprev=0;
    for(c=connectionList ; c!=0; cprev=c, c=c->next)
      if (c->sin.sin_addr.s_addr == sin.sin_addr.s_addr &&
	  c->sin.sin_port == sin.sin_port) break;
    if (c==0) {
      printf("Error: UnReg of %s:%d. Not Found\n",
	     inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
      return;
    }
    if (cprev) cprev->next = c->next;
    else connectionList = c->next;

    if (verbose)
        printf("UnReg of %s:%d\n",
	       inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    return;
  }

  received(tmp);
}

int NetworkDomain::count()
{
    int cc = 0;

    for(Connection* c=connectionList; c!=0; c=c->next)
	if (c->reachable) cc++;

    return cc;
}

void NetworkDomain::received(char* str)
{
    printf("NetworkDomain::received: '%s' in domain %d\n",
	   str, ID());
}

void NetworkDomain::newConnection(Connection* c)
{
    if (verbose)
	printf("NetworkDomain::newConnection: %s, now %d active connections\n",
	       c->addr(), count());
}

void NetworkDomain::addConnection(const char* host, int port)
{
    Connection* c = prepareConnection(host,port);
    if (c) c->start();
}

Connection* NetworkDomain::prepareConnection(const char* host, int port)
{
  struct hostent *hostinfo;
  struct sockaddr_in name;

  memset(&name, 0, sizeof(struct sockaddr_in));
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  hostinfo = gethostbyname (host);
  if (hostinfo == NULL) {
    printf ("NetworkDomain::prepareConnection: Error: Unknown host %s.\n",
	    host);
    return 0;
  }
  name.sin_addr = *(struct in_addr *) hostinfo->h_addr;

  return getNewConnection(host, name);
}


void NetworkDomain::broadcast(const char* str)
{
  int len = strlen(str);
  
  for(Connection* c=connectionList; c!=0; c=c->next)
      c->sendString(str, len);
}

