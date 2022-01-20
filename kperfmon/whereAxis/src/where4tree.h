// where4tree.h
// Ariel Tamches

// Header file for subtree based on where4.fig [and where5.fig]

/* $Id: where4tree.h,v 1.14 2004/04/22 21:23:31 mjbrim Exp $ */

// This class is sort of a placeholder.  It has variables to find out who
// is expanded and who isn't; it maintains the tree layout.
// But many details are left to the template <USERNODE> class, which
// is whereAxisRootNode for the where axis.  Such a class actually holds
// the node name, draws the node (both root and within-listbox), etc.
// In particular, this template class can maintain extra information
// allowing it to draw specially.  For example, the upcoming search-history-graph
// root node will maintain a state (instrumented, false, true, not instrumented)
// needed to do its drawing.

#ifndef _WHERE4TREE_H_
#define _WHERE4TREE_H_

#include <stdlib.h>

#include "tkTools.h"

#include "common/h/String.h"
#include "common/h/Vector.h"

#include "simpSeq.h"

#include "where4treeConstants.h"

#include "scrollbar.h"

/* ********************************************************************* */

typedef simpSeq<unsigned> whereNodePosRawPath;

// An explanation of the template params:
// NODEDATA -- a class to hold information on the contents of the node.  It will
//    certainly contain the node's name.  It can store other stuff, such as "color",
//    as desired.  It must have certain methods: getName(), getPixWidthAsListboxItem(),
//    getPixHeightAsListboxItem() [constant], draw_as_listbox_item(),
//    getPixHeightAsRoot(), drawAsRoot(), more...
// childrenOracle -- a new class that should help us save memory in big where axes
//    where the actual items in the where axis is calculable elsewhere (thus, there's
//    no inherent need to store it all in here...we can retrieve it on demand).
//    This class is consulted when numChildren **appears** to be 0; it may have
//    something to say about that, such as "no, it's not really 0; we were just saving
//    memory...now that you're asking for them, here are the children".
//    Methods that must be defined:
//    bool hasChildren(where4tree *parent)
//       // Useful for knowing if there are any children w/o actually filling them in.
//       // Useful, e.g., for knowing when to call drawTriangle()
//    bool fillInChildren(where4tree *parent)
//       // writes to parent->children[] if appropriate; returns true iff any changes


template <class NODEDATA, class childrenOracle>
class where4tree { // base class (some day)
   
 private:
   NODEDATA theNodeData;
      // must have several particular routines defined:

   bool anything2Draw;
      // if false, drawing & measurement routines will ignore the entire
      // subtree rooted here.  This value is updated when outside code calls
      // updateAnything2Draw(), which in turn checks the value of
      // NODEDATA::anything2Draw() for each node.

   bool changesSinceLastSort;
      // if false, no sorting is needed

   struct childstruct {
      where4tree *theTree;
      bool isExplicitlyExpanded;
   };

   static bool sort_comparitor(const childstruct &first,
                               const childstruct &second) {
      const pdstring &s1 = first.theTree->getRootName();
      const pdstring &s2 = second.theTree->getRootName();
      return (s1 < s2);
   }

   class search_comparitor {
    public:
      bool operator()(const pdstring &name,
                      const where4tree<NODEDATA, childrenOracle>::childstruct &cs) const {
         const pdstring &cs_name = cs.theTree->getRootName();
         return name < cs_name;
      }
      bool operator()(const childstruct &cs,
                      const pdstring &name) const {
         const pdstring &cs_name = cs.theTree->getRootName();
         return cs_name < name;
      }
   };
   
   pdvector<childstruct> theChildren;
      // a vec, not a set, because children are ordered.
      // Even if theChildren.size()==0, there might be children (that haven't been
      // "allocated", to save memory usage); consult the childrenOracle to know for
      // certain.

   int listboxPixWidth; // includes scrollbar, if applicable
   int listboxFullPixHeight; // total lb height (if no SB used)
   int listboxActualPixHeight; // we limit ourselves to a "window" this tall
 
   scrollbar theScrollbar;

   int allExpandedChildrenWidthAsDrawn, allExpandedChildrenHeightAsDrawn;
      // Both implicitly _and_ explicitly expanded children are included.
      // Listbox (if any) is _not_ included.
      // If no children are expanded, then these are 0.
      // Non-leaf children can be complex (have their own listboxes, subtrees, etc.).
      // This is counted.

 private:

   bool expandUnexpand1(const where4TreeConstants &tc, unsigned childindex,
                        bool expandFlag, bool rethinkFlag, bool force);
      // won't expand a leaf node.
      // NOTE: Expanding a child will require rethinking the all-expanded-children
      //       dimensions for ALL ancestors of the expandee.  This routine only takes
      //       care of updating those dimensions for the immediate ancestor (parent)
      //       of the expandee.  YOU MUST MANUALLY HANDLE THE REST OF THE PATH!

   // Mouse clicks and node expansion
   int point2ItemWithinListbox(const where4TreeConstants &tc,
			       int localx, int localy) const;
      // returns index of item clicked on (-2 if nothing), given point
      // local to the _data_ part of listbox.  0<=localy<listboxFullPixHeight

   int point2ItemOneStepScrollbar(const where4TreeConstants &tc,
				  int ypix,
				  int scrollbarTop,
				  int scrollbarHeight) const;
      // -3 for a point in listbox scrollbar up-arrow,
      // -4 for point in listbox scrollbar down-arrow,
      // -5 for point in listbox scrollbar pageup-region,
      // -6 for point in listbox scrollbar pagedown-region,
      // -7 for point in listbox scrollbar slider.

 public:
   int point2ItemOneStep(const where4TreeConstants &tc,
			 int xpix, int ypix,
			 int root_centerx, int root_topy) const;
      // returns -1 for a point in root, 0 thru n-1 for point w/in general
      // range of a child subtree [even if child is in a listbox],
      // -2 for a point on nothing,
      // -3 for a point in listbox scrollbar up-arrow,
      // -4 for point in listbox scrollbar down-arrow,
      // -5 for point in listbox scrollbar pageup-region,
      // -6 for point in listbox scrollbar pagedown-region,
      // -7 for point in listbox scrollbar slider.

 private:
   const where4tree *get_end_of_path0(const whereNodePosRawPath &thePath,
                                      unsigned index) const {
      if (index < thePath.getSize())
         return getChildTree(thePath[index])->get_end_of_path0(thePath, index+1);
      else
         return this;
   }

   where4tree *get_end_of_path0(const whereNodePosRawPath &thePath, unsigned index) {
      if (index < thePath.getSize())
         return getChildTree(thePath[index])->get_end_of_path0(thePath, index+1);
      else
         return this;
   }

   const where4tree *get_parent_of_end_of_path0(const whereNodePosRawPath &thePath,
                                                unsigned index) const {
      if (index < thePath.getSize()-1)
         return getChildTree(thePath[index])->get_parent_of_end_of_path0(thePath, index+1);
      else
         return this;
   }

   where4tree *get_parent_of_end_of_path0(const whereNodePosRawPath &thePath,
                                          unsigned index) {
      if (index < thePath.getSize()-1)
         return getChildTree(thePath[index])->get_parent_of_end_of_path0(thePath, index+1);
      else
         return this;
   }

 public:

   const where4tree *get_end_of_path(const whereNodePosRawPath &thePath) const {
      return get_end_of_path0(thePath, 0);
   }

   where4tree *get_end_of_path(const whereNodePosRawPath &thePath) {
      return get_end_of_path0(thePath, 0);
   }

   const where4tree *get_parent_of_end_of_path(const whereNodePosRawPath &thePath) const {
      return get_parent_of_end_of_path0(thePath, 0);
   }

   where4tree *get_parent_of_end_of_path(const whereNodePosRawPath &thePath) {
      return get_parent_of_end_of_path0(thePath, 0);
   }

   unsigned getNumChildren() const {return theChildren.size();}
   where4tree *getChildTree(unsigned index) {
      // should this routine return a reference instead?
      return theChildren[index].theTree;
   }
   const where4tree *getChildTree(unsigned index) const {
      // should this routine return a reference instead?
      return theChildren[index].theTree;
   }
   bool getChildIsExpandedFlag(unsigned index) const {
      return theChildren[index].isExplicitlyExpanded;
   }

   bool existsANonExplicitlyExpandedChild() const {
      // assert me when a listbox is up
      const unsigned numChildren = theChildren.size();
      for (unsigned childlcv = 0; childlcv < numChildren; childlcv++)
         if (!theChildren[childlcv].isExplicitlyExpanded)
           return true;
      return false;
   }

   int getListboxPixWidth() const {return listboxPixWidth;}
   int getListboxFullPixHeight() const {return listboxFullPixHeight;}
   int getListboxActualPixHeight() const {return listboxActualPixHeight;}
   scrollbar &getScrollbar() {return theScrollbar;}
   const scrollbar &getScrollbar() const {return theScrollbar;}

   void removeListbox();

   int scroll_listbox(const where4TreeConstants &tc, int deltaYpix);
      // returns true scroll amt.  Doesn't redraw

   bool scroll_listbox(const where4TreeConstants &tc,
		       int listboxLeft,
		       int listboxTop,
		       int deltaYpix);
      // returns true iff any changes were made.  This version redraws.

   void rethink_listbox_dimensions(const where4TreeConstants &tc);
      // an expensive routine; time = O(numchildren).  Calculating just the
      // listbox height could be quicker.

   static void drawTriangle(const where4TreeConstants &tc, Drawable theDrawable,
		            int triangleEndX, int currBaseLine);
      // cost is O(XFillPolygon())

   bool draw_listbox(Tk_Window, const where4TreeConstants &tc, Drawable theDrawable,
		     int left, int top,
		     int datapart_relative_starty,
		     int datapart_relative_height);
      // returns true iff (via oracle) we detect that things are out of date, and the
      // layout needs to be rethough and redrawn.

   // Children Pixel Calculations (excl. those in a listbox)
   int horiz_offset_to_expanded_child(const where4TreeConstants &,
                                      unsigned childIndex) const;
      // Returns the horiz pix offset (from left side of leftmost item drawn
      // below root, which is usually the listbox) of a certain expanded
      // (implicitly or explicitly) child.
      // cost is O(childindex)

   void rethink_all_expanded_children_dimensions(const where4TreeConstants &tc) {
      allExpandedChildrenWidthAsDrawn =wouldbe_all_expanded_children_width(tc);
      allExpandedChildrenHeightAsDrawn=wouldbe_all_expanded_children_height(tc);
   }

   // The following 2 routines are expensive; call only to rethink
   // cache variables "allExpandedChildrenWidth(Height)AsDrawn".
   // Each assumes all descendants have updated layout variables.
   // Costs are O(numchildren)
   int wouldbe_all_expanded_children_width(const where4TreeConstants &tc) const;
   int wouldbe_all_expanded_children_height(const where4TreeConstants &tc) const;

   // The following routines are O(1):
   int horiz_pix_everything_below_root(const where4TreeConstants &tc) const;
   int vert_pix_everything_below_root() const;
   int entire_width (const where4TreeConstants &tc) const;
   int entire_height(const where4TreeConstants &tc) const;

   bool expandEntirePath(const where4TreeConstants &tc,
                         const whereNodePosRawPath &thePath,
                         unsigned pathIndex);
      // Given a path, forcibly expand out any and all listbox path elements.
      // Exception: won't expand a listbox item at the very end of the path.
      // Returns true iff any expansion(s) actually took place.

   bool explicitlyExpandSubchild(const where4TreeConstants &tc, unsigned childindex,
				 bool force);
      // Expand a subtree out of a listbox.  Doesn't redraw.
      // Returns true iff any changes were made.
      // NOTE: The same warning of expandUnexpand1() applies -- outside code must
      //       manually rethink_expanded_children_dimensions() for ALL ancestors of
      //       the expandee; this routine only handles the immeidate ancestor (parent).
   bool explicitlyUnexpandSubchild(const where4TreeConstants &tc, unsigned childindex);
      // Unexpand a subchild (into a listbox).  Doesn't redraw.
      // Returns true iff any changes were made.
      // NOTE: The same warning of expandUnexpand1() applies -- outside code must
      //       manually rethink_expanded_children_dimensions() for ALL ancestors of
      //       the expandee; this routine only handles the immeidate ancestor (parent).
   
   bool expandAllChildren(const where4TreeConstants &tc);
   bool unExpandAllChildren(const where4TreeConstants &tc);

   bool updateAnything2Draw1(const where4TreeConstants &tc);

 public:
   where4tree(const NODEDATA &iNodeData);
  ~where4tree();

   bool updateAnything2Draw(const where4TreeConstants &tc);

   const NODEDATA &getNodeData() const {return theNodeData;}
   NODEDATA &getNodeData() {return theNodeData;}

   const pdstring &getRootName() const { return theNodeData.getName(); }

   bool alreadyExistsChildName(const pdstring &proposedNewChildName);
      // can't be const method since it'll sort if necessary

   // Adding children:
   bool addChild(where4tree *theNewChild,
		 bool explicitlyExpanded,
		 const where4TreeConstants &tc,
		 bool rethinkLayoutNow,
                 bool resortNow);
      // add a child subtree **that has been allocated with new** (not negotiable)
      // Returns true iff added (i.e., if not a duplicate).
      // NOTE: Current implementation puts child into listbox unless
      //       explicitlyExpanded.
      // NOTE: Even if you pass rethinkLayoutNow as true, we only rethink the listbox
      //       dimensions and/or the all-expanded-children dimensions, as needed.
      //       In all likelihood, you'll also need to rethink all-expanded-children
      //       dimensions for all ancestors of this node; you must do this manually.

   // Deleting children:
   bool deleteChild(where4tree *theChild,
                    const where4TreeConstants &tc,
                    bool rethinkLayoutNow,
                    bool resortNow);
      // returns true iff found (& deleted)
      // calls delete on "theChild" after removing it from the list of
      // children.
      // NOTE: Even if you pass rethinkLayoutNow as true, we only rethink the listbox
      //       dimensions and/or the all-expanded-children dimensions, as needed.
      //       In all likelihood, you'll also need to rethink all-expanded-children
      //       dimensions for all ancestors of this node; you must do this manually.


   void doneAddingChildren(const where4TreeConstants &tc, bool sortNow);
   void recursiveDoneAddingChildren(const where4TreeConstants &tc, bool sortNow);
      // Needed after having called addChild() several times for a given root and
      // had passed false as the last parameter...

   void sortChildren();
      // does not redraw

   bool draw(Tk_Window, const where4TreeConstants &tc, Drawable theDrawable,
	     int middlex, int topy, bool rootOnly, bool listboxOnly);
      // Draws root node AND the lines connecting the subtrees; children are
      // drawn via recursion.
      // Returns true iff it was detected (by the oracle, presumably) that
      // the graphical display is a bit out of date, and that a "rethink"
      // and redraw are called for.  Perhaps a resort too.

   // Resize:
   bool rethinkListboxAfterResize1(const where4TreeConstants &tc);
   bool rethinkListboxAfterResize(const where4TreeConstants &tc);
   
   void rethinkAfterResize(const where4TreeConstants &tc,
			   int horizSpaceToWorkWithThisSubtree);
      // Assuming a resize (change of tc.availableWidth, tc.availableHeight),
      // rethink whether or not there should be a listbox, and possibly
      // make internal changes.  Explicitly expanded items will stay expanded;
      // implicitly expanded items _may_ become un-expanded.

   int substring2Path(whereNodePosRawPath &thePath, const where4TreeConstants &tc,
                      const pdstring &str, where4tree *beginSearchFromPtr,
                      bool testRootNode);
      // If not found, thePath is left undefined, and 0 is returned.
      // Otherwise, modifies "thePath" and:
      //    -- returns 1 if no expansions are necessary (can scroll)
      //    -- returns 2 if expansions are necessary before scrolling

   bool fullName2Path(whereNodePosRawPath &thePath, // updated in place
                      const pdstring &fullName,
                      const where4TreeConstants &);
      // returns true iff found.  Updates thePath in place.
      // Not a const method since it can load children into memory, as needed.

   // Subtree expansion/un-expansion
   bool path2lbItemExpand(const where4TreeConstants &tc, 
                          const whereNodePosRawPath &thePath, unsigned pathIndex);
      // Given a path (ending in a listbox item), expand it, as well as any
      // ancestors, as necessary.  Returns true iff any changes.

   bool path2lbItemUnexpand(const where4TreeConstants &tc,
			    const whereNodePosRawPath &thePath, unsigned pathIndex);
      // Un-expand the (currently expanded) item at the end of the path.
      // Creates a listbox if needed.  Returns true iff any changes.

   bool path2ExpandAllChildren(const where4TreeConstants &tc,
			       const whereNodePosRawPath &thePath,
			       unsigned index);

   bool path2UnExpandAllChildren(const where4TreeConstants &tc,
				 const whereNodePosRawPath &thePath, unsigned index);

   bool path2SelectUnSelect(bool selectFlag,
                            const where4TreeConstants &tc,
                            const whereNodePosRawPath &thePath,
                            unsigned pathIndex);

   bool rigListboxScrollbarSliderTopPix(const where4TreeConstants &tc,
					int scrollBarLeft,
					int scrollBarTop,
					int scrollBarBottom,
					int newScrollBarSliderTopPix,
					bool redrawNow);
      // returns true iff any changes were made

   // The following highlight routines do not redraw:
   bool isHighlighted() const {
      return theNodeData.getHighlighted();
   }
   void highlight() {
      theNodeData.highlight();
   }
   void unhighlight() {
      theNodeData.unhighlight();
   }
   void toggle_highlight() {
      theNodeData.toggle_highlight();
   }

//     bool selectUnSelectFromFullPathName(const char *name, bool selectFlag,
//                                         where4TreeConstants &);
//        // returns true iff found.  char * is used instead of pdstring because
//        // we'll be using pointer arithmetic as we parse "name".

   pdvector<const NODEDATA *> getSelections() const;

   void recursiveClearSelections();
};

template <class NODEDATA, class childrenOracle>
bool operator<(const pdstring &str, const typename where4tree<NODEDATA, childrenOracle>::childstruct &other) {
   return !(other < str) && !(other == str);
}

#endif
