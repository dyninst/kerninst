// pendingMMapRequests.h

#ifndef _PENDING_MMAP_REQUEST_H_
#define _PENDING_MMAP_REQUEST_H_

#include "util/h/Dictionary.h"

#include "common/h/String.h"

class pendingMMapRequest {
 private:
   pdstring symName; // symbol to map
   unsigned nbytes; // its length

   // private so they're not used:
   pendingMMapRequest(const pendingMMapRequest &src);
   pendingMMapRequest& operator=(const pendingMMapRequest &src);
   
   static dictionary_hash<unsigned, pendingMMapRequest*> pendingMMapRequests;
   static unsigned uniqueRequestId;
   
   static unsigned addToPendingRequestDict(pendingMMapRequest *);
      // returns reqid

 public:
   pendingMMapRequest(const pdstring &isymName, unsigned nbytes);
   virtual ~pendingMMapRequest() {}

   const pdstring &getSymName() const { return symName; }
   unsigned getNumBytes() const { return nbytes; }

   virtual void operator()(unsigned reqid, // chosen by us
                           unsigned long addrInKerninstd) = 0;

   static void make_request(pendingMMapRequest *);
      // installs in dictionary, makes igen call
   static void processResponse(unsigned reqid,
                               unsigned long addrInKerninstd);
};


#endif
