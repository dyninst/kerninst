// pendingPeekRequest.h

#ifndef _PENDING_PEEK_REQUEST_H_
#define _PENDING_PEEK_REQUEST_H_

#include "util/h/kdrvtypes.h"
#include "util/h/Dictionary.h"
#include <sys/types.h> // uint32_t
#include "kerninstdClient.xdr.h" // chunkRange

class pendingPeekRequest {
 private:
   pdvector<chunkRange> addresses;
      // chunkRange is defined in .I file
      // a vector since we can peek a non-contiguous set of "chunks"

   // private so they're not used:
   pendingPeekRequest(const pendingPeekRequest &src);
   pendingPeekRequest& operator=(const pendingPeekRequest &src);
   
   static dictionary_hash<unsigned, pendingPeekRequest*> pendingPeekRequests;
   static unsigned uniqueRequestId;
   
   static unsigned addToPendingRequestDict(pendingPeekRequest *);
      // returns reqid

 public:
   pendingPeekRequest(const pdvector< pair<kptr_t, unsigned> > &iaddrs) :
      addresses(iaddrs) {
   }
   virtual ~pendingPeekRequest() {}

   virtual void operator()(const pdvector<chunkData> &data) = 0;
      // a vector since we can peek several chunks
      // chunkData is defined in .I file

   static void make_request(pendingPeekRequest *);
      // installs in dictionary, makes igen call

   static void processResponse(unsigned reqid,
                               const pdvector<chunkData> &data);
      // chunkData is defined in .I file
};


#endif
