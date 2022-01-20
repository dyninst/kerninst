/*
 * Copyright (c) 1996 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 * 
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 * 
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */

/************************************************************************
 * $Id: Object.h,v 1.5 2004/04/22 21:23:38 mjbrim Exp $
 * Object.h: interface to objects, symbols, lines and instructions.
************************************************************************/


#if !defined(_Object_h_)
#define _Object_h_

/************************************************************************
 * header files.
************************************************************************/

#include "util/h/Dictionary.h"
#include "common/h/String.h"
// trace data streams
#include "common/h/ByteArray.h"
#include "common/h/Symbol.h"
#include "common/h/Types.h"
#include "common/h/Vector.h"
#include "util/h/lprintf.h"

extern int symbol_compare(const void *x, const void *y);

// relocation information for calls to functions not in this image
// on sparc-solaris: target_addr_ = rel_addr_ = PLT entry addr
// on x86-solaris: target_addr_ = PLT entry addr
//		   rel_addr_ =  GOT entry addr  corr. to PLT_entry
class relocationEntry {
public:
    relocationEntry():target_addr_(0),rel_addr_(0),name_(0){}   
    relocationEntry(Address ta,Address ra, pdstring n):target_addr_(ta),
	rel_addr_(ra),name_(n){}   
    relocationEntry(const relocationEntry& ra): target_addr_(ra.target_addr_), 
    	rel_addr_(ra.rel_addr_), name_(ra.name_) { }
    ~relocationEntry(){}

    const relocationEntry& operator= (const relocationEntry &ra) {
	target_addr_ = ra.target_addr_; rel_addr_ = ra.rel_addr_; 
	name_ = ra.name_; 
	return *this;
    }
    Address target_addr() const { return target_addr_; }
    Address rel_addr() const { return rel_addr_; }
    const pdstring &name() const { return name_; }

    // dump output.  Currently setup as a debugging aid, not really
    //  for object persistance....
    ostream & operator<<(ostream &s) const;
    friend ostream &operator<<(ostream &os, relocationEntry &q);

private:
   Address target_addr_;	// target address of call instruction 
   Address rel_addr_;		// address of corresponding relocation entry 
   pdstring  name_;
};

/************************************************************************
 * class AObject
 *
 *  WHAT IS THIS CLASS????  COMMENTS????
 *  Looks like it has a dictionary hash of symbols, as well as
 *   a ptr to to the code section, an offset into the code section,
 *   and a length of the code section, and ditto for the data
 *   section....
************************************************************************/

class AObject {
public:

    unsigned  nsymbols () const { return symbols_.size(); }

    bool      get_symbol (const pdstring &name, Symbol &symbol) {
    	if (!symbols_.defines(name)) {
       	    return false;
    	}
    	symbol = symbols_[name];
    	return true;
    }

    const Word*       code_ptr () const { return code_ptr_; } 
    Address           code_off () const { return code_off_; }
    Address           code_len () const { return code_len_; }

    const Word*       data_ptr () const { return data_ptr_; }
    Address           data_off () const { return data_off_; }
    Address           data_len () const { return data_len_; }

    virtual  bool   needs_function_binding()  const;
    virtual  bool   get_func_binding_table(pdvector<relocationEntry> &) const;
    virtual  bool   get_func_binding_table_ptr(const pdvector<relocationEntry> *&) const;
    
    // for debuggering....
    const ostream &dump_state_info(ostream &s);

protected:
    virtual ~AObject() {}
    // explicitly protected
    AObject(const pdstring file , void (*err_func)(const char *)): file_(file), 
	symbols_(pdstring::hash), code_ptr_(0), code_off_(0), code_len_(0), 
	data_ptr_(0), data_off_(0), data_len_(0),err_func_(err_func){} 

    AObject(const AObject &obj): file_(obj.file_), symbols_(obj.symbols_), 
        code_ptr_(obj.code_ptr_), code_off_(obj.code_off_), 
	code_len_(obj.code_len_), data_ptr_(obj.data_ptr_), 
	data_off_(obj.data_off_), data_len_(obj.data_len_), 
	err_func_(obj.err_func_) {}   

    AObject&  operator= (const AObject &obj) {   

	 if (this == &obj) { return *this; }
	 file_      = obj.file_; 	symbols_   = obj.symbols_;
	 code_ptr_  = obj.code_ptr_; 	code_off_  = obj.code_off_;
	 code_len_  = obj.code_len_; 	data_ptr_  = obj.data_ptr_;
	 data_off_  = obj.data_off_; 	data_len_  = obj.data_len_;
	 err_func_  = obj.err_func_;
	 return *this;
    }

    pdstring                          file_;
    dictionary_hash<pdstring, Symbol> symbols_;

    Word*    code_ptr_;
    Address code_off_;
    Address code_len_;

    Word*    data_ptr_;
    Address  data_off_;
    Address data_len_;

    void (*err_func_)(const char*);

private:
    friend class SymbolIter;
};

/************************************************************************
 * include the architecture-operating system specific object files.
************************************************************************/

#undef HAVE_SPECIFIC_OBJECT

#if defined(sparc_sun_solaris2_4) || defined(i386_unknown_solaris2_5)
#include "util/h/Object-elf32.h"
#define HAVE_SPECIFIC_OBJECT
#endif /* defined(sparc_sun_solaris2_4) */

#if defined(i386_unknown_linux2_0)
#include "util/h/Object-elf32.h"
#define HAVE_SPECIFIC_OBJECT
#endif /* defined(i386_unknown_linux2_0) */

#if defined(mips_sgi_irix6_4)
#include "util/h/Object-elf32.h"
#define HAVE_SPECIFIC_OBJECT
#endif /* defined(mips_sgi_irix6_4) */

#if defined(rs6000_ibm_aix4_1)
#include "util/h/Object-aix.h"
#define HAVE_SPECIFIC_OBJECT
#endif /* defined(rs6000_ibm_aix4_1) */

#if defined(i386_unknown_nt4_0)
#include "util/h/Object-nt.h"
#define HAVE_SPECIFIC_OBJECT
#endif /* defined(i386_unknown_nt4_0) */

#if defined(alpha_dec_osf4_0)
#include "util/h/Object-coff.h"
#define HAVE_SPECIFIC_OBJECT
#endif /* defined(alpha_dec_osf4_0) */

#if !defined(HAVE_SPECIFIC_OBJECT)
#error "unable to locate system-specific object files"
#endif /* !defined(HAVE_SPECIFIC_OBJECT) */

/************************************************************************
 * class SymbolIter
************************************************************************/

class SymbolIter {
public:
     SymbolIter (const Object &obj):  si_(obj.symbols_.begin()) {}

    ~SymbolIter () {}

   void operator++(int) { si_++; }
   void operator++() { si_++; }
   
   const pdstring &currkey() const {
      return si_.currkey();
   }
   const Symbol &currval() const {
      return si_.currval();
   }

private:
    dictionary_hash<pdstring, Symbol>::const_iterator si_;

   SymbolIter (const SymbolIter &src): si_(src.si_) {} // explicitly disallowed
    SymbolIter& operator= (const SymbolIter &); // explicitly disallowed
};

#endif /* !defined(_Object_h_) */
