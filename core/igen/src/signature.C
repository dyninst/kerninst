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

// signature.C

#include "signature.h"
#include "type_defn.h"
#include "Options.h"
#include "common/h/Dictionary.h"

signature::signature(pdvector<arg*> *alist, const pdstring rf_name) : is_const_(false), stars_(0) {
  assert(alist);
  for (unsigned i=0; i<alist->size(); i++) 
    args += (*alist)[i];

  switch (args.size()) {
  case 0: type_ = "void"; break;
  case 1:
    type_ = args[0]->base_type();
    stars_ = args[0]->stars();
    is_const_ = args[0]->is_const();
    break;
  default:
    const type_defn *td = NULL;
    bool match = false;
    for (dictionary_hash_iter<pdstring, type_defn*> tdi=Options::all_types.begin();
         tdi != Options::all_types.end() && !match; tdi++) {
       td = tdi.currval();
       match = td->is_same_type(alist);
    }
    
    if (match) {
      type_ = td->name();
      for (unsigned q=0; q<args.size(); ++q)
	delete args[q];
      args.resize(0);
      args = td->copy_args();
      // get names here
    } else {
      type_ = Options::allocate_type(pdstring("T_") + rf_name, false, false,
				     false,
                     false,
                     "",
				     type_defn::TYPE_COMPLEX,
				     true, false, &args);
    }
    break;
  }
}

pdstring signature::type(const bool use_c) const {
  pdstring ret;
  if (use_c && is_const_)
    ret = "const ";
  ret += type_;
  ret += Options::make_ptrs(stars_);
  return ret;
}

pdstring signature::gen_bundler_call(bool send_routine,
                                   const pdstring &obj_nm, const pdstring &data_nm) const {
  return ((Options::all_types[base_type()])->gen_bundler_call(send_routine,
                                                              obj_nm,
							      data_nm, stars_));
}

bool signature::tag_bundle_send(ofstream &out_stream, const pdstring &return_value,
				const pdstring &req_tag) const
{
  if (Options::ml->address_space() == message_layer::AS_many)
    return (tag_bundle_send_many(out_stream, return_value, req_tag));
  else
    return (tag_bundle_send_one(out_stream, return_value, req_tag));
}

bool signature::tag_bundle_send_many(ofstream &out_stream, const pdstring &return_value,
				     const pdstring &) const
{
   // set direction encode
   out_stream << "   " << Options::set_dir_encode() << ";\n";

   // bundle individual args
   out_stream << "   if (!" << Options::ml->send_tag("net_obj()", "tag");
   out_stream << std::flush;

   for (unsigned i=0; i<args.size(); i++) {
      out_stream << " ||" << endl;
      out_stream << "       !";

      args[i]->gen_bundler(true, // sending
                           out_stream, "net_obj()", ""); // "" was formerly "&"
      out_stream << std::flush;
   }
  
   out_stream << ") ";
   out_stream << std::flush;
   out_stream << Options::error_state(true, 6, "igen_encode_err", return_value);
   out_stream << std::flush;

   // send message
   out_stream << "   if (!" << Options::ml->send_message() << ") ";
   out_stream << Options::error_state(true, 6, "igen_send_err", return_value);

   out_stream << std::flush;
   
   return true;
}

bool signature::tag_bundle_send_one(ofstream &out_stream, const pdstring return_value,
				    const pdstring req_tag) const
{
    if( args.size() > 0 )
    {
        out_stream << type(true) << "* send_buffer = new " << type(true) << ";\n";
        if( args.size() == 1 )
        {
            out_stream << "*send_buffer = " << args[0]->name() << ";\n";
        }
        else
        {
            (Options::all_types[type()])->assign_to("send_buffer->", 
                                                        args, 
                                                        out_stream);
        }
    }
    else
    {
        out_stream << "char* send_buffer = new char;\n";
    }

  pdstring sb = "send_buffer);\n";

  out_stream << Options::ml->bundler_return_type() << " res = "
    << Options::ml->send_message() << "(net_obj(), " << req_tag
      << ", " << sb;

  out_stream << "if (res == " << Options::ml->bundle_fail() << ") ";
  out_stream << Options::error_state(true, 6, "igen_send_err", return_value);

  return true;
}

pdstring signature::dump_args(const pdstring data_name, const pdstring sep) const {
  pdstring ret;
  switch (args.size()) {
  case 0:
    // out_stream << "void";
    return "";
  case 1:
    return data_name;
  default:
    for (unsigned i=0; i<args.size(); i++) {
      ret += data_name + sep + args[i]->name();
      if (i < (args.size() - 1))
	ret += ", ";
    }
    return ret;
  }
}

bool signature::gen_sig(ofstream &out_stream) const {
   switch (args.size()) {
      case 0:
         //out_stream << "void";
         out_stream << "";
         break;
      case 1:
      default:
         for (unsigned i=0; i<args.size(); i++) {
            out_stream << args[i]->type(true, true) << " " << args[i]->name();
            if (i < (args.size()-1))
               out_stream << ", ";
         }
         break;
   }
   return true;
}

bool signature::arg_struct(ofstream &out_stream) const {
  for (unsigned i=0; i<args.size(); i++)
    out_stream << args[i]->type() << " " << args[i]->name() << ";\n";
  return true;
}

