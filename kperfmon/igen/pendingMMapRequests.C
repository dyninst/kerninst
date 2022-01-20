// pendingMMapRequests.C

#include "pendingMMapRequests.h"
#include "kerninstdClient.xdr.CLNT.h"

extern kerninstdClientUser *connection_to_kerninstd;

pendingMMapRequest::pendingMMapRequest(const pdstring &isymName, unsigned inbytes) :
      symName(isymName), nbytes(inbytes) {
}

unsigned pendingMMapRequest::addToPendingRequestDict(pendingMMapRequest *req) {
   // a static method

   const unsigned id = uniqueRequestId++;
   pendingMMapRequests.set(id, req);

   return id;
}

static unsigned our_hash(const unsigned &id) {
   return id; // for now; we have to improve this later
}

unsigned pendingMMapRequest::uniqueRequestId = 0;
dictionary_hash<unsigned, pendingMMapRequest*> pendingMMapRequest::pendingMMapRequests(our_hash, 13); // 13 bins by default

void pendingMMapRequest::make_request(pendingMMapRequest *req) {
   // add to dictionary
   const unsigned reqid = addToPendingRequestDict(req);
   
   // make igen ct alloc request call   
   connection_to_kerninstd->
      requestReadOnlyMMapOfKernelSymIntoKerninstd(reqid, req->getSymName(),
                                                  req->getNumBytes());
}

void pendingMMapRequest::
processResponse(unsigned reqid, unsigned long addrInKerninstd) {
   pendingMMapRequest *reqptr = pendingMMapRequests.get(reqid);
   pendingMMapRequest &req = *reqptr;

   req(reqid, addrInKerninstd); // operator()

   delete reqptr;
   reqptr = NULL; // help purify find mem leaks
   pendingMMapRequests.undef(reqid);
}
