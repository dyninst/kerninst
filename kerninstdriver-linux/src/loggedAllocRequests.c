// loggedAllocRequests.c

#include "loggedAllocRequests.h"
#include <linux/slab.h>

#ifdef ppc64_unknown_linux2_4
#include <linux/vmalloc.h>
#endif
#define MAX_ALLOC_REQS 100

extern uint32_t kerninst_debug_bits;

void kerninst_touch_bolted (void *data); 


extern int (*kerninst_on_each_cpu_ptr) (void (*func) (void *info), void *info, int nonatomic, int wait);
extern void* (*kerninst_malloc_ptr) (unsigned long size);
extern void (*kerninst_free_ptr)(void *ea);


int OAR_allocate(oneAllocReq *oar, unsigned nbytes,
                 unsigned reqdAlign, unsigned allocType)
{
   kptr_t region;
#ifdef ppc64_unknown_linux2_4
   if((allocType != ALLOC_TYPE_DATA) && (allocType != ALLOC_TYPE_KMEM32)
      && (allocType != ALLOC_TYPE_NUCLEUS) ){
#else
   if((allocType != ALLOC_TYPE_DATA) && (allocType != ALLOC_TYPE_KMEM32)) {
#endif
      printk(KERN_ERR "kerninst: loggedAllocRequests.c//OAR_allocate() - invalid allocation type\n");
      return 1;
   }
   else if(nbytes > 131000) { // 128KB == 131072 to be exact
      // kmalloc only supports allocations <= 128KB. eventually, we should
      // use get_free_pages for allocations > 128KB
      printk(KERN_ERR "kerninst: loggedAllocRequests.c//OAR_allocate() - nbytes>128KB, not supported\n");
      return 1;
   }
#ifdef ppc64_unknown_linux2_4
   if (allocType == ALLOC_TYPE_NUCLEUS) {
      region = (kptr_t)kerninst_malloc_ptr(nbytes);
      kerninst_touch_bolted((void *)region);
      kerninst_on_each_cpu_ptr(kerninst_touch_bolted, (void *)region, 0 , 1);
   }
   else
      region = (kptr_t)vmalloc(nbytes);
     
#else
   region = (kptr_t)kmalloc(nbytes, GFP_KERNEL);
#endif
   if(!region) {
      printk(KERN_ERR "kerninst: loggedAllocRequests.c//OAR_allocate() - kmalloc(nbytes=%d) failed\n", nbytes);
      return 1;
   }
   if(region % reqdAlign != 0) {
      printk(KERN_ERR "kerninst: loggedAllocRequests.c//OAR_allocate() - region not aligned properly\n");
      return 1;
   }
   oar->prettyAddr = oar->actuallyAllocatedAt = region;
   oar->prettyMappedAddr = oar->actuallyMappedAt = region;
   oar->prettyNumBytes = oar->actualNumBytesAllocated = nbytes;
   oar->allocType = allocType;
   return 0;
}

loggedAllocRequests* initialize_logged_allocRequests(void)
{
   loggedAllocRequests *tmp = (loggedAllocRequests*) 
      kmalloc(sizeof(loggedAllocRequests), GFP_KERNEL);
   if(tmp) {
      tmp->num_reqs = 0;
      tmp->reqs = (oneAllocReq*) kmalloc(MAX_ALLOC_REQS*sizeof(oneAllocReq), GFP_KERNEL);
      if(tmp->reqs == NULL)
         printk(KERN_ERR "kerninst: loggedAllocRequests.c//initialize_logged_allocRequests() - kmalloc(MAX_ALLOC_REQS*oneAllocReq) failed\n");
   }
   else
      printk(KERN_ERR "kerninst: loggedAllocRequests.c//initialize_logged_allocRequests() - kmalloc(loggedAllocRequests) failed\n");
   return tmp;
}

void destroy_logged_allocRequests(loggedAllocRequests *lar)
{
   if(lar) {
      if(lar->reqs) {
         LAR_undo(lar);
         kfree(lar->reqs);
      }
      kfree(lar);
   }
}

void LAR_undo(loggedAllocRequests *lar)
{
   int i;
   oneAllocReq *iter;
   if(lar) {
      if(lar->num_reqs) {
         if(kerninst_debug_bits & KERNINST_DEBUG_MEMORY)
            printk(KERN_INFO "kerninst: loggedAllocRequests.c//LAR_undo() - freeing %d alloc reqs\n", lar->num_reqs);
         for(i=0, iter=lar->reqs; i < lar->num_reqs; i++, iter++) {
            switch (iter->allocType) {
            case ALLOC_TYPE_DATA: 
            case ALLOC_TYPE_KMEM32:
#ifdef ppc64_unknown_linux2_4
               vfree((void*)iter->actuallyAllocatedAt); 
#else
               kfree((void*)iter->actuallyAllocatedAt); 
#endif
               break;
#ifdef ppc64_unknown_linux2_4
            case ALLOC_TYPE_NUCLEUS:
               kerninst_free_ptr((void*)iter->actuallyAllocatedAt);
               break;
#endif
            default:
               printk(KERN_ERR "kerninst: loggedAllocRequests.c//LAR_undo() - invalid allocation type %d\n", (int)iter->allocType);
               break;
            }
         }
      }
   }
}

int LAR_alloc(loggedAllocRequests *lar, 
              unsigned nbytes, 
              unsigned requiredAlignment, 
              unsigned allocType, 
              kptr_t *result, 
              kptr_t *result_mappable)
{
   oneAllocReq *new_req;
   if(lar) {
      if(lar->num_reqs == MAX_ALLOC_REQS) {
         printk(KERN_ERR "kerninst: loggedAllocRequests.c//LAR_alloc() - already have MAX_ALLOC_REQS\n");
         return 1;
      }
      new_req = lar->reqs + lar->num_reqs;
      if(!OAR_allocate(new_req, nbytes, requiredAlignment, allocType)) {
         lar->num_reqs++;
         *result = new_req->prettyAddr;
         *result_mappable = new_req->prettyMappedAddr;
      }
      else {
         *result = 0;
         *result_mappable = 0;
      }
   }
   else {
      printk(KERN_ERR "kerninst: loggedAllocRequests.c//LAR_alloc() - NULL loggedAllocRequests*\n");
      return 1;
   }
   return 0;
}

int LAR_free(loggedAllocRequests *lar, 
             kptr_t kernelAddr, 
             kptr_t mappedKernelAddr, 
             unsigned nbytes)
{
   int i;
   oneAllocReq *iter, *end;
   if(lar) {
      if(lar->num_reqs) {
         for(i=0, iter=lar->reqs; i < lar->num_reqs; i++, iter++) {
            if(iter->prettyAddr == kernelAddr) {
               if(iter->prettyMappedAddr != mappedKernelAddr) {
                  printk(KERN_ERR "kerninst: loggedAllocRequests.c//LAR_free() - addr mismatch\n");
                  return 1;
               }
#ifdef ppc64_unknown_linux2_4
               if (iter->allocType == ALLOC_TYPE_NUCLEUS) {
                  kerninst_free_ptr((void*)iter->actuallyAllocatedAt);
               } else {
                  vfree((void*)iter->actuallyAllocatedAt);
               }
#else
               kfree((void*)iter->actuallyAllocatedAt);
#endif
               // replace this oneAllocReq with the last one to free space
               end = lar->reqs + (lar->num_reqs - 1);
               if(end != iter) {
                  iter->prettyAddr = end->prettyAddr;
                  iter->prettyMappedAddr = end->prettyMappedAddr;
                  iter->prettyNumBytes = end->prettyNumBytes;
                  iter->allocType = end->allocType;
                  iter->actuallyAllocatedAt = end->actuallyAllocatedAt;
                  iter->actuallyMappedAt = end->actuallyMappedAt;
                  iter->actualNumBytesAllocated = end->actualNumBytesAllocated;
               }
               lar->num_reqs--;
               break;
            }
         }
      }
      else {
         printk(KERN_ERR "kerninst: loggedAllocRequests.c//LAR_free() - lar->num_reqs==0\n");
         return 1;
      }
   }
   else {
      printk(KERN_ERR "kerninst: loggedAllocRequests.c//LAR_free() - NULL loggedAllocRequests*\n");
      return 1;
   }
   return 0;
}
