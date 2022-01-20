// loggedWrites.c

#include <linux/slab.h> // memcmp

#include "loggedWrites.h"
#include "physMem.h"

#define MAX_WRITES 1024 

void undo(oneWrite *w, char *oldval_to_set_to)
{
   if(memcmp(w->origval, oldval_to_set_to, w->length) != 0)
      printk(KERN_ERR "kerninst: loggedWrites.c//undo() - old value mismatch\n");
   undo_blind(w);
}

void undo_blind(oneWrite *w) 
{
   int ret = write1Insn(w->addr, w->origval, w->length);
   if(ret)
      printk(KERN_ERR "kerninst: loggedWrites.c//undo_blind() - call to write1Insn() failed\n");
}

loggedWrites* initialize_logged_writes(void)
{
   loggedWrites *tmp = (loggedWrites*) kmalloc(sizeof(loggedWrites), 
                                               GFP_KERNEL);
   if(tmp) {
      tmp->num_writes = 0;
      tmp->writes = (oneWrite*) kmalloc(MAX_WRITES*sizeof(oneWrite), 
                                        GFP_KERNEL);
      if(tmp->writes == NULL)
         printk(KERN_ERR "kerninst: loggedWrites.c//initialize_logged_writes() - kmalloc(MAX_WRITES*oneWrite) failed\n");
   }
   else
      printk(KERN_ERR "kerninst: loggedWrites.c//initialize_logged_writes() - kmalloc(loggedWrites) failed\n");
   return tmp;
}

void destroy_logged_writes(loggedWrites *lw)
{
   int i;
   oneWrite *w;
   if(lw) {
      if(lw->writes) {
         // do howsoon==0 writes first
         for(i=0; i<lw->num_writes; i++) {
            w = lw->writes + i;
            if(w->howsoon == 0)
               undo_blind(w);
         }
         // then everybody else
         for(i=0; i<lw->num_writes; i++) {
            w = lw->writes + i;
            if(w->howsoon)
               undo_blind(w);
         }
         kfree(lw->writes);
      }
      kfree(lw);
   }
}

int LW_performUndoableWrite(loggedWrites *theLoggedWrites, kptr_t addr, 
                            unsigned length, char *val, 
                            char *oldval_fillin, unsigned howSoon)
{
   int ret, i;
   int exists = 0;
   oneWrite *w;
   for(i=0; i < theLoggedWrites->num_writes; i++) {
      if((theLoggedWrites->writes + i)->addr == addr) {
         exists = 1;
         break;
      }
   }
   if(!exists) {
      if(theLoggedWrites->num_writes == MAX_WRITES) {
         printk(KERN_ERR "kerninst: loggedWrites.c//LW_performUndoableWrite() - already have MAX_WRITES\n");
         return 1;
      }
      else {
         w = theLoggedWrites->writes + theLoggedWrites->num_writes;
         w->addr = addr;
         w->length = length;
         w->howsoon = howSoon;
         w->origval = (char*) kmalloc(length, GFP_KERNEL);
         memcpy(w->origval, (char*)addr, length);
         memcpy(oldval_fillin, w->origval, length);
         theLoggedWrites->num_writes++;
      }
   }
   ret = write1Insn(addr, val, length);
   return ret;
}

int LW_undoWrite1Insn(loggedWrites *theLoggedWrites, kptr_t addr, 
                      unsigned length, char *oldval_to_set_to)
{
   int i;
   oneWrite *w = NULL;
   oneWrite *last = NULL;
   for(i=0; i < theLoggedWrites->num_writes; i++) {
      if((theLoggedWrites->writes + i)->addr == addr) {
         undo(theLoggedWrites->writes + i, oldval_to_set_to);
         w = theLoggedWrites->writes + i;
         last = theLoggedWrites->writes + (theLoggedWrites->num_writes-1);
         kfree(w->origval);
         if(w != last) {
            w->origval = last->origval;
            w->length = last->length;
            w->addr = last->addr;
            w->howsoon = last->howsoon;
         }
         theLoggedWrites->num_writes--;
         return 0;
      }
   }
   printk(KERN_ERR "kerninst: loggedWrites.c//LW_undoWrite1Insn() - could not find write for addr=%08lx\n", (u_long)addr);
   return 1;
}

int LW_changeUndoableWriteHowSoon(loggedWrites *theLoggedWrites, kptr_t addr, 
                                  unsigned howSoon)
{
   int i;
   oneWrite *w;

   for(i=0; i < theLoggedWrites->num_writes; i++) {
      if((theLoggedWrites->writes + i)->addr == addr) {
         w = theLoggedWrites->writes + i;
         w->howsoon = howSoon;
         return 0;
      }
   }
   printk(KERN_WARNING "kerninst: loggedWrites.c//LW_changeUndoableWriteHowSoon() - could not find write for addr=%08lx\n", (u_long)addr);
   return 1;
}
