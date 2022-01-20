// whereAxis.h
// Ariel Tamches

// A where axis corresponds to _exactly_ one Paradyn abstraction.

/* $Id: whereAxis.h,v 1.10 2004/04/22 21:23:34 mjbrim Exp $ */

#ifndef _WHERE_AXIS_H_
#define _WHERE_AXIS_H_

#include <fstream.h>
#include "where4treeConstants.h"
#include "where4tree.h"
#include "rootNode.h"
#include "graphicalPath.h"
#include "common/h/String.h"

#include "util/h/Dictionary.h"

// whereAxisChildrenOracle: see where4tree.h

template <class whereAxisChildrenOracle>
class whereAxis {
 private:
   typedef whereAxisRootNode rootNode;
   typedef where4tree<rootNode, whereAxisChildrenOracle> nodeType;
   typedef whereNodeGraphicalPath<rootNode, whereAxisChildrenOracle> pathType;

   // this appears to be the WRONG class for the following vrbles:
   static int listboxBorderPix; // 3
   static int listboxScrollBarWidth; // 16

   // These ugly variables keep track of a button press (and possible hold-down)
   // in scrollbar-up/down or pageup/pagedown region.  They are not used for
   // press (and possible hold-down) on the scrollbar slider
   bool nonSliderButtonCurrentlyPressed; // init to false
   typename pathType::pathEndsIn nonSliderButtonPressRegion;
   Tk_TimerToken buttonAutoRepeatToken;
   nodeType *nonSliderCurrentSubtree;
   int nonSliderSubtreeCenter;
   int nonSliderSubtreeTop;

   // Analagous to above; used only for scrollbar slider
   int slider_scrollbar_left, slider_scrollbar_top, slider_scrollbar_bottom;
   int slider_initial_yclick, slider_initial_scrollbar_slider_top;
   nodeType *slider_currently_dragging_subtree;

   void initializeStaticsIfNeeded();

   where4TreeConstants consts;
      // Each where axis has its own set of constants, so different axis may,
      // for example, have different color configurations.
   nodeType *rootPtr;

   dictionary_hash<unsigned, nodeType *> hash;
      // associative array: resource unique id --> its corresponding node

   pdstring horizSBName; // e.g. ".nontop.main.bottsb"
   pdstring vertSBName;  // e.g. ".nontop.main.leftsb"
   pdstring navigateMenuName; // if empty then we don't have any such menu

   whereNodePosRawPath lastClickPath;
      // used in the navigate menu
   nodeType *beginSearchFromPtr;
      // if NULL, then begin from the top.  Otherwise,
      // find() treats ignores found items until this one is reached.

   Tcl_Interp *interp;

   bool obscured;
      // true if the underlying window is partially or fully obscured.
      // Currently, used to know when to properly redraw after scrolling
      // a listbox, since I'm having trouble getting tk to recognize
      // GraphicsExpose events...sigh

   int nominal_centerx; // actual centerx = nominal_centerx + horizScrollBarOffset
   int horizScrollBarOffset; // always <= 0

   // why isn't there a nominal_topy?  Because it's always at the top of the window
   // (0 or 3)
   int vertScrollBarOffset; // always <= 0

   void resizeScrollbars();

   bool set_scrollbars(int absolute_x, int relative_x,
		       int absolute_y, int relative_y,
		       bool warpPointer);
      // returns true iff any sb changes were made
      // Moves the cursor if warpPointer is true.

   pathType point2path(int x, int y) const;

   static void nonSliderButtonRelease(ClientData cd, XEvent *);
   static void nonSliderButtonAutoRepeatCallback(ClientData cd);

   void processNonSliderButtonPress(pathType &thePath);

   static void sliderMouseMotion(ClientData cd, XEvent *eventPtr);
   static void sliderButtonRelease(ClientData cd, XEvent *eventPtr);

 protected:
   void rethink_nominal_centerx();

   static unsigned hashFunc(const unsigned &uniqueId) {return uniqueId;}
      // needed for hash table class...

#ifdef WHEREAXISTEST
   // only the where axis test program reads from a file
   int readTree(ifstream &,
		unsigned parentUniqueId,
		unsigned nextAvailChildId);
      // returns # of nodes read in.  Recursive.
#endif
		 
 public:
   typedef pdvector<unsigned> focus; // vector of resource id's
   
   whereAxis(Tcl_Interp *in_interp, Tk_Window theTkWindow,
	     const pdstring &root_str,
	     const pdstring &iHorizSBName, const pdstring &iVertSBName,
	     const pdstring &iNavigateMenuName, // can be empty
             const char *rootTextFontString,
             const char *listboxTextFontString);

#ifdef WHEREAXISTEST
   // only the where axis test program reads from a file
   whereAxis(ifstream &infile, Tcl_Interp *interp, Tk_Window theTkWindow,
	     const char *iHorizSBName, const char *iVertSBName,
	     const char *iNavigateMenuName);
#endif

  ~whereAxis() {delete rootPtr;}

   // the return values of the next 2 routines will be <= 0
   int getVertSBOffset() const {return vertScrollBarOffset;}
   int getHorizSBOffset() const {return horizScrollBarOffset;}

   int getTotalVertPixUsed() const {return rootPtr->entire_height(consts);}
   int getTotalHorizPixUsed() const {return rootPtr->entire_width(consts);}

   int getVisibleVertPix() const {return Tk_Height(consts.theTkWindow);}
   int getVisibleHorizPix() const {return Tk_Width(consts.theTkWindow);}

   bool wouldAddingItemBeDuplicate(const pdstring &name,
                                   unsigned parentId) const;
   void addItem(const pdstring &name,
		unsigned parentUniqueId,
		unsigned newNodeUniqueId,
		bool rethinkGraphicsNow,
		bool resortNow,
                bool mayAlreadyExist);

   void deleteItem(unsigned nodeUniqueId,
                   unsigned parentNodeUniqueId,
                   bool rethinkGraphicsNow,
                   bool resortNow,
                   bool mayNotExit);

   void recursiveDoneAddingChildren(bool resortNow) {
      rootPtr->recursiveDoneAddingChildren(consts, resortNow);
   }

   bool draw(bool doubleBuffer, bool isXsynchOn);
      // returns true iff a resort/rethink is needed

   void resize(bool rethinkScrollbars);
      // should be true only if we are the currently displayed abstraction

   void makeVisibilityUnobscured() {consts.makeVisibilityUnobscured();}
   void makeVisibilityPartiallyObscured() {consts.makeVisibilityPartiallyObscured();}
   void makeVisibilityFullyObscured() {consts.makeVisibilityFullyObscured();}

   void processSingleClick(int x, int y);
   bool processDoubleClick(int x, int y);
      // returns true iff a redraw of everything is still needed
   bool processShiftDoubleClick(int x, int y);
   bool processCtrlDoubleClick (int x, int y);

   int find(const pdstring &str);
      // uses and updates "beginSearchFromPtr"
      // returns 0 if not found; 1 if found & no expansion needed;
      // 2 if found & some expansion is needed

   bool softScrollToPathItem(const whereNodePosRawPath &thePath, unsigned index);
      // scrolls s.t. the (centerx, topy) of the path item in question is placed in the
      // middle of the screen.  Returns true iff the scrollbar settings changed.

   bool softScrollToEndOfPath(const whereNodePosRawPath &thePath);
      // Like the above routine, but always scrolls to the last item in the path.

   bool forciblyScrollToEndOfPath(const whereNodePosRawPath &thePath);
      // Like the above routine, but explicitly expands any un-expanded children
      // along the path.
   bool forciblyScrollToPathItem(const whereNodePosRawPath &thePath, unsigned pathLen);

   // Noe of these scrollbar adjustment routines redraw anything
   bool adjustHorizSBOffset(float newFirstFrac);
   bool adjustHorizSBOffsetFromDeltaPix(int deltapix);
      // needed for alt-mousemove
//   bool adjustHorizSBOffsetFromDeltaPages(int deltapages);
   bool adjustHorizSBOffset(); // Obtains FirstPix from actual tk scrollbar

   bool adjustVertSBOffset (float newFirstFrac);
   bool adjustVertSBOffsetFromDeltaPix(int deltapix);
      // needed for alt-mousemove
//   bool adjustVertSBOffsetFromDeltaPages(int deltapages);
   bool adjustVertSBOffset(); // Obtains FirstPix from actual tk scrollbar

   void rethinkNavigateMenu();
   void navigateTo(unsigned pathLen);
      // forcibly scrolls to item #pathLen of "lastClickPath"

//     bool selectUnSelectFromFullPathName(const pdstring &name, bool select);
//        // returns true iff the item was found
//        // pass true for the 2nd param iff you want to select it; false
//        // if you want to unselect it.
   bool selectUnSelectFromPath(const simpSeq<unsigned> &thePath, bool selectFlag);

   bool fullName2Path(const pdstring &theName,
                      simpSeq<unsigned> &thePath) {
      // can't be a const method since it may load in children
      return rootPtr->fullName2Path(thePath, theName, consts);
   }

   // getSelectionsAsResIds: the lowest-level way to obtain the selections.
   // returns a vector (whose size is the # of resource hierarchies; i.e. the # of
   // children of whole-program), each containing the selections (resource ids)
   // made in this hierarchy.  It's low level because we haven't sliced and diced
   // these selections into the foci that were chosen; you have to do that on your
   // own.  Or better yet, if that's what you're after, call getSelectionsAsFoci()
   // instead (which in turn calls getSelectionsAsResIds).
   pdvector< pdvector<unsigned> > getSelectionsAsResIds(bool &wholeProgram, pdvector<unsigned> &wholeProgramFocus) const;

   // The high-level way to get selections:
   pdvector<focus> getSelectionsAsFoci() const;

   pdvector<unsigned> expandResourceId(unsigned resid) const;
      // Let's say resource id #56 represents /code/foo.c/bar
      // This routine returns a vector of (in this example) 3 items: the resource id for
      // /code, the resource id for foo.c, and the resource id for bar (which'll be
      // #56 in this example).
   
   void clearSelections();
};

#endif
