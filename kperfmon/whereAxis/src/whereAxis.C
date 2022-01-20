// whereAxis.C
// Ariel Tamches

// A where axis corresponds to _exactly_ one Paradyn abstraction.

/* $Id: whereAxis.C,v 1.15 2004/04/22 21:23:33 mjbrim Exp $ */

#include <stdlib.h> // exit()

#include "util/h/minmax.h"
#include "util/h/rope-utils.h" // num2string()

#include "whereAxis.h"

#include "pdutil/h/odometer.h"

#ifdef WHEREAXISTEST
// only the where axis test program needs this
#include "whereAxisMisc.h"
#include "util/h/hashFns.h"
#endif

#include "tkTools.h"

template <class childrenOracle>
int whereAxis<childrenOracle>::listboxBorderPix = 3;

template <class childrenOracle>
int whereAxis<childrenOracle>::listboxScrollBarWidth = 16;



template <class childrenOracle>
void whereAxis<childrenOracle>::initializeStaticsIfNeeded() {
   rootNode::initializeStaticsIfNeeded(consts);
}

template <class childrenOracle>
whereAxis<childrenOracle>::whereAxis(Tcl_Interp *in_interp, Tk_Window theTkWindow,
                                     const pdstring &root_str,
                                     const pdstring &iHorizSBName,
                                     const pdstring &iVertSBName,
                                     const pdstring &iNavigateMenuName, // can be empty
                                     const char *rootTextFontString,
                                     const char *listboxTextFontString) :
	     consts(in_interp, theTkWindow,
                    rootTextFontString, listboxTextFontString),
	     hash(&whereAxis<childrenOracle>::hashFunc),
	     horizSBName(iHorizSBName),
	     vertSBName(iVertSBName),
	     navigateMenuName(iNavigateMenuName) {
   initializeStaticsIfNeeded();

   const unsigned rootResHandle = 0;

   rootNode tempRootNode(rootResHandle, rootResHandle, root_str);
   rootPtr = new nodeType(tempRootNode);
   assert(rootPtr);

   hash[rootResHandle] = rootPtr;

   beginSearchFromPtr = NULL;

   interp = in_interp;
      
   horizScrollBarOffset = vertScrollBarOffset = 0;
      
   rethink_nominal_centerx();

   nonSliderButtonCurrentlyPressed = false;
   nonSliderCurrentSubtree = NULL;
   slider_currently_dragging_subtree = NULL;
}

template <class childrenOracle>
void whereAxis<childrenOracle>::rethink_nominal_centerx() {
   // using Tk_Width(theTkWindow) as the available screen width, and
   // using root->myEntireWidthAsDrawn(consts) as the amount of screen real
   // estate used, this routine rethinks this->nominal_centerx.

   const int horizSpaceUsedByTree = rootPtr->entire_width(consts);

   // If the entire tree fits, then set center-x to window-width / 2.
   // Otherwise, set center-x to used-width / 2;
   if (horizSpaceUsedByTree <= Tk_Width(consts.theTkWindow))
      nominal_centerx = Tk_Width(consts.theTkWindow) / 2;
   else
      nominal_centerx = horizSpaceUsedByTree / 2;
}

template <class childrenOracle>
void whereAxis<childrenOracle>::resizeScrollbars() {
   pdstring commandStr = pdstring("resize1Scrollbar ") + horizSBName + " " +
                       num2string(rootPtr->entire_width(consts)) + " " +
                       num2string(Tk_Width(consts.theTkWindow));
   myTclEval(interp, commandStr);

   commandStr = pdstring("resize1Scrollbar ") + vertSBName + " " +
      num2string(rootPtr->entire_height(consts)) + " " +
      num2string(Tk_Height(consts.theTkWindow));
   myTclEval(interp, commandStr);
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::set_scrollbars(int absolute_x, int relative_x,
			       int absolute_y, int relative_y,
			       bool warpPointer) {
   // Sets the scrollbars s.t. (absolute_x, absolute_y) will appear
   // at window (relative) location (relative_x, relative_y).
   // May need to take into account the current scrollbar setting...

   bool anyChanges = true;

   horizScrollBarOffset = -set_scrollbar(interp, horizSBName,
					 rootPtr->entire_width(consts),
					 absolute_x, relative_x);
      // relative_x will be updated
   
   vertScrollBarOffset = -set_scrollbar(interp, vertSBName,
					rootPtr->entire_height(consts),
					absolute_y, relative_y);

#if !defined(i386_unknown_nt4_0)
   if (warpPointer)
      XWarpPointer(Tk_Display(consts.theTkWindow),
		   Tk_WindowId(consts.theTkWindow), // src win
		   Tk_WindowId(consts.theTkWindow), // dest win
		   0, 0, // src x,y
		   0, 0, // src height, width
		   relative_x, relative_y
		   );
#else // !defined(i386_unknown_nt4_0)
	// TODO - implement warping behavior (?)
#endif // !defined(i386_unknown_nt4_0)

   return anyChanges;
}

template <class childrenOracle>
typename whereAxis<childrenOracle>::pathType
whereAxis<childrenOracle>::point2path(int x, int y) const {
   const int overallWindowBorderPix = 0;
   const int root_centerx = nominal_centerx + horizScrollBarOffset;
      // relative (not absolute) coord.  note: horizScrollBarOffset <= 0
   const int root_topy = overallWindowBorderPix + vertScrollBarOffset;
      // relative (not absolute) coord.  note: vertScrollBarOffset <= 0

   return pathType(x, y, consts, rootPtr,
                   root_centerx, root_topy);
}

template <class childrenOracle>
void whereAxis<childrenOracle>::nonSliderButtonRelease(ClientData cd, XEvent *) {
   whereAxis *pthis = (whereAxis *)cd;

   pthis->nonSliderButtonCurrentlyPressed = false;
   Tk_DeleteTimerHandler(pthis->buttonAutoRepeatToken);
}

template <class childrenOracle>
void whereAxis<childrenOracle>::nonSliderButtonAutoRepeatCallback(ClientData cd) {
   // If the mouse button has been released, do NOT re-invoke the timer.
   whereAxis *pthis = (whereAxis *)cd;
   if (!pthis->nonSliderButtonCurrentlyPressed)
      return;

   const int listboxLeft = pthis->nonSliderSubtreeCenter -
                           pthis->nonSliderCurrentSubtree->
			     horiz_pix_everything_below_root(pthis->consts) / 2;
   const int listboxTop = pthis->nonSliderSubtreeTop +
                          pthis->nonSliderCurrentSubtree->getNodeData().
			       getHeightAsRoot() +
			  pthis->consts.vertPixParent2ChildTop;

   const int listboxActualDataPix = pthis->nonSliderCurrentSubtree->getListboxActualPixHeight() - 2 * listboxBorderPix;
   int deltaYpix = 0;
   int repeatIntervalMillisecs = 100;

   switch (pthis->nonSliderButtonPressRegion) {
      case pathType::ListboxScrollbarUpArrow:
         deltaYpix = -4;
         repeatIntervalMillisecs = 10;
         break;
      case pathType::ListboxScrollbarDownArrow:
         deltaYpix = 4;
         repeatIntervalMillisecs = 10;
         break;
      case pathType::ListboxScrollbarPageup:
         deltaYpix = -listboxActualDataPix;
         repeatIntervalMillisecs = 250; // not so fast
         break;
      case pathType::ListboxScrollbarPagedown:
         deltaYpix = listboxActualDataPix;
         repeatIntervalMillisecs = 250; // not so fast
         break;
      default:
         assert(false);
   }

   (void)pthis->nonSliderCurrentSubtree->scroll_listbox(pthis->consts,
							listboxLeft, listboxTop,
							deltaYpix);
   pthis->buttonAutoRepeatToken = Tk_CreateTimerHandler(repeatIntervalMillisecs,
							nonSliderButtonAutoRepeatCallback,
							pthis);
}

template <class childrenOracle>
void whereAxis<childrenOracle>::
processNonSliderButtonPress(pathType &thePath) {
   nonSliderButtonCurrentlyPressed = true;
   nonSliderButtonPressRegion = thePath.whatDoesPathEndIn();
   nonSliderCurrentSubtree = thePath.getLastPathNode(rootPtr);
   nonSliderSubtreeCenter = thePath.get_endpath_centerx();
   nonSliderSubtreeTop = thePath.get_endpath_topy();

   Tk_CreateEventHandler(consts.theTkWindow, ButtonReleaseMask,
			 nonSliderButtonRelease, this);

   nonSliderButtonAutoRepeatCallback(this);
}

template <class childrenOracle>
void whereAxis<childrenOracle>::sliderMouseMotion(ClientData cd, XEvent *eventPtr) {
   assert(eventPtr->type == MotionNotify);
   whereAxis *pthis = (whereAxis *)cd;

   const int y = eventPtr->xmotion.y;
   const int amount_moved = y - pthis->slider_initial_yclick;
      // may be negative, of course.
   const int newScrollBarSliderTopPix = pthis->slider_initial_scrollbar_slider_top +
                                        amount_moved;

   assert(pthis->slider_currently_dragging_subtree != NULL);
   (void)pthis->slider_currently_dragging_subtree->
	   rigListboxScrollbarSliderTopPix(pthis->consts, pthis->slider_scrollbar_left,
					   pthis->slider_scrollbar_top,
					   pthis->slider_scrollbar_bottom,
					   newScrollBarSliderTopPix,
					   true // redraw now
					   );
}

template <class childrenOracle>
void whereAxis<childrenOracle>::sliderButtonRelease(ClientData cd, XEvent *eventPtr) {
   assert(eventPtr->type == ButtonRelease);
   whereAxis *pthis = (whereAxis *)cd;
      
   Tk_DeleteEventHandler(pthis->consts.theTkWindow,
			 PointerMotionMask,
			 sliderMouseMotion,
			 pthis);
   Tk_DeleteEventHandler(pthis->consts.theTkWindow,
			 ButtonReleaseMask,
			 sliderButtonRelease,
			 pthis);
   pthis->slider_currently_dragging_subtree = NULL;
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::wouldAddingItemBeDuplicate(const pdstring &newName,
                                                           unsigned parentId) const {
   nodeType *parentPtr = hash.get(parentId);
   assert(parentPtr != NULL);

   return parentPtr->alreadyExistsChildName(newName);
}

template <class childrenOracle>
void whereAxis<childrenOracle>::addItem(const pdstring &newName,
                                        unsigned parentUniqueId,
                                        unsigned newNodeUniqueId,
                                        bool rethinkGraphicsNow,
                                        bool resortNow,
                                        bool mayAlreadyExist) {
   nodeType *parentPtr = hash.get(parentUniqueId);
   assert(parentPtr != NULL);

   // First thing's first: don't add the item if it's a duplicate
   if (mayAlreadyExist) {
      // It may be a duplicate
      if (parentPtr->alreadyExistsChildName(newName))
         return;
   }

   rootNode tempRootNode(newNodeUniqueId, parentUniqueId, newName);
   nodeType *newNode = new nodeType(tempRootNode);
   assert(newNode);

   parentPtr->addChild(newNode,
                       false, // not explicitly expanded
                       consts,
                       false, // rethink layout now?
                       resortNow);

   hash.set(newNodeUniqueId, newNode);

   if (rethinkGraphicsNow)
      resize(true); // super-expensive
}

template <class childrenOracle>
void whereAxis<childrenOracle>::deleteItem(unsigned nodeUniqueId,
                                           unsigned parentNodeUniqueId,
                                           bool rethinkGraphicsNow,
                                           bool resortNow,
                                           bool mayNotExist) {
   nodeType *parentPtr = hash.get(parentNodeUniqueId);
   assert(parentPtr);
   
   if (mayNotExist && !hash.defines(nodeUniqueId))
      return;

   if (!parentPtr->deleteChild(hash.get(nodeUniqueId), // child ptr
                               consts,
                               true,
                               resortNow)) {
      cout << "WARNING whereAxis.C found both parent & child id's in hash table"
           << endl
           << "but could not find the child as a successor of the parent!" << endl;

      assert(false);
   }

   (void)hash.get_and_remove(nodeUniqueId);

   if (rethinkGraphicsNow)
      resize(true); // super-expensive
}

#ifdef WHEREAXISTEST
// only the where axis test program uses this stuff:

template <class childrenOracle>
int whereAxis<childrenOracle>::readTree(ifstream &is,
                                              unsigned parentUniqueId,
                                              unsigned nextUniqueIdToUse) {
   // returns the number of new nodes added, 0 if nothing could be read
   char c;
   is >> c;
   if (!is) return false;
   if (c == ')') {
      is.putback(c);
      return 0;
   }

   const bool oneItemTree = (c != '(');
   if (oneItemTree)
      is.putback(c);

   pdstring rootString = readItem(is);
   if (rootString.length() == 0)
      return 0; // could not read root name

   childrenOracle::addNewEntry(nextUniqueIdToUse, rootString);

   const bool thisIsWhereAxisRoot = (nextUniqueIdToUse==parentUniqueId);
   if (thisIsWhereAxisRoot) {
      // a little hack for the where axis root node, which can't be added
      // using addItem().  Not to mention we have to set "whereAxis::rootPtr"

      rootNode tempRootNode(nextUniqueIdToUse, nextUniqueIdToUse, rootString);
      nodeType *newParentNode = new nodeType (tempRootNode);

      assert(newParentNode);

      assert(nextUniqueIdToUse==0);
      assert(!hash.defines(nextUniqueIdToUse));
      hash[nextUniqueIdToUse] = newParentNode;
      assert(hash.defines(nextUniqueIdToUse));

      rootPtr = newParentNode;
   }
   else {
      childrenOracle::markARelationship(parentUniqueId, nextUniqueIdToUse);
      
//      this->addItem(rootString, parentUniqueId, nextUniqueIdToUse, false, false);
//      // don't redraw; don't resort
   }
   
   if (oneItemTree)
      return 1;

   parentUniqueId = nextUniqueIdToUse++;

   // now the children (if any)
   int result = 1; // number of nodes read in so far...
   while (true) {
      int localResult = readTree(is, parentUniqueId, nextUniqueIdToUse);
      if (localResult == 0)
         break;

      result += localResult;
      nextUniqueIdToUse += localResult;
   }

   // now, do some graphical rethinking w.r.t. "result" that we had suppressed
   // up until now in the name of improved performance...
//   assert(hash.defines(parentUniqueId));
//   nodeType *theParentNode = hash[parentUniqueId];

//   theParentNode->doneAddingChildren(consts, true); // true --> resort

   // eat the closing )
   is >> c;
   if (c != ')') {
      cerr << "expected ) to close tree, but found " << c << endl;
      return 0;
   }

   if (thisIsWhereAxisRoot) {
      // all done (?).  Root node unique id is 0
      nodeType *theRootNode = hash.get(0);

      const pdvector<unsigned> &childIds = childrenOracle::getChildrenOf(0);
      
      for (pdvector<unsigned>::const_iterator iter = childIds.begin();
           iter != childIds.end(); ++iter) {
         const unsigned second_level_resid = *iter;

         const pdstring &name = childrenOracle::id2Name(second_level_resid);
         
         this->addItem(name,
                       0, // parent id
                       second_level_resid,
                       false, false); // don't redraw, don't resort
      }

      theRootNode->doneAddingChildren(consts, true); // true --> resort
   }

   return result;
}

// only the where axis test program gets this routine
template <class childrenOracle>
whereAxis<childrenOracle>::whereAxis(ifstream &infile, Tcl_Interp *in_interp,
                                     Tk_Window theTkWindow,
                                     const char *iHorizSBName, const char *iVertSBName,
                                     const char *iNavigateMenuName) :
                              consts(in_interp, theTkWindow,
                                     "helvetica 14", "helvetica 12"),
			      hash(hashFunc),
                              horizSBName(iHorizSBName),
                              vertSBName(iVertSBName),
                              navigateMenuName(iNavigateMenuName) {
   initializeStaticsIfNeeded();

   interp = in_interp;
   horizScrollBarOffset = vertScrollBarOffset = 0;

   unsigned rootNodeUniqueId = 0;
   const int result = readTree(infile, rootNodeUniqueId, rootNodeUniqueId);
   assert(result > 0);

   beginSearchFromPtr = NULL;  

   rethink_nominal_centerx();
}
#endif

template <class childrenOracle>
bool whereAxis<childrenOracle>::draw(bool doubleBuffer,
                                     bool xsynchronize // are we debugging?
   ) {
   // returns true iff things were out of date, and a redraw should be rescheduled
   // (when idle, of course, for best performance).

   if (xsynchronize)
      doubleBuffer = false;
   
   Drawable theDrawable = doubleBuffer ? consts.offscreenPixmap :
      Tk_WindowId(consts.theTkWindow);

   if (doubleBuffer || xsynchronize)
      // clear the drawable before drawing onto it
      XFillRectangle(consts.display,
                     theDrawable,
                     consts.erasingGC,
                     0, // x-offset relative to drawable
                     0, // y-offset relative to drawable
                     Tk_Width(consts.theTkWindow),
                     Tk_Height(consts.theTkWindow)
                     );

   const int overallWindowBorderPix = 0;
   bool needToRedraw = false;

   if (rootPtr) {
      const bool thingsAreOutOfDate =
         rootPtr->draw(consts.theTkWindow, consts, theDrawable,
                       nominal_centerx + horizScrollBarOffset,
                       // relative (not absolute) coord
                       overallWindowBorderPix + vertScrollBarOffset,
                       // relative (not absolute) coord
                       false, // not root only
                       false // not listbox only
                       );
      
      if (thingsAreOutOfDate) {
         cout << "draw() detected odd things; rethinking layout..." << endl;
         recursiveDoneAddingChildren(true); // true --> resort, too

         needToRedraw = true;
      }
   }

   if (!needToRedraw && doubleBuffer) {
      // don't bother with the expensive XCopyArea if we're about to redraw anyway
      // copy from offscreen pixmap onto the 'real' window
      XCopyArea(consts.display,
                theDrawable, // source pixmap
                Tk_WindowId(consts.theTkWindow), // dest pixmap
                consts.erasingGC, // ?????
                0, 0, // source x,y pix
                Tk_Width(consts.theTkWindow),
                Tk_Height(consts.theTkWindow),
                0, 0 // dest x,y offset pix
                );
   }

   return needToRedraw;
}

template <class childrenOracle>
void whereAxis<childrenOracle>::resize(bool currentlyDisplayedAbstraction) {
   const int newWindowWidth = Tk_Width(consts.theTkWindow);
//   const int newWindowHeight = Tk_Height(consts.theTkWindow); [not used]

   consts.resize(); // reallocates offscreen pixmap; rethinks
                    // 'listboxHeightWhereSBappears'

   // Loop through EVERY listbox (gasp) and rethink whether or not it needs
   // a scrollbar and (possibly) change listBoxActualHeight...
   // _Must_ go _after_ the consts.resize()
   (void)rootPtr->rethinkListboxAfterResize(consts);

   // Now the more conventional resize code:
   rootPtr->rethinkAfterResize(consts, newWindowWidth);
   rethink_nominal_centerx(); // must go _after_ the rootPtr->rethinkAfterResize()

   if (currentlyDisplayedAbstraction) {
      // We have changed axis width and/or height.  Let's inform tcl, so it may
      // rethink the scrollbar ranges.
      resizeScrollbars();

      // Now, let's update our own stored horiz & vert scrollbar offset values
      adjustHorizSBOffset(); // obtain FirstPix from the actual tk scrollbar
      adjustVertSBOffset (); // obtain FirstPix from the actual tk scrollbar
   }
}

template <class childrenOracle>
void whereAxis<childrenOracle>::processSingleClick(int x, int y) {
   pathType thePath=point2path(x, y);

   switch (thePath.whatDoesPathEndIn()) {
      case pathType::Nothing:
//         cout << "single-click in nothing at (" << x << "," << y << ")" << endl;
         return;
      case pathType::ExpandedNode: {
//         cout << "click on an non-listbox item; adjusting NAVIGATE menu..." << endl;
         lastClickPath = thePath.getPath();
         rethinkNavigateMenu();

         // Now redraw the node in question...(its highlightedness changed)
         nodeType *ptr = thePath.getLastPathNode(rootPtr);
         ptr->toggle_highlight();
         ptr->getNodeData().drawAsRoot(consts.theTkWindow,
				       Tk_WindowId(consts.theTkWindow),
				       thePath.get_endpath_centerx(),
				       thePath.get_endpath_topy());
         return;
      }
      case pathType::ListboxItem:
//         cout << "click on a listbox item; adjusting NAVIGATE menu..." << endl;
         lastClickPath = thePath.getPath();
         rethinkNavigateMenu();

         // Now we have to redraw the item in question...(its highlightedness changed)
         thePath.getLastPathNode(rootPtr)->toggle_highlight();

         thePath.getParentOfLastPathNode(rootPtr)->draw(consts.theTkWindow,
							consts, Tk_WindowId(consts.theTkWindow),
							thePath.get_endpath_centerx(),
							thePath.get_endpath_topy(),
							false, // not root only
							true // listbox only
							);
         break;
      case pathType::ListboxScrollbarUpArrow:
      case pathType::ListboxScrollbarDownArrow:
      case pathType::ListboxScrollbarPageup:
      case pathType::ListboxScrollbarPagedown:
         processNonSliderButtonPress(thePath);
         return;
      case pathType::ListboxScrollbarSlider: {
//         cout << "looks like a click in a listbox scrollbar slider" << endl;

         nodeType *parentPtr = thePath.getLastPathNode(rootPtr);

         slider_initial_yclick = y;
         slider_currently_dragging_subtree = parentPtr;

         const int lbTop = thePath.get_endpath_topy() +
	                   parentPtr->getNodeData().getHeightAsRoot() +
			   consts.vertPixParent2ChildTop;

         int dummyint;
         parentPtr->getScrollbar().getSliderCoords(lbTop,
		     lbTop + parentPtr->getListboxActualPixHeight() - 1,
		     parentPtr->getListboxActualPixHeight() - 2*listboxBorderPix,
		     parentPtr->getListboxFullPixHeight() - 2*listboxBorderPix,
		     slider_initial_scrollbar_slider_top, // filled in
		     dummyint);

         slider_scrollbar_left = thePath.get_endpath_centerx() -
	                         parentPtr->horiz_pix_everything_below_root(consts) / 2;
         slider_scrollbar_top = thePath.get_endpath_topy() +
                                parentPtr->getNodeData().getHeightAsRoot() +
				consts.vertPixParent2ChildTop;
         slider_scrollbar_bottom = slider_scrollbar_top +
                                   parentPtr->getListboxActualPixHeight() - 1;

//         cout << "slider click was on subtree whose root name is "
//              << parentPtr->getRootName() << endl;

         Tk_CreateEventHandler(consts.theTkWindow,
			       ButtonReleaseMask,
			       sliderButtonRelease,
			       this);
	 Tk_CreateEventHandler(consts.theTkWindow,
			       PointerMotionMask,
			       sliderMouseMotion,
			       this);
         break;
      }
      default:
         assert(false);
   }
}

/* ***************************************************************** */

template <class childrenOracle>
bool whereAxis<childrenOracle>::processShiftDoubleClick(int x, int y) {
   // returns true iff a complete redraw is called for

   pathType thePath = point2path(x, y);

   switch (thePath.whatDoesPathEndIn()) {
      case pathType::Nothing:
//         cout << "shift-double-click in nothing" << endl;
         return false;
      case pathType::ListboxItem:
//         cout << "shift-double-click in lb item; ignoring" << endl;
         return false;
      case pathType::ExpandedNode:
         break; // some breathing room for lots of code to follow...
      case pathType::ListboxScrollbarUpArrow:
      case pathType::ListboxScrollbarDownArrow:
      case pathType::ListboxScrollbarPageup:
      case pathType::ListboxScrollbarPagedown:
         // in this case, do the same as a single-click
         processNonSliderButtonPress(thePath);
         return false; // no need to redraw further
      case pathType::ListboxScrollbarSlider:
//         cout << "shift-double-click in a listbox scrollbar slider...doing nothing" << endl;
         return false;
      default:
         assert(false);
   }

   // How do we know whether to expand-all or un-expand all?
   // Well, if there is no listbox up, then we should _definitely_ unexpand.
   // And, if there is a listbox up containing all items (no explicitly expanded items),
   //    then we should _definitely_ expand.
   // But, if there is a listbox up with _some_ explicitly expanded items, then
   //    are we supposed to expand out the remaining ones; or un-expand the expanded
   //    ones?  It could go either way.  For now, we'll say that we should un-expand.

   bool anyChanges = false; // so far...
   nodeType *ptr = thePath.getLastPathNode(rootPtr);
   ptr->toggle_highlight(); // doesn't redraw

   if (ptr->getListboxPixWidth() > 0) {
      bool noExplicitlyExpandedChildren = true; // so far...
      for (unsigned childlcv=0; childlcv < ptr->getNumChildren(); childlcv++)
         if (ptr->getChildIsExpandedFlag(childlcv)) {
            noExplicitlyExpandedChildren = false; 
            break;
         }

      if (noExplicitlyExpandedChildren)
         // There is a listbox up, and it contains ALL children.
         // So, obviously, we want to expand them all...
         anyChanges = rootPtr->path2ExpandAllChildren(consts, thePath.getPath(), 0);
      else
         // There is a listbox up, but there are also some expanded children.
         // This call (whether to expand the remaining listbox items, or whether
         // to un-expand the expanded items) could go either way; we choose the
         // latter (for now, at least)
         anyChanges = rootPtr->path2UnExpandAllChildren(consts, thePath.getPath(), 0);
   }
   else
      // No listbox is up; hence, all children are expanded.
      // So obviously, we wish to un-expand all the children.
      anyChanges = rootPtr->path2UnExpandAllChildren(consts, thePath.getPath(), 0);

   if (!anyChanges)
      return false;

   rethink_nominal_centerx();

   // We have changed axis width and/or height.  Let's inform tcl, so it may
   // rethink the scrollbar ranges.
   resizeScrollbars();

   // Now, let's update our own stored horiz & vert scrollbar offset values
   adjustHorizSBOffset(); // obtain FirstPix from the actual tk scrollbar
   adjustVertSBOffset (); // obtain FirstPix from the actual tk scrollbar

   return true;
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::processCtrlDoubleClick(int x, int y) {
   // returns true iff changes were made

   pathType thePath = point2path(x, y);

   if (thePath.whatDoesPathEndIn() != pathType::ExpandedNode)
      return false;

//   cout << "ctrl-double-click:" << endl;

   // How do we know whether to select-all or unselect-all?
   // Well, if noone is selected, then we should _definitely_ select all.
   // And, if everyone is selected, then we should _definitely_ unselect all.
   // But, if _some_ items are selected, what should we do?  It could go either way.
   // For now, we'll say unselect.

   nodeType *ptr = thePath.getLastPathNode(rootPtr);

   // change highlightedness of the root node (i.e. the double-click should undo
   // the effects of the earlier single-click, now that we know that a double-click
   // was the user's intention all along)
   ptr->toggle_highlight(); // doesn't redraw

   if (ptr->getNumChildren()==0)
      return true; // changes were made

   bool allChildrenSelected = true;
   bool noChildrenSelected = true;
   for (unsigned childlcv=0; childlcv < ptr->getNumChildren(); childlcv++)
      if (ptr->getChildTree(childlcv)->isHighlighted()) {
         noChildrenSelected = false;
         if (!allChildrenSelected)
            break;
      }
      else {
         allChildrenSelected = false;
         if (!noChildrenSelected)
            break;
      }

   assert(!(allChildrenSelected && noChildrenSelected));

   if (allChildrenSelected || !noChildrenSelected) {
      // unselect all children
      for (unsigned childlcv=0; childlcv < ptr->getNumChildren(); childlcv++)
         ptr->getChildTree(childlcv)->unhighlight();
      return true; // changes were made
   }
   else {
      assert(noChildrenSelected);
      for (unsigned childlcv=0; childlcv < ptr->getNumChildren(); childlcv++)
         ptr->getChildTree(childlcv)->highlight();
      return true; // changes were made
   }
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::processDoubleClick(int x, int y) {
   // returns true iff a complete redraw is called for

   bool scrollToWhenDone = false; // for now...

   pathType thePath = point2path(x, y);

   switch (thePath.whatDoesPathEndIn()) {
      case pathType::Nothing:
//         cout << "looks like a double-click in nothing" << endl;
         return false;
      case pathType::ListboxScrollbarUpArrow:
      case pathType::ListboxScrollbarDownArrow:
      case pathType::ListboxScrollbarPageup:
      case pathType::ListboxScrollbarPagedown:
         // in this case, do the same as a single-click
         processNonSliderButtonPress(thePath);
         return false; // no need to redraw further
      case pathType::ListboxScrollbarSlider:
//         cout << "double-click in a listbox scrollbar slider...doing nothing" << endl;
         return false;
      case pathType::ExpandedNode: {
         // double-click in a "regular" node (not in listbox): un-expand
//         cout << "double-click on a non-listbox item" << endl;

         if (thePath.getSize() == 0) {
//            cout << "double-click un-expansion on the root..ignoring" << endl;
            return false;
         }

         if (!rootPtr->path2lbItemUnexpand(consts, thePath.getPath(), 0)) {
            // probably an attempt to expand a leaf item (w/o a triangle next to it)
//            cout << "could not un-expand this subtree (a leaf?)...continuing" << endl;
            return false;
         }

         // Now let's scroll to the un-expanded item.
         rethink_nominal_centerx();
         resizeScrollbars();
         adjustHorizSBOffset();
         adjustVertSBOffset();
         softScrollToEndOfPath(thePath.getPath());
         return true;
      }
      case pathType::ListboxItem: {
         // double-click in a listbox item
//         cout << "double-click on a listbox item" << endl;
         // first thing's first: now that we know the user intended to do a double-click
         // all along, we should undo the effects of the single-click which came earlier.
         thePath.getLastPathNode(rootPtr)->toggle_highlight(); // doesn't redraw

         const bool anyChanges = rootPtr->path2lbItemExpand(consts,
							    thePath.getPath(), 0);

         if (!anyChanges) {
            // The only real change we made was the toggle_highlight().  This is
            // a case where we can do the redrawing ourselves (and fast).
            thePath.getParentOfLastPathNode(rootPtr)->
	         draw(consts.theTkWindow, consts, Tk_WindowId(consts.theTkWindow),
		      thePath.get_endpath_centerx(),
		      thePath.get_endpath_topy(),
		      false, // not root only
		      true // listbox only
		      );
            return false;
         }
         else {
            // expansion was successful...later, we'll scroll to the expanded item.
            // NOTE: rootPtr->path2lbItemExpand will have modified "thePath"
            //       for us (by changing the last item from a listbox item to
            //       an expanded one, which is the proper thing to do).

            scrollToWhenDone = true;
         }
         break;
      }
      default:
         assert(false);
   }

   // expansion or un-expansion successful

   rethink_nominal_centerx();

   // We have changed axis width and/or height.  Let's inform tcl, so it may
   // rethink the scrollbar ranges.
   resizeScrollbars();

   // Now, let's update our own stored horiz & vert scrollbar offset values
   adjustHorizSBOffset(); // obtain FirstPix from the actual tk scrollbar
   adjustVertSBOffset (); // obtain FirstPix from the actual tk scrollbar

   if (scrollToWhenDone) {
      const int overallWindowBorderPix = 0;

      pathType path_to_scroll_to(thePath.getPath(),
			consts, rootPtr,
			nominal_centerx,
                           // root centerx (abs. pos., regardless of sb [intentional])
			overallWindowBorderPix
                           // topy of root (abs. pos., regardless of sb [intentional])
			);

      int newlyExpandedElemCenterX = path_to_scroll_to.get_endpath_centerx();
      int newlyExpandedElemTopY    = path_to_scroll_to.get_endpath_topy();
      int newlyExpandedElemMiddleY = newlyExpandedElemTopY +
	                             path_to_scroll_to.getLastPathNode(rootPtr)->getNodeData().getHeightAsRoot() / 2;

      (void)set_scrollbars(newlyExpandedElemCenterX, x,
			   newlyExpandedElemMiddleY, y,
			   true);
   }

   return true;
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::softScrollToPathItem(const whereNodePosRawPath &thePath,
				     unsigned index) {
   // scrolls s.t. the (centerx, topy) of path index is in middle of screen.

   whereNodePosRawPath newPath = thePath;
     // make a copy, because we're gonna hack it up a bit.
   newPath.rigSize(index);
   return softScrollToEndOfPath(newPath);
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::
forciblyScrollToPathItem(const whereNodePosRawPath &thePath,
                         unsigned pathLen) {
   // Simply a stub which generates a new path and calls forciblyScrollToEndOfPath()   
   // "index" indicates the length of the new path; in particular, 0 will give
   // and empty path (the root node).

   // make a copy of "thePath", truncate it to "pathLen", and forciblyScrollToEndOfPath
   whereNodePosRawPath newPath = thePath;
   newPath.rigSize(pathLen);
   return forciblyScrollToEndOfPath(newPath);
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::softScrollToEndOfPath(const whereNodePosRawPath &thePath) {
   const int overallWindowBorderPix = 0;
   pathType
         scrollToPath(thePath, consts, rootPtr,
		      nominal_centerx,
		         // ignores scrollbar settings (an absolute coord),
		      overallWindowBorderPix
		         // absolute coord of root topy
		      );
   const int last_item_centerx = scrollToPath.get_endpath_centerx();
   const int last_item_topy    = scrollToPath.get_endpath_topy();
      // note: if the path ends in an lb item, these coords refer to the PARENT
      //       node, which is expanded.

   switch (scrollToPath.whatDoesPathEndIn()) {
      case pathType::ExpandedNode: {
//         cout << "soft scrolling to expanded node" << endl;
         const int last_item_middley =
            last_item_topy +
            scrollToPath.getLastPathNode(rootPtr)->getNodeData().getHeightAsRoot() / 2;
         return set_scrollbars(last_item_centerx,
			       Tk_Width(consts.theTkWindow) / 2,
			       last_item_middley,
			       Tk_Height(consts.theTkWindow) / 2,
			       true);
      }
      case pathType::ListboxItem: {
//         cout << "soft scrolling to lb item" << endl;

         // First, let's scroll within the listbox (no redrawing yet)
         nodeType *parent = scrollToPath.getParentOfLastPathNode(rootPtr);

         Tk_FontMetrics lbFontMetrics; // filled in by Tk_GetFontMetrics()
         Tk_GetFontMetrics(consts.listboxFontStruct, &lbFontMetrics);
         const unsigned itemHeight = consts.listboxVertPadAboveItem +
                                     lbFontMetrics.ascent +
   	                             consts.listboxVertPadAfterItemBaseline;
   
         int scrollToVertPix = 0; // relative to listbox top
         const unsigned childnum = scrollToPath.getPath().getLastItem();
         for (unsigned childlcv=0; childlcv < childnum; childlcv++)
            if (!parent->getChildIsExpandedFlag(childlcv))
               scrollToVertPix += itemHeight;

         int destItemRelToListboxTop = scrollToVertPix;   
         if (parent->getScrollbar().isValid()) {
            (void)parent->scroll_listbox(consts, scrollToVertPix -
					 parent->getScrollbar().getPixFirst());

            destItemRelToListboxTop -= parent->getScrollbar().getPixFirst();
         }

         if (destItemRelToListboxTop < 0) {
            cout << "note: softScrollToEndOfPath() failed to scroll properly to item w/in listbox" << endl;
         }

         return set_scrollbars(scrollToPath.get_endpath_centerx() -
			          parent->horiz_pix_everything_below_root(consts)/2 +
  			          parent->getListboxPixWidth() / 2,
   			          // listbox centerx
			       Tk_Width(consts.theTkWindow) / 2,
			       scrollToPath.get_endpath_topy() +
			          parent->getNodeData().getHeightAsRoot() +
			          consts.vertPixParent2ChildTop +
			          destItemRelToListboxTop + itemHeight / 2,
			          // should be the middley of the lb item
			       Tk_Height(consts.theTkWindow) / 2,
			       true);
      }
      default: assert(false);
   }

   assert(false);
   return false; // placate compiler
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::
forciblyScrollToEndOfPath(const whereNodePosRawPath &thePath) {
   // Forcibly expands path items as needed, then calls softScrollToEndOfPath()

   const bool anyChanges = rootPtr->expandEntirePath(consts, thePath, 0);
   if (anyChanges) {
      rethink_nominal_centerx();
      resizeScrollbars();
      adjustHorizSBOffset();
      adjustVertSBOffset();
   }

   softScrollToEndOfPath(thePath);

   return anyChanges;
}

template <class childrenOracle>
void whereAxis<childrenOracle>::navigateTo(unsigned pathLen) {
   (void)forciblyScrollToPathItem(lastClickPath, pathLen);
}

template <class childrenOracle>
int whereAxis<childrenOracle>::find(const pdstring &str) {
   // does a blind search for the given string.  Expands things along the
   // way if needed.  Returns 0 if not found, 1 if found & no expansions
   // were needed, 2 if found & expansion(s) _were_ needed

   // Our strategy: (1) make a path out of the string.
   //               (2) call a routine that "forcefully" scrolls to the end of that
   //                   path.  We say "forcefully" beceause this routine will
   //                   expand items along the way, if necessary.

   // Uses and alters "beginSearchFromPtr"

   whereNodePosRawPath thePath;
   int result = rootPtr->substring2Path(thePath, consts, str, beginSearchFromPtr, true);

   if (result == 0)
      if (beginSearchFromPtr == NULL)
         return 0; // not found
      else {
         // try again with beginSearchFromPtr of NULL (wrap-around search)
//         cout << "search wrap-around" << endl;
         beginSearchFromPtr = NULL;
         return find(str);
      }

   // found.  Update beginSearchFromPtr.
   nodeType *beginSearchFromPtr = rootPtr;
   for (unsigned i=0; i < thePath.getSize(); i++)
      beginSearchFromPtr = beginSearchFromPtr->getChildTree(thePath[i]);

   if (result==1)
      (void)softScrollToEndOfPath(thePath);
   else {
      assert(result==2);
      bool aflag;
      aflag = (forciblyScrollToEndOfPath(thePath));
      // rethinks nominal centerx, resizes scrollbars, etc.
      assert(aflag);
   }

   return result;
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::adjustHorizSBOffset(float newFirst) {
   // does not redraw.  Returns true iff any changes.

   // First, we need to make the change to the tk scrollbar
   newFirst = moveScrollBar(interp, horizSBName, newFirst);

   // Then, we update our C++ variables
   int widthOfEverything = rootPtr->entire_width(consts);
   int oldHorizScrollBarOffset = horizScrollBarOffset;
   horizScrollBarOffset = -(int)(newFirst * widthOfEverything);
      // yes, horizScrollBarOffset is always negative (unless it's zero)
   return (horizScrollBarOffset != oldHorizScrollBarOffset);
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::adjustHorizSBOffsetFromDeltaPix(int deltapix) {
   // does not redraw.  Returns true iff any changes.

   const int widthOfEverything = rootPtr->entire_width(consts);
   float newFirst = (float)(-horizScrollBarOffset + deltapix) / widthOfEverything;
   return adjustHorizSBOffset(newFirst);
}

//template <class NODEDATA>
//bool whereAxis<NODEDATA>::adjustHorizSBOffsetFromDeltaPages(int deltapages) {
//   // does not redraw.  Returns true iff any changes.
//
//   // First, update the tk scrollbar
//   const int widthOfEverything = rootPtr->entire_width(consts);
//   const int widthOfVisible = Tk_Width(consts.theTkWindow);
//   float newFirst = (float)(-horizScrollBarOffset + widthOfVisible*deltapages) /
//                     widthOfEverything;
//   return adjustHorizSBOffset(newFirst);
//}

template <class childrenOracle>
bool whereAxis<childrenOracle>::adjustHorizSBOffset() {
   // Does not redraw.  Obtains PixFirst from actual tk scrollbar.
   float first, last; // fractions (0.0 to 1.0)
   getScrollBarValues(interp, horizSBName, first, last);

   return adjustHorizSBOffset(first);
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::adjustVertSBOffset(float newFirst) {
   // does not redraw.  Returns true iff any changes.
   // First, we need to make the change to the tk scrollbar
   newFirst = moveScrollBar(interp, vertSBName, newFirst);

   // Then, we update our C++ variables
   int heightOfEverything = rootPtr->entire_height(consts);
   int oldVertScrollBarOffset = vertScrollBarOffset;
   vertScrollBarOffset = -(int)(newFirst * heightOfEverything);
      // yes, vertScrollBarOffset is always negative (unless it's zero)
   return (vertScrollBarOffset != oldVertScrollBarOffset);
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::adjustVertSBOffsetFromDeltaPix(int deltapix) {
   // does not redraw.  Returns true iff any changes were made.

   const int heightOfEverything = rootPtr->entire_height(consts);
   float newFirst = (float)(-vertScrollBarOffset + deltapix) / heightOfEverything;
   return adjustVertSBOffset(newFirst);
}

//template <class NODEDATA>
//bool whereAxis<NODEDATA>::adjustVertSBOffsetFromDeltaPages(int deltapages) {
//   // does not redraw
//
//   // First, update the tk scrollbar
//   const int heightOfEverything = rootPtr->entire_height(consts);
//   const int heightOfVisible = Tk_Height(consts.theTkWindow);
//   float newFirst = (float)(-vertScrollBarOffset + heightOfVisible*deltapages) /
//                    heightOfEverything;
//   return adjustVertSBOffset(newFirst);
//}

template <class childrenOracle>
bool whereAxis<childrenOracle>::adjustVertSBOffset() {
   // Does not redraw.  Obtains PixFirst from actual tk scrollbar.
   float first, last; // fractions (0.0 to 1.0)
   getScrollBarValues(interp, vertSBName, first, last);

   return adjustVertSBOffset(first);
}

/* ************************************************************************ */

template <class childrenOracle>
void whereAxis<childrenOracle>::rethinkNavigateMenu() {
   // We loop through the items of "lastClickPath".  For each item,
   // we get the name of the root node, and append the full-path entry
   // to the navigate menu.

   if (navigateMenuName.length() == 0)
      return;

   nodeType *currTree = rootPtr;   

   // Note: in tk4.0, menu indices start at 1, not 0 (0 is reserved for tearoff)
   pdstring commandStr = navigateMenuName + " delete 1 100";
   myTclEval(interp, commandStr);

   pdstring theString;

   unsigned itemlcv = 0;
   while (true) {
      if (itemlcv >= 1)
         theString += "/";
      theString += currTree->getNodeData().getName();

//      cout << "adding " << theString << " to the navigate menu" << endl;

      pdstring commandStr = navigateMenuName +
                          " add command -label {" + theString + "} " +
                          "-command {whereAxisNavigateTo " +
                          num2string(itemlcv) + "}";
      myTclEval(interp, commandStr);

      if (itemlcv >= lastClickPath.getSize())
         break;

      unsigned theItem = lastClickPath[itemlcv++];
      currTree = currTree->getChildTree(theItem);
   }
}

template <class childrenOracle>
bool whereAxis<childrenOracle>::
selectUnSelectFromPath(const simpSeq<unsigned> &thePath, bool selectFlag) {
   return rootPtr->path2SelectUnSelect(selectFlag, consts, thePath, 0);
}

template <class childrenOracle>
pdvector< pdvector<unsigned> >
whereAxis<childrenOracle>::
getSelectionsAsResIds(bool &wholeProgram,
pdvector<unsigned> &wholeProgramFocus) const {
   // returns a vector[num-hierarchies] of vector of selections.
   // The number of hierarchies is defined as the number of children of the
   // root node.  If "Whole Program" was selection, it isn't returned with
   // the main result; it's returned by modifying the 2 params
   const unsigned numHierarchies = rootPtr->getNumChildren();

   pdvector< pdvector<unsigned> > result(numHierarchies);

   bool wholeProgramImplicit = true; // so far...

   for (unsigned i=0; i < numHierarchies; i++) {
      nodeType *hierarchyRoot = rootPtr->getChildTree(i);
      pdvector<const rootNode *> thisHierarchySelections = hierarchyRoot->getSelections();

      if (thisHierarchySelections.size()==0)
         // add hierarchy's root item
         thisHierarchySelections += &hierarchyRoot->getNodeData();
      else
         // since the hierarchy selection was not empty, we do _not_
         // want to implicitly select whole-program
         wholeProgramImplicit = false;

      result[i].resize(thisHierarchySelections.size());
      for (unsigned j=0; j < thisHierarchySelections.size(); j++)
         result[i][j] = thisHierarchySelections[j]->getUniqueId();
   }

   wholeProgram = wholeProgramImplicit || rootPtr->isHighlighted();
   if (wholeProgram) {
      // write to wholeProgramFocus:
      wholeProgramFocus.resize(numHierarchies);
      for (unsigned i=0; i < numHierarchies; i++) {
         nodeType *hierarchyRoot = rootPtr->getChildTree(i);
         const rootNode &hierarchyRootData = hierarchyRoot->getNodeData();
         unsigned hierarchyRootUniqueId = hierarchyRootData.getUniqueId();
         wholeProgramFocus[i] = hierarchyRootUniqueId;
      }
   }

   return result;
}

template <class childrenOracle>
pdvector<typename whereAxis<childrenOracle>::focus>
whereAxis<childrenOracle>::getSelectionsAsFoci() const {
   // formerly encapsulated as fn parseSelections() of uimpd.tcl.C; we moved it
   // here because it seemed to belong to this class.
   
   bool plusWholeProgram;
   focus wholeProgramFocus;
   pdvector< pdvector<unsigned> > theHierarchy=getSelectionsAsResIds(plusWholeProgram, wholeProgramFocus);

   // how many resources were selected in each hierarchy?:
   pdvector<unsigned> numResourcesByHierarchy(theHierarchy.size());
   for (unsigned hier=0; hier < theHierarchy.size(); hier++)
      numResourcesByHierarchy[hier] = theHierarchy[hier].size();

   odometer theOdometer(numResourcesByHierarchy);
   pdvector<focus> result;

   while (!theOdometer.done()) {
      // create a focus using the odometer's current setting
      // Make a note if we have added the equivalent of "Whole Program"

      focus theFocus(theHierarchy.size());

      bool addedEquivOfWholeProgram = plusWholeProgram; // so far...

      for (unsigned hier=0; hier < theHierarchy.size(); hier++) {
         theFocus[hier] = theHierarchy[hier][theOdometer[hier]];
         if (addedEquivOfWholeProgram)
	    addedEquivOfWholeProgram = (theFocus[hier] == wholeProgramFocus[hier]);
      }

      if (!addedEquivOfWholeProgram)
         result += theFocus;
//      else
//         cout << "Suppressing duplicate of whole program" << endl;

      theOdometer++;
   }

   // There is one final thing to check for: whole-program
   if (plusWholeProgram)
      result += wholeProgramFocus;

//   cout << "parseSelections: returning result of " << result.size() << " foci" << endl;
   return result;
}

template <class childrenOracle>
pdvector<unsigned> whereAxis<childrenOracle>::expandResourceId(unsigned resid) const {
   // Let's say resource id #56 represents /code/foo.c/bar
   // This routine returns a vector of (in this example) 3 items: the resource id for
   // /code, the resource id for foo.c, and the resource id for bar (which'll be #56 in
   // this example).

   nodeType *resourceNode = hash.get(resid);
   assert(resourceNode);

   unsigned parentResId = resourceNode->getNodeData().getParentUniqueId();
   if (parentResId == resid) {
      // this must be the root node
      pdvector<unsigned> result;
      result += resid;
      return result;
   }

   // recurse, then append resid
   pdvector<unsigned> result = expandResourceId(parentResId);
   result += resid;
   return result;
}

template <class childrenOracle>
void whereAxis<childrenOracle>::clearSelections() {
   rootPtr->recursiveClearSelections();
}
