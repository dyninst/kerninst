// loop.C
// kerninstd's main poll() loop
// Doesn't return until it's time to die

// What we wait on:
// 1) requests to hook up with a newly created client process (we do an accept())
// 2) igen messages from already-hooked-up client processes
// 3) timers, for sampling and sending up data to various client processes
//
// Q: when do we die?  It is *essential* that kerninstd dies gracefully, so it
// can do *crucial* things like free allocate kernel memory; it's a disaster
// to, e.g. leak kernel memory!!!

#ifdef i386_unknown_linux2_4
#define _XOPEN_SOURCE 500 // for POLLRDNORM in <poll.h>
#endif

#include <inttypes.h>
#include <sys/socket.h>
#include <stdio.h> // perror
#include <errno.h>

#include <iostream.h>
#include <algorithm> // stl find()

#include "loop.h"
#include "clientConn.h" // each kerninstd client process (igen handle for it)
#include "instrumentationMgr.h"
#include <poll.h>
#include <signal.h>

#include "pdutil/h/rpcUtil.h" // RPC_setup_socket
#include "util/h/Dictionary.h"
#include "pdutil/h/PriorityQueue.h"
#include "util/h/mrtime.h"
#include "util/h/out_streams.h"


static unsigned kerninstdClientPtrHash(kerninstdClient * const &ptr) {
   unsigned long val = (unsigned long)ptr;
   val >>= 2; // low 2 bits of a ptr are rarely interesting
   
   unsigned result = 0;
   while (val) {
      result = (result << 1) + result + (val & 0x3);
      val >>= 2;
   }

   return result;
}

pdvector<client_connection*> clientProcs; // vec of *ptrs* because no copy-ctor
   // (in turn, because igen doesn't make a reliable one, I think)
unsigned clientConnUniqueId = 0;
dictionary_hash<kerninstdClient*, client_connection*> igenHandle2ClientConn(kerninstdClientPtrHash);
extern instrumentationMgr *global_instrumentationMgr;


bool got_signal_to_die = false; // set by signal handler; checked in the main loop

void the_sigint_handler(int, siginfo_t *, void *) {
   dout << "welcome to ^c handler" << endl;
   
   got_signal_to_die = true;
}

bool setup_signal_handler() {
   struct sigaction action;
   action.sa_handler = NULL;
   action.sa_sigaction = &the_sigint_handler;
   sigemptyset(&action.sa_mask);
   action.sa_flags = SA_SIGINFO | SA_RESTART;
      // SA_RESTART -- xparently restart some interrupted syscalls
   
   int result = sigaction(SIGINT, &action, NULL);
   if (result == -1) {
      perror("sigaction");
      return false;
   }
   
   return true;
}

class accept_socket_died {};

bool process_possible_accept(const pollfd &accept_socket_fd) throw(accept_socket_died) {
   // returns true iff a new connection has been made; false if nothing happened
   // throws an exception if the accept socket has died somehow

   bool error = false;
   if (accept_socket_fd.revents & POLLERR)
      error = true;
   if (accept_socket_fd.revents & POLLHUP)
      error = true;
   if (accept_socket_fd.revents & POLLNVAL)
      error = true;
   if (error)
      throw accept_socket_died();
   
   if (accept_socket_fd.revents & POLLRDNORM) {
      //cout << "kerninstd: looks like someone is trying to make a new connection" << endl;
      int fd_returned_by_accept = RPC_getConnect(accept_socket_fd.fd);
      if (fd_returned_by_accept == -1) {
         cerr << "RPC_getConnect (accept()) failed" << endl;
         throw accept_socket_died();
      }

      assert(igenHandle2ClientConn.size() == clientProcs.size());
      
      client_connection *theClient = new client_connection(fd_returned_by_accept,
                                                           clientConnUniqueId++);
      clientProcs += theClient;

      assert(!igenHandle2ClientConn.defines(&theClient->getIGenHandle()));
      igenHandle2ClientConn[&theClient->getIGenHandle()] = theClient;
      
      assert(igenHandle2ClientConn.size() == clientProcs.size());
      
//        cout << "Looks like an igen connection has been established successfully!!!"
//             << endl;
      
      return true;
   }

   return false; // nothing interesting happened
}

static void fry_client_by_ndx(unsigned ndx) {
   client_connection *theClient = clientProcs[ndx];

   // Uninstrument stuff related to this client
   global_instrumentationMgr->clientIsGoingDownNow(*theClient);
   
   kerninstdClient &theIGenHandle = theClient->getIGenHandle();
   kerninstdClient *theIGenHandlePtr = &theIGenHandle;

   assert(igenHandle2ClientConn.defines(theIGenHandlePtr));
   igenHandle2ClientConn.undef(theIGenHandlePtr);

   delete theClient;
   theClient = NULL; // help purify find mem leaks
   
   clientProcs[ndx] = clientProcs.back();
   clientProcs.shrink(clientProcs.size()-1);
}

// internalEvent is a base class, so we must use pointers
PriorityQueue<mrtime_t, const internalEvent*> oneTimeInternalEvents;

void doOneTimeInternalEventInTheFuture(const internalEvent *e) {
   // internalEvent is a base class...
   // NOTE: "e" *MUST* have been allocated with "new"
   oneTimeInternalEvents.add(e->getWhenToDo(), e);
}

static int nsec2msec_roundup(uint64_t nsecs) {
   const bool needs_roundup = (nsecs % 1000000) > 0;
   const uint64_t result = nsecs / 1000000 + (needs_roundup ? 1 : 0);
   return (int)result;
}

void kerninstd_mainloop() {
   // Create a socket that we'll accept() on.
   // NOTE: We don't specify a port; the system
   int accept_fd;
   int accept_portnum = RPC_setup_socket(accept_fd, // filled in
                                         PF_INET, // family
                                         SOCK_STREAM // type
                                         );
   if (accept_portnum == -1) {
      cerr << "RPC_setup_socket failed" << endl;
      return;
   }

   cout << "The chosen accept port number is " << accept_portnum << endl;
   cout << "Kerninstd ready." << endl;

   // Main loop: poll() on 3 things:
   // a) new accept() connections
   // b) igen msgs from client(s)
   // c) sampling that we should do for client(s)
   // d) one-time internal actions such as freeing springboards or code patches,
   //    that have been delayed for safety purposes.

   if (!setup_signal_handler()) {
      cerr << "setup_signal_handler failed" << endl;
      return;
   }

   while (true) {
      // Since our last run thru the main loop, did someone hit ^C?
      if (got_signal_to_die)
         break;

      //cout << "kerninstd polling..." << endl;

      bool any_timeouts = false;
      int poll_timeout = INT_MAX;
      // update poll_timeout's...
      
      if (!any_timeouts)
         poll_timeout = -1;

      struct pollfd poll_fds[100]; // too arbitrary a max limit?
      unsigned num_poll_fds = 0; // for now...

      // Add the accept socket to the poll list:
      poll_fds[num_poll_fds].fd = accept_fd;
      poll_fds[num_poll_fds].events = POLLRDNORM;
      num_poll_fds++;

      // Add igen connections to the poll list.  But first, handle the possibility that
      // in the last iteration of the main loop, one or more of the connections died, in
      // which case they'll be NULL in clientProcs[].  If so, compact clientProcs by
      // removing the scruffy obsolete NULL entry.
      pdvector<client_connection*>::iterator iter = std::find(clientProcs.begin(),
                                                            clientProcs.end(),
                                                            (client_connection*)NULL);
      while (iter != clientProcs.end()) {
         cout << "Found a NULL (fried) clientProc; removing from clientProcs[]" << endl;
         
         *iter = clientProcs.back();
         clientProcs.pop_back();

         iter = std::find(iter, clientProcs.end(), (client_connection*)NULL);
      }
      assert(clientProcs.end() == std::find(clientProcs.begin(), clientProcs.end(),
                                            (client_connection*)NULL));
      
      const mrtime_t currtime = getmrtime();
      for (pdvector<client_connection*>::const_iterator iter = clientProcs.begin();
           iter != clientProcs.end();
           ++iter) {
         client_connection *client = *iter;
         client->update_poll_info(poll_fds[num_poll_fds++], currtime, poll_timeout);
      }

      // Update poll_timeout, which is in milliseconds, for internal one-time events:
      if (!oneTimeInternalEvents.empty()) {
         const mrtime_t nextInternalEventTime = oneTimeInternalEvents.peek_first_key();
         if (currtime >= nextInternalEventTime) {
            // time to work on this right now!
            //cout << "right now" << endl;
            poll_timeout = 0;
         }
         else {
            const int msecsInTheFuture =
               (int)nsec2msec_roundup(nextInternalEventTime - currtime);
            //cout << msecsInTheFuture << " in the future" << endl;

            if (poll_timeout == -1 || msecsInTheFuture < poll_timeout)
               poll_timeout = msecsInTheFuture;
         }
      }

      int poll_result = poll(poll_fds, num_poll_fds, poll_timeout);
      if (poll_result == -1) {
         switch (errno) {
            case EAGAIN:
               // The man page suggests retrying in this case.  OK.
               continue;
            case EINTR:
               // interrupted poll (perhaps ^c); ignore.
               continue;
            default:
               perror("poll");
               break;
         }
      }
      else {
         // result: # of fds for which revents field is nonzero; could be 0 (timeout)
         
         //cout << "poll result: " << poll_result << endl;
         unsigned num_handled = 0;

         // Did anything happen on the accept socket?
         try {
            if (process_possible_accept(poll_fds[0]))
               num_handled++;
         }
         catch (accept_socket_died) {
            cerr << "accept socket seems to have died" << endl;
            break;
         }

         // Check for something coming in over one of the client sockets.
         // (The following starts at 1 to exclude the accept socket)
         for (unsigned lcv=1; lcv < num_poll_fds; lcv++) {
            if (poll_fds[lcv].revents != 0) {
               try {
                  clientProcs[lcv-1]->process_poll_response(poll_fds[lcv].revents);
               }
               catch (client_connection::client_connection_error) {
                  cout << "A client connection seems to have died" << endl;
                  cout << "Cleaning up structures for that client & continuing" << endl;
                  fry_client_by_ndx(lcv-1);

                  cout << "Reminder: the accept port number is "
                       << accept_portnum << endl;
               }
               catch (...) {
                  cout << "A client connection has died [unknown exception]" << endl;
                  cout << "Cleaning up structures for that client & continuing" << endl;
                  fry_client_by_ndx(lcv-1);

                  cout << "Reminder: the accept port number is "
                       << accept_portnum << endl;
               }
               num_handled++;
            }
         }

         if (num_handled != (unsigned)poll_result)
            cerr << "kerninstd warning: handled " << num_handled << " but poll_result returned " << poll_result << endl;

         // Check for doing periodic downloaded code among all of the clients
         for (pdvector<client_connection*>::const_iterator iter = clientProcs.begin();
              iter != clientProcs.end(); ++iter) {
            client_connection *client = *iter;
            client->do_periodic_if_appropriate();
            client->ship_any_buffered_samples();
         }

         // Check for doing delayed kerninstd internal actions, such as (after a timeout
         // for safety) freeing up a springboard or code patch
         while (!oneTimeInternalEvents.empty()) {
            const mrtime_t currTime = getmrtime();
            const mrtime_t nextInternalEventTime =
               oneTimeInternalEvents.peek_first_key();
            if (currTime >= nextInternalEventTime) {
               // Do this event now
               const internalEvent *theInternalEvent =
                  oneTimeInternalEvents.peek_first_data();

               assert(theInternalEvent->getWhenToDo() == nextInternalEventTime);
               assert(currTime >= theInternalEvent->getWhenToDo());

//              cout << "CurrTime=" << currTime << ": doing internal event marked for "
//                   << nextInternalEventTime << endl;
               
               theInternalEvent->doNow(theInternalEvent->getVerboseTiming());
            
               delete theInternalEvent;
            
               oneTimeInternalEvents.delete_first();
            }
            else
               break;
         }
      }
   }

   //   dout << "leaving kerninstd_mainloop" << endl;

   (void)close(accept_fd);
}

