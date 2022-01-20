// tkTools.h
// Ariel Tamches

// Some C++ stuff that I have found to be both useful and generic
// across all the tk4.0 programs I've written...

/* $Id: tkTools.h,v 1.8 2004/10/25 23:09:18 igor Exp $ */

#ifndef _TK_TOOLS_H_
#define _TK_TOOLS_H_

#include "tcl.h"
#include "tk.h"

#include "common/h/String.h"

#include <iostream.h>

class tclInstallIdle {
 private:
   bool currentlyInstalled;

   void (*usersRoutine)(ClientData cd);
   ClientData usersClientData;
      // needed since we overwrite clientdata with "this"

 private:
   static void installMe(ClientData cd);
      // "cd" will be "this"

 public:
   tclInstallIdle(void (*iUsersRoutine)(ClientData));
  ~tclInstallIdle();

   void install(ClientData cd);
};

void myTclEval(Tcl_Interp *interp, const char *, bool exitOnError=true);
void myTclEval(Tcl_Interp *interp, const pdstring &, bool exitOnError=true);
void tclpanic(Tcl_Interp *interp, const pdstring &str);

void getScrollBarValues(Tcl_Interp *, const pdstring &sbName,
                        float &theFirst, float &theLast);

float moveScrollBar(Tcl_Interp *, const pdstring &sbName,
                    float tentativeNewFirst);
   // returns actual new-first, after (possible) pinning

int set_scrollbar(Tcl_Interp *interp, const pdstring &sbname,
                  int total_width, 
                  int global_coord, int &screen_coord);

bool processScrollCallback(Tcl_Interp *interp,
                           int argc, const char **argv,
                           const pdstring &sbName,
			   int oldoffsetUnits, int totalWidthUnits,
			   int visibleWidthUnits,
                           float &newFirst);
   // tk4.0 has a complex method of reporting scroll events.
   // It will invoke a callback with a variable number of arguments,
   // depending on whether the sb was unit-moved, page-moved, or dragged.
   // This routine parses those arguments, modifies the scrollbar,
   // and returns true iff anything changed.  If so, the parameter
   // "newFirst" is modified (so you can read it and invoke application-specific
   // stuff.  After all, this routine merely updates the scrollbar)

void resizeScrollbar(Tcl_Interp *interp, const pdstring &sbName,
                     int total_width, int visible_width);

void setResultBool(Tcl_Interp *interp, bool val);

class tcl_cmd_installer {
 private:
   static void dummy_delete_proc(ClientData) {}

 public:
   tcl_cmd_installer(Tcl_Interp *interp, const pdstring &tclCmdName, Tcl_CmdProc proc) {
      Tcl_CreateCommand(interp, const_cast<char*>(tclCmdName.c_str()), 
			proc, NULL, &dummy_delete_proc);
   }

   tcl_cmd_installer(Tcl_Interp *interp, const pdstring &tclCmdName, Tcl_CmdProc proc, ClientData cd) {
      Tcl_CreateCommand(interp, const_cast<char*>(tclCmdName.c_str()), 
			proc, cd, &dummy_delete_proc);
   }
  ~tcl_cmd_installer() {}
};

#endif
