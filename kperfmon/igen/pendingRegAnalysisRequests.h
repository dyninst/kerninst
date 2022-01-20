// pendingRegAnalysisRequests.h

#ifndef _PENDING_REG_ANALYSIS_REQUESTS_H_
#define _PENDING_REG_ANALYSIS_REQUESTS_H_

#include "util/h/Dictionary.h"
#include "kerninstdClient.xdr.CLNT.h"

class pendingRegAnalysisRequest {
 private:
   pdvector<oneRegAnalysisReqInfo> where; // igen class

   // private to ensure not used:
   pendingRegAnalysisRequest(const pendingRegAnalysisRequest &);
   pendingRegAnalysisRequest &operator=(const pendingRegAnalysisRequest &);

   static dictionary_hash<unsigned, pendingRegAnalysisRequest*> pendingRegAnalRequests;
   static unsigned uniqReqId;
   
   static unsigned addToPendingRequestDict(pendingRegAnalysisRequest *);
      // returns reqid

 public:
   pendingRegAnalysisRequest(const pdvector<oneRegAnalysisReqInfo> &iwhere);
   virtual ~pendingRegAnalysisRequest() {}

   const pdvector<oneRegAnalysisReqInfo> &getWheres() const;
   
   virtual void operator()(const pdvector<dataflowFn1Insn> &) = 0;
   
   static void make_request(pendingRegAnalysisRequest *);
   static void processResponse(unsigned reqid, const pdvector<dataflowFn1Insn> &data);
};

#endif
