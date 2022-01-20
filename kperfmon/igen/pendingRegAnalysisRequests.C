// pendingRegAnalysisRequests.C

#include "pendingRegAnalysisRequests.h"
#include "kerninstdClient.xdr.CLNT.h"

extern kerninstdClientUser *connection_to_kerninstd;

pendingRegAnalysisRequest::
pendingRegAnalysisRequest(const pdvector<oneRegAnalysisReqInfo> &iwhere) : where(iwhere) {
}

unsigned pendingRegAnalysisRequest::
addToPendingRequestDict(pendingRegAnalysisRequest *req) {
   // a static method

   const unsigned id = uniqReqId++;
   pendingRegAnalRequests.set(id, req);

   return id;
}

const pdvector<oneRegAnalysisReqInfo> &
pendingRegAnalysisRequest::getWheres() const {
   return where;
}

static unsigned our_hash(const unsigned &id) {
   return id; // for now; we have improve this later
}

unsigned pendingRegAnalysisRequest::uniqReqId = 0;
dictionary_hash<unsigned, pendingRegAnalysisRequest*> pendingRegAnalysisRequest::pendingRegAnalRequests(our_hash, 13); // 13 bins by default

void pendingRegAnalysisRequest::make_request(pendingRegAnalysisRequest *req) {
   // add to dictionary
   const unsigned reqid = addToPendingRequestDict(req);
   
   // make igen reg analysis request call   
   connection_to_kerninstd->requestRegAnalysis(reqid, req->getWheres());
}

void pendingRegAnalysisRequest::
processResponse(unsigned reqid,
                const pdvector<dataflowFn1Insn> &data) {
   pendingRegAnalysisRequest *reqptr = pendingRegAnalRequests.get(reqid);
   pendingRegAnalysisRequest &req = *reqptr;

   req(data); // operator()

   delete reqptr;
   pendingRegAnalRequests.undef(reqid);
}
