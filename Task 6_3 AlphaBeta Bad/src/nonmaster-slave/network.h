/*
 * Simple Network Communication via event-driven loop
 *
 * (C) 2005, Josef Weidendorfer
 *
 * Install a communication domain which is a listening socket
 * on a port and a set of remote connection to this port.
 * You can register callbacks for incoming ASCII data on
 * communcation domains and broadcast your own data into a domain.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <sys/types.h>
#include <netinet/in.h>

class NetworkDomain;
class NetworkTimer;

/**
 * Event loop for network communication
 */
class NetworkLoop
{
 public:
    NetworkLoop();

    /**
     * Install a listening socket on the port given by the NetworkDomain.
     * Incoming connections are accepted, and incoming
     * data will lead to callbacks specified in the NetworkDomain.
     * Returns true if successfull.
     */
    bool install(NetworkDomain*);

    /**
     * Install a oneshot timer object. Install the timer again in
     * timeout() for regular calls.
     */
    bool install(NetworkTimer*);

    /**
     * Remove a NetworkDomain. Existing connections will be closed.
     */
    void remove(NetworkDomain*);

    /**
     * Blocks in a select() on network connections and
     * calls registered callbacks on incoming data.
     * Call exit() to leave the event loop with given exit value.
     */
    int run();

    /**
     * While running inside of a event loop via run(),
     * call this function the trigger quitting the loop, i.e.
     * returning from run().
     */
    void exit(int value = 1);

    /**
     * Are incoming data pending?
     */
    bool pending();

    /**
     * Process pending data
     */
    void processPending();


 private:
    NetworkDomain* domainList;
    NetworkTimer* timerList;
    bool exit_loop;
    int exit_value;

    /* Set of file descriptors used in select.
     * This includes listening and data sockets
     */
    fd_set readfds;
    int max_readfd;
};


/**
 * A timer to be used with NetworkLoop.
 *
 * Install an instance into the loop to get timeout() called
 * after <secs> seconds.
 */
class NetworkTimer
{
 public:
    NetworkTimer(int msecs);
    virtual ~NetworkTimer() {}

    int msecs() { return _msecs; }

    virtual void timeout(NetworkLoop*);

    /**
     * Internal.
     * Helpers for NetworkLoop
     */    
    void reset();
    void set(struct timeval*);
    void minLeft(struct timeval*);
    bool subLeft(struct timeval*);

    NetworkTimer* next;

 private:
    int _msecs;
    struct timeval _left;
};

/** 
 * An active connection in a communication domain
 */
class Connection {
 public:
    Connection(NetworkDomain*, Connection*,
	       const char*, int,
	       struct sockaddr_in, bool);
  
    ~Connection();

    void setHost(const char* h);
    
    /**
     * Send a string to this connection.
     */
    bool sendString(const char* str, int len);

    /* Get string for remote end */
    char* addr();

    /**
     * Internal.
     * Called from NetworkDomain to send registration string
     */
    bool start();

  
    char host[100];
    int port;
    struct sockaddr_in sin;
    bool reachable;

    NetworkDomain* domain;
    Connection* next;
};


/**
 * The Network domainclass can be subclassed to send and to
 * receive multicasts from other Network instances in
 * other processes/machines.
 */
class NetworkDomain
{
 public:
  enum { defaultPort = 23412 };

  /* install listening TCP socket on port */
  NetworkDomain(int port = defaultPort);
  virtual ~NetworkDomain();

  bool isListening() { return (fd>=0); }

  /**
   * Returns the file descriptor for the listening socket
   */
  int startListening(NetworkLoop*);

  /**
   * Close all connections and listening socket
   */
  void close();

  int ID() { return myID; }
  int listeningPort() { return myPort; }
  int listeningFD() { return fd; }
  void addConnection(const char* host, int port);  
  void broadcast(const char* str);

  /* return number of connections */
  int count();

  /* For list of domains used in a NetworkLoop.
   * Can be done this way as a NetworkDomain can only be used
   * in one NetworkLoop at a time
   */
  NetworkDomain* next;

  /**
   * Internal use.
   * NetworkLoop asks to check for pending connections and data
   */
  void check(fd_set*);

  /**
   * Internal use.
   * Overwrite this to react on new connections
   */
  virtual void newConnection(Connection*);

 protected:
  /* overwrite this in your subclass to receive
   * strings broadcasted */
  virtual void received(char* str);
  Connection* prepareConnection(const char* host, int port);  

 private:
  void gotConnection();
  /* factory for connections to not get multiply connections to one target */
  Connection* getNewConnection(const char* h, struct sockaddr_in sin);

  NetworkLoop* loop;
  Connection* connectionList;
  struct sockaddr_in mySin;
  int fd, myID, myPort;
};

#endif

  
