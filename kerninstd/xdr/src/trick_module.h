// trick_module.h

// An igen kludge to avoid a copy-constructor call in kperfmon when it 
// receives a loadedModule.
//
// If you want more specifics, here goes. If we were to pass a "loadedModule&" 
// directly as a param in an igen routine, then we'll be unable to avoid a 
// copy constructor for class loadedModule, though it's not obvious why at 
// first.  Using P_xdr_recv(), we could indeed receive over an xdr connection 
// and initialize the "loadedModule&" in place.  So far no copy-ctor. But 
// under igen-generated code, the reference can be to *stack* memory, so 
// pointers to it won't stay valid for very long.  So inevitably, after 
// receiving the loadedModule, adding it to the moduleMgr will require a copy 
// ctor for safety, to stop using the stack memory version of the loadedModule.
// trick_module() gets around this by allocating the loadedModule in heap 
// memory with regular "new", and initializing it in place.  Since the heap 
// memory stays valid, we can pass around pointers to it at will, which will 
// stay valid forever. And this makes adding a module to the moduleMgr a 
// trivial operation (just adds the pointer to its collection of modules).

#ifndef _TRICK_MODULE_H_
#define _TRICK_MODULE_H_

class loadedModule;

class trick_module {
 private:
   const loadedModule *mod;
   
 public:
   trick_module(const loadedModule *imod);
  ~trick_module();

   const loadedModule* get() const;
};

#endif
