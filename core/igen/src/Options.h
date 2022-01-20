/*
 * Copyright (c) 1996-2004 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
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

// Options.h
// Moved here by Ariel Tamches, from parse.h, into its own .[hC] file combo
// for clarity.

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "common/h/String.h"
#include "type_defn.h"
#include "interface_spec.h"
#include "message_layer.h"

using std::ofstream;

class Options {
public:
  typedef enum { Mem_ignore, Mem_detect, Mem_handle } mem_type;

  static pdstring make_ptrs(unsigned count);
  static pdstring qual_to_unqual(const pdstring type);
  static pdstring file_base() { return file_base_;}
  static void set_file_base(const pdstring f) { file_base_ = f;}
  static pdstring input_file() { return input_file_;}
  static void set_input_file(const pdstring f) { input_file_ = f;}

  static bool profile() { return profile_;}
  static void set_profile(const bool b) { profile_ = b;}
  static bool shortnames() { return shortnames_;}
  static void set_shortnames(const bool b) { shortnames_ = b;}
  static mem_type mem() { return mem_;}
  static void set_mem(const mem_type m) { mem_ = m;}

  static pdstring error_state(bool braces,
                            unsigned nspaces,
                            const pdstring &err_name, const pdstring &return_value);
  static interface_spec *current_interface;
  static dictionary_hash<pdstring, type_defn*> all_types;
  static pdvector<type_defn*> vec_types;
  static pdvector<message_layer*> all_ml;
  static message_layer *ml;

  static void ignore(pdstring &s, bool is_server);
  static pdvector<pdstring> client_ignores;
  static pdvector<pdstring> server_ignores;
  static pdvector<pdstring> forward_decls;

  typedef struct el_data {
    pdstring type;
    unsigned stars;
    pdstring name;
  } el_data;

  typedef struct stl_data {
    pdstring include_file;
    pdstring name;
    bool need_include;
    pdstring pragma_name;
    pdvector<el_data> elements;
  } stl_data;
  static pdvector<stl_data> stl_types;

  static std::ifstream input;
  static ofstream dot_h;
  static ofstream dot_c;
  static ofstream clnt_dot_h;
  static ofstream clnt_dot_c;
  static ofstream srvr_dot_h;
  static ofstream srvr_dot_c;
  static ofstream temp_dot_c;

  static void dump_types();

  static pdstring allocate_stl_type(pdstring stl_type, pdstring element_name,
				  const unsigned star_count, const bool in_lib);
  static pdstring allocate_type(const pdstring &name, bool is_class,
                              bool is_abstract,
			                  bool is_derived,
                              bool is_virtual,
                  const pdstring &parent,
			      const type_defn::type_type &typ,
			      bool can_point, bool in_lib,
			      pdvector<arg*> *arglist=NULL, const pdstring &ignore_text="",
			      const pdstring &bundle_name="");
   static pdstring allocate_type(const pdstring &name, const bool isClass,
                               const type_defn::type_type &typ,
                               bool can_point, bool in_lib);
                               

  static pdstring add_type(const pdstring name,
                        const bool is_class,
                        const bool is_abstract,
			            const bool is_derived,
                        const bool is_virtual,
                        const pdstring parent,
			 const type_defn::type_type &type, 
			 const bool can_point, const bool in_lib, 
			 pdvector<arg*> *arglist=NULL, const pdstring ignore="",
			 const pdstring bundler_name="");
   static pdstring add_type(const pdstring &name, bool is_class,
                          const type_defn::type_type &type, 
                          const bool can_point, const bool in_lib);


  static pdstring obj() { return (pdstring("obj"));}
  static pdstring obj_ptr() { return (pdstring("obj->") + Options::ml->dir_field());}
  static pdstring gen_name();
  static pdstring set_dir_encode();
  static pdstring set_dir_decode();
  static pdstring type_class() {return (pdstring("T_") + file_base_);}
  static pdstring type_prefix() { return (type_class() + "::");}
  static bool types_defined(const pdstring name) {
    return (all_types.defines(name) || all_types.defines(type_prefix()+name));
  }
  static pdstring get_type(const pdstring name) {
    if (all_types.defines(name))
      return name;
    else if (all_types.defines(type_prefix()+name))
      return (type_prefix()+name);
    else {
      abort();
      return((char *)NULL); // some compilers complain if we don't return a value
    }
  }
  static bool stl_seen;
  static bool dont_gen_handle_err;

private:
  static pdstring file_base_;
  static pdstring input_file_;
  static bool profile_;
  static bool shortnames_;
  static mem_type mem_;
  static unsigned var_count_;
};

#endif


