// kernInstIoctls.c

#include "kernInstIoctls.h"
#include "loggedAllocRequests.h"
#include <linux/kernel.h>

int kerninst_alloc_block(void *loggedAllocReqs,
                         struct kernInstAllocStruct *req)
{
   loggedAllocRequests *lar = (loggedAllocRequests*) loggedAllocReqs;
   if(!lar) {
      printk(KERN_ERR "kerninst: kerninst_alloc_block called with NULL lar\n");
      return 1;
   }
   return LAR_alloc(lar, req->nbytes, req->requiredAlignment,
                    req->allocType, &req->result, &req->result_mappable);
}

int kerninst_free_block(void *loggedAllocReqs,
                        struct kernInstFreeStruct *req)
{
   loggedAllocRequests *lar = (loggedAllocRequests*) loggedAllocReqs;
   if(!lar) {
      printk(KERN_ERR "kerninst: kerninst_free_block called with NULL lar\n");
      return 1;
   }
   return LAR_free(lar, req->kernelAddr, req->mappedKernelAddr, req->nbytes);
}
