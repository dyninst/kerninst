// abstractions.C

/* $Id: abstractions.C,v 1.12 2004/04/22 21:23:24 mjbrim Exp $ */

#include "common/h/String.h"

#include "abstractions.h"
#include "focus.h"

template <class whereAxisChildrenOracle> 
abstractions<whereAxisChildrenOracle>::
abstractions(const pdstring &iabsMenuName, const pdstring &iNavMenuName,
	     const pdstring &iHorizSBName, const pdstring &iVertSBName,
	     const pdstring &iFindName,
	     Tcl_Interp *iInterp, Tk_Window iTkWindow,
	     const char *iRootTextFontString,
	     const char *iListboxTextFontString) :
    absMenuName(iabsMenuName), navigateMenuName(iNavMenuName),
    horizSBName(iHorizSBName), vertSBName(iVertSBName),
    findName(iFindName),
    rootTextFontString(iRootTextFontString),
    listboxTextFontString(iListboxTextFontString)
{
    currAbstractionIndex = UINT_MAX;
    interp = iInterp;
    theTkWindow = iTkWindow;

    inBatchMode = false;
    cpusel += CPU_TOTAL;
}

template <class whereAxisChildrenOracle> 
abstractions<whereAxisChildrenOracle>::
~abstractions() 
{
    for (unsigned i=0; i < theAbstractions.size(); i++)
	delete theAbstractions[i].theWhereAxis;
 
    currAbstractionIndex = UINT_MAX;
}

template <class whereAxisChildrenOracle>
void abstractions<whereAxisChildrenOracle>::add(whereAxisType *theNewAbstraction,
                                                const pdstring &whereAxisName) {
   whereAxisStruct theStruct;
   theStruct.abstractionName = whereAxisName;
   theStruct.theWhereAxis = theNewAbstraction;
   theStruct.horizSBfirst = theStruct.horizSBlast = 0.0;
   theStruct.vertSBfirst  = theStruct.vertSBlast = 0.0;

   bool firstAbstraction = false;

   theAbstractions += theStruct;
   if (theAbstractions.size() == 1) {
      firstAbstraction = true;
   }

   if (absMenuName.length() > 0) {
      pdstring commandStr = absMenuName + " add radiobutton -label " +
         "\"" + whereAxisName + "\"" +
         " -command " + "\"" + "whereAxisChangeAbstraction " +
         num2string(theAbstractions.size()) + "\"" +
         " -variable currMenuAbstraction -value " +
         num2string(theAbstractions.size());
      myTclEval(interp, commandStr);
   }
   
   // Now let us set the tcl variable "currMenuAbstraction", which
   // should generate a call to "change", below
   if (firstAbstraction) {
      currAbstractionIndex = 0;
      pdstring commandStr = pdstring("set currMenuAbstraction 1");
      myTclEval(interp, commandStr);
   }   
}

template <class whereAxisChildrenOracle>
typename abstractions<whereAxisChildrenOracle>::whereAxisType &
abstractions<whereAxisChildrenOracle>::operator[](const pdstring &absName) {
   // given an abstraction name, this routine returns the where axis
   // structure.  If no abstraction/where-axis exists with the given
   // name, however, we ADD A NEW WHERE-AXIS and return that one.
   // This routine pretty much ignores the concept of a current where axis.

   for (unsigned i=0; i < theAbstractions.size(); i++)
      if (absName == theAbstractions[i].abstractionName) {
         assert(theAbstractions[i].theWhereAxis);
         return *(theAbstractions[i].theWhereAxis);
      }

   // cout << "abstractions[]: adding a new where axis..." << endl;
   whereAxisType *theNewWhereAxis = new whereAxisType
                 (interp, theTkWindow, "Whole Program",
                  horizSBName, vertSBName, navigateMenuName,
                  rootTextFontString, listboxTextFontString);
   assert(theNewWhereAxis);

   add(theNewWhereAxis, absName);
   return *theNewWhereAxis;
}

template <class whereAxisChildrenOracle>
int abstractions<whereAxisChildrenOracle>::name2index(const pdstring &name) const {
   // returns -1 if not found
   for (unsigned i=0; i < theAbstractions.size(); i++)
      if (name == theAbstractions[i].abstractionName)
         return i;

   return -1;
}

template <class whereAxisChildrenOracle>
bool abstractions<whereAxisChildrenOracle>::change(unsigned newindex) {
   // returns true iff any changes
   if (newindex == currAbstractionIndex)
      // nothing to do...
      return false;

   // Save current scrollbar values
   whereAxisStruct &was = theAbstractions[currAbstractionIndex];

   pdstring commandStr = horizSBName + " get";
   myTclEval(interp, commandStr);
   bool aflag;
   aflag=(2==sscanf(interp->result,"%f %f",&was.horizSBfirst,&was.horizSBlast));
   assert(aflag);

   commandStr = vertSBName + " get";
   myTclEval(interp, commandStr);
   aflag = (2==sscanf(interp->result, "%f %f", &was.vertSBfirst, &was.vertSBlast));
   assert(aflag);

   // Save current find string
   commandStr = findName + " get";
   myTclEval(interp, commandStr);
   was.findString = interp->result;

   // Set new scrollbar values
   whereAxisStruct &newWas = theAbstractions[currAbstractionIndex = newindex];
   commandStr = horizSBName + " set " + num2string(newWas.horizSBfirst) + " "
                                      + num2string(newWas.horizSBlast);
   myTclEval(interp, commandStr);

   commandStr = vertSBName + " set " + num2string(newWas.vertSBfirst) + " "
                                     + num2string(newWas.vertSBlast);
   myTclEval(interp, commandStr);

   // Set the new navigate menu:
   newWas.theWhereAxis->rethinkNavigateMenu();

   // Set the new find string
   commandStr = findName + " delete 0 end";
   myTclEval(interp, commandStr);

   commandStr = findName + " insert 0 " + "\"" + newWas.findString + "\"";
   myTclEval(interp, commandStr);

   // Finally, we must be safe and assume that the toplevel window
   // has been resized...in short, we need to simulate a resize right now.
   // (code in test.C does this for us...)

   return true;
}

template <class whereAxisChildrenOracle>
bool abstractions<whereAxisChildrenOracle>::change(const pdstring &newName) {
   // unlike change(unsigned), we return true if successful (as opposed
   // to if any changes were made)

   for (unsigned i=0; i < theAbstractions.size(); i++) {
      whereAxisStruct &was = theAbstractions[i];
      if (was.abstractionName == newName) {
         (void)change(i);
         return true; // success         
      }
   }

   return false; // could not find any abstraction with that name
}

template <class whereAxisChildrenOracle>
bool abstractions<whereAxisChildrenOracle>::
drawCurrent(bool doubleBuffer, bool xsynchOn) {
   // checks batch mode; if true, rethinks before the redraw
   if (!existsCurrent()) return false;

   if (inBatchMode) {
      //cerr << "rethinking before redraw since batch mode is true!" << endl;
      resizeEverything(true); // resorts, rethinks layout...expensive!
   }

   const bool neededRethinking = getCurrent().draw(doubleBuffer, xsynchOn);
   return neededRethinking;
}

template <class whereAxisChildrenOracle>
void abstractions<whereAxisChildrenOracle>::startBatchMode() {
   inBatchMode = true;
      // when true, draw() can draw garbage, so in such cases, we rethink
      // before drawing.  (expensive, of course)
}

template <class whereAxisChildrenOracle>
void abstractions<whereAxisChildrenOracle>::endBatchMode() {
   inBatchMode = false;
   resizeEverything(true); // resorts, rethinks layout.  expensive.
}

//  template <class whereAxisChildrenOracle>
//  bool abstractions<whereAxisChildrenOracle>::
//  selectUnSelectFromFullPathName(const pdstring &name, bool select) {
//     assert(existsCurrent());
//     return getCurrent().selectUnSelectFromFullPathName(name, select);
//  }

template <class whereAxisChildrenOracle>
pdvector<typename whereAxis<whereAxisChildrenOracle>::focus>
abstractions<whereAxisChildrenOracle>::getCurrAbsSelectionsAsFoci() const {
   return getCurrent().getSelectionsAsFoci();
}

template <class whereAxisChildrenOracle>
pdvector<unsigned> abstractions<whereAxisChildrenOracle>::expandResourceIdInCurrAbs(unsigned resid) const {
   return getCurrent().expandResourceId(resid);
}

