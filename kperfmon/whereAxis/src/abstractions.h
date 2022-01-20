// abstractions.h
// Ariel Tamches

// Holds a collection of "whereAxis".  Each "whereAxis"
// is 1 abstraction; hence, this class maintains a set of
// abstractions.

/* $Id: abstractions.h,v 1.14 2004/04/22 21:23:25 mjbrim Exp $ */

#ifndef _ABSTRACTIONS_H_
#define _ABSTRACTIONS_H_

#include <limits.h>

#include "common/h/Vector.h"

#include "whereAxis.h"
#include "tkTools.h" // myTclEval

template <class whereAxisChildrenOracle>
class abstractions {
 private:
   typedef whereAxis<whereAxisChildrenOracle> whereAxisType;

   struct whereAxisStruct {
      whereAxisType *theWhereAxis;
      pdstring abstractionName; // what's used in the menu

      // These values save where axis state when changing abstractions.
      // On changing abstractions, we use these stashed-away values to
      // reset the scrollbars, the navigate menu, and the text in the find box.
      float horizSBfirst, horizSBlast;
      float vertSBfirst, vertSBlast;
      // pdvector<pdstring> navigateMenuItems; [not needed; each whereAxis has
      //                                    its own 'path'; just rethinkNavigateMenu()]
      pdstring findString;
   };

   pdvector<whereAxisStruct> theAbstractions;
   unsigned currAbstractionIndex; // an index into the above array

   Tcl_Interp *interp;
   Tk_Window   theTkWindow;
   pdstring absMenuName; // can be empty pdstring: no menu
   pdstring navigateMenuName; // can be empty pdstring: no menu
   pdstring horizSBName, vertSBName;
   pdstring findName;
   const char *rootTextFontString;
   const char *listboxTextFontString;

   bool inBatchMode;

   pdvector<int> cpusel;

   whereAxisType &getCurrent() {
      assert(existsCurrent());
      whereAxisType *result = theAbstractions[currAbstractionIndex].theWhereAxis;
      assert(result);
      
      return *result;
   }

   const whereAxisType &getCurrent() const {
      assert(existsCurrent());
      whereAxisType *result = theAbstractions[currAbstractionIndex].theWhereAxis;
      assert(result);
      
      return *result;
   }

 public:
   Tk_Window getTkWindow() const {return theTkWindow;}
   const pdstring &getHorizSBName() const {return horizSBName;}
   const pdstring &getVertSBName() const {return vertSBName;}
   const pdstring &getAbsMenuName() const {return absMenuName;}

   abstractions(const pdstring &iabsMenuName, const pdstring &iNavMenuName,
		const pdstring &iHorizSBName, const pdstring &iVertSBName,
		const pdstring &iFindName,
		Tcl_Interp *iInterp, Tk_Window iTkWindow,
                const char *iRootTextFontString,
                const char *iListboxTextFontString);

  ~abstractions();

   void add(whereAxisType *theNewAbstraction,
	    const pdstring &whereAxisName);
      // theNewAbstraction must have been allocated with new.
      // It now belongs to the abstractions class; we'll delete it for you
      // in our dtor.  Sorry for the imbalance.

   bool wouldAddingItemInCurrentBeADuplicate(const pdstring &name,
                                             unsigned parentId) const {
      return getCurrent().wouldAddingItemBeDuplicate(name, parentId);
   }

   void addItemInCurrent(const pdstring &name,
                         unsigned parentId,
                         unsigned childId,
                         bool rethinkGraphicsNow,
                         bool resortNow,
                         bool mayAlreadyExist,
                         bool) {
      getCurrent().addItem(name, parentId, childId,
                           rethinkGraphicsNow,
                           resortNow, mayAlreadyExist);
   }

   void deleteItemInCurrent(unsigned nodeId,
                            unsigned parentNodeId,
                            bool rethinkGraphicsNow,
                            bool resortNow,
                            bool mayNotExist) {
      getCurrent().deleteItem(nodeId, parentNodeId,
                              rethinkGraphicsNow,
                              resortNow, mayNotExist);
   }
   
   void doneAddingInCurrent(bool resortNow) {
      getCurrent().recursiveDoneAddingChildren(resortNow);
   }

   whereAxisType &operator[](const pdstring &absName);

   int name2index(const pdstring &name) const;
      // returns -1 if not found

   bool change(unsigned newindex);
   bool change(const pdstring &newName);

   bool existsCurrent() const {
      return currAbstractionIndex < theAbstractions.size();
   }

   bool drawCurrent(bool doubleBuffer, bool xsynchOn);
      // checks batch mode; if true, rethinks before the redraw
      // returns true iff a rethink was needed during the draw, so you should redraw

   // there are 2 ways to get the selections: getCurrAbsSelectionsAsResIds (the old,
   // crusty, and inconvenient way) and getCurrAbsSelectionsAsFoci (the new,
   // convenient way).
   pdvector< pdvector<unsigned> >
   getCurrAbsSelectionsAsResIds(bool &wholeProgram,
                                pdvector<unsigned> &wholeProgramFocus) const {
      // returns a vector[num-hierarchies] of vector of selections.
      // The number of hierarchies is defined as the number of children of the
      // root node.
      return getCurrent().getSelectionsAsResIds(wholeProgram, wholeProgramFocus);
   }
   pdvector<typename whereAxisType::focus> getCurrAbsSelectionsAsFoci() const;
      // returns foci selected -- usually what you want.  That's why this routine is
      // better than getCurrAbsSelectionsAsResIds.
   pdvector<unsigned> expandResourceIdInCurrAbs(unsigned resid) const;

   // Return the vector of selected cpu ids. -1 corresponds to the sum
   // of all cpus
   const pdvector<int>& getCurrentCPUSelections() const {
       return cpusel;
   }

   // Change the selection state of the given cpu -- update the list of
   // currently selected CPUs
   void updateCurrentCPUSelections(int cpu_id, int state) {
       if (state == 0) { // Remove cpu_id from the list

	   pdvector<int> newsel;
	   
	   pdvector<int>::const_iterator iter = cpusel.begin();
	   for (; iter != cpusel.end(); iter++) {
	       if (*iter != cpu_id) {
		   newsel += *iter;
	       }
	   }
	   cpusel = newsel;
       }
       else { // Add cpu_id to the list
	   pdvector<int>::const_iterator iter = cpusel.begin();
	   for (; iter != cpusel.end(); iter++) {
	       if (*iter == cpu_id) {
                   // The item already exists. For example, it was selected 
		   // individually and now we are selecting every CPU
		   return; 
	       }
	   }
	   cpusel += cpu_id;
       }
   }

   void resizeEverything(bool resort) {
      if (!existsCurrent())
         return;

      for (unsigned i=0; i < theAbstractions.size(); i++) {
         theAbstractions[i].theWhereAxis->recursiveDoneAddingChildren(resort);
         theAbstractions[i].theWhereAxis->resize(i==currAbstractionIndex);
      }
   }

   void resizeCurrent() {
      if (!existsCurrent()) return;
      getCurrent().resize(true); // true --> resize sb's since we're curr displayed abs
   }

   void makeVisibilityUnobscured() {
      for (unsigned i=0; i < theAbstractions.size(); i++)
         theAbstractions[i].theWhereAxis->makeVisibilityUnobscured();
   }
   void makeVisibilityPartiallyObscured() {
      for (unsigned i=0; i < theAbstractions.size(); i++)
         theAbstractions[i].theWhereAxis->makeVisibilityPartiallyObscured();
   }
   void makeVisibilityFullyObscured() {
      for (unsigned i=0; i < theAbstractions.size(); i++)
         theAbstractions[i].theWhereAxis->makeVisibilityFullyObscured();
   }

   void startBatchMode();
   void endBatchMode();

   bool fullName2PathInCurr(const pdstring &theName,
                            simpSeq<unsigned> &thePath) {
      // can't be a const method since it may load in children
      return getCurrent().fullName2Path(theName, thePath);
   }

   bool selectUnSelectFromPath(const simpSeq<unsigned> &thePath,
                               bool expandFlag) {
      // returns true iff the item was found
      // pass true for the 2nd param iff you want to select it; false
      // if you want to unselect it.

      return getCurrent().selectUnSelectFromPath(thePath, expandFlag);
   }

   void processSingleClick(int x, int y) {
      getCurrent().processSingleClick(x,y);
   }
 
   bool processDoubleClick(int x, int y) {
      return getCurrent().processDoubleClick(x,y);
   }

   bool processShiftDoubleClick(int x, int y) {
      return getCurrent().processShiftDoubleClick(x,y);
   }

   bool processCtrlDoubleClick(int x, int y) {
      return getCurrent().processCtrlDoubleClick(x,y);
   }

   int getVertSBOffset() const { return getCurrent().getVertSBOffset(); }
   int getHorizSBOffset() const { return getCurrent().getHorizSBOffset(); }
   int getTotalVertPixUsed() const { return getCurrent().getTotalVertPixUsed(); }
   int getTotalHorizPixUsed() const { return getCurrent().getTotalHorizPixUsed(); }
   int getVisibleVertPix() const { return getCurrent().getVisibleVertPix(); }
   int getVisibleHorizPix() const { return getCurrent().getVisibleHorizPix(); }

   bool adjustVertSBOffset(float newfirstfrac) {
      return getCurrent().adjustVertSBOffset(newfirstfrac);
   }
   bool adjustVertSBOffsetFromDeltaPix(int deltay) {
      return getCurrent().adjustVertSBOffsetFromDeltaPix(deltay);
   }
   bool adjustHorizSBOffset(float newfirstfrac) {
      return getCurrent().adjustHorizSBOffset(newfirstfrac);
   }
   bool adjustHorizSBOffsetFromDeltaPix(int deltax) {
      return getCurrent().adjustHorizSBOffsetFromDeltaPix(deltax);
   }

   void clearSelections() { getCurrent().clearSelections(); }
   void navigateTo(unsigned pathLen) { getCurrent().navigateTo(pathLen); }

   bool forciblyExpandAndScrollToEndOfPath(whereNodePosRawPath &thePath) {
      // returns true iff any changes
      return getCurrent().forciblyScrollToEndOfPath(thePath);
   }

   int find(const pdstring &str) { return getCurrent().find(str); }
};

#endif
