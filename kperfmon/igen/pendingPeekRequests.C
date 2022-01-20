// pendingPeekRequests.C

#include "pendingPeekRequests.h"
#include "kerninstdClient.xdr.CLNT.h"

extern kerninstdClientUser *connection_to_kerninstd;

unsigned pendingPeekRequest::addToPendingRequestDict(pendingPeekRequest *req) {
   // a static method

   const unsigned id = uniqueRequestId++;
   pendingPeekRequests.set(id, req);

   return id;
}

static unsigned our_hash(const unsigned &id) {
   return id; // for now; we have improve this later
}

unsigned pendingPeekRequest::uniqueRequestId = 0;
dictionary_hash<unsigned, pendingPeekRequest*> pendingPeekRequest::pendingPeekRequests(our_hash, 13); // 13 bins by default

void pendingPeekRequest::make_request(pendingPeekRequest *req) {
   // add to dictionary
   const unsigned reqid = addToPendingRequestDict(req);
   
   // make igen peek request call   
   connection_to_kerninstd->request_for_peek(reqid, req->addresses);
}

void pendingPeekRequest::processResponse(unsigned reqid,
                                         const pdvector<chunkData> &data) {
   pendingPeekRequest *reqptr=pendingPeekRequests.get(reqid);
   pendingPeekRequest &req = *reqptr;

   req(data); // operator()

   delete reqptr;
   pendingPeekRequests.undef(reqid);
}
