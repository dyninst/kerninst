// rootNode.h
// Ariel Tamches
// C++ class for the root node of subtrees declared in where4tree.h
// Basically, this file exists just to make where4tree.h that much shorter.

/* $Id: rootNode.h,v 1.9 2004/04/22 21:23:27 mjbrim Exp $ */

#ifndef _ROOTNODE_H_
#define _ROOTNODE_H_

#include "tkTools.h"
#include "common/h/String.h"

#include "where4treeConstants.h"

// This class is intended to be used as the template type of where4tree<>
// for the where axis.  For the search history graph, try some other type.
// In particular, this class contains a resourceHandle, while is not applicable
// in the searc history graph.

class whereAxisRootNode {
 private:
   unsigned uniqueId;
   unsigned parentUniqueId; // new, untested --ari 5/98

   pdstring Name;
   
   bool highlighted;

   int pixWidthAsRoot, pixHeightAsRoot;
   int pixWidthAsListboxItem;

   static const int borderPix=3;
      // both horiz & vertical [Tk_Fill3DRectangle forces these
      // to be the same]
   static const int horizPad = 3;
   static const int vertPad = 2;

   // Formerly in whereAxis; now here:
   static Tk_Font theRootItemFontStruct;
   static Tk_Font theListboxItemFontStruct;
   static Tk_3DBorder rootItemTk3DBorder;
   static GC rootItemTextGC;
   static GC listboxRayGC;
   static GC nonListboxRayGC;
   static Tk_3DBorder listboxItem3DBorder;
   static GC listboxItemGC;


 public:
   static void initializeStaticsIfNeeded(const where4TreeConstants &consts);

   whereAxisRootNode(unsigned uniqueId, unsigned parentUniqueId,
                     const pdstring &init_str);
   whereAxisRootNode(const whereAxisRootNode &src)  : Name(src.Name) {
      uniqueId = src.uniqueId;
      parentUniqueId = src.parentUniqueId;
      highlighted = src.highlighted;
      pixWidthAsRoot = src.pixWidthAsRoot;
      pixHeightAsRoot = src.pixHeightAsRoot;
      pixWidthAsListboxItem = src.pixWidthAsListboxItem;
   }
  ~whereAxisRootNode() {}

   bool anything2draw() const {return true;}
      // where axis items are never hidden

   bool operator<(const whereAxisRootNode &other) {return Name < other.Name;}
   bool operator>(const whereAxisRootNode &other) {return Name > other.Name;}
   bool operator==(const whereAxisRootNode &other) {return Name == other.Name;}
   bool operator<(const pdstring &other) {return Name < other;}
   bool operator>(const pdstring &other) {return Name > other;}
   bool operator==(const pdstring &other) {return Name == other;}

   unsigned getUniqueId() const {return uniqueId;}
   unsigned getParentUniqueId() const {return parentUniqueId;}

   const pdstring &getName() const {
      return Name;
   }

   int getWidthAsRoot()  const {return pixWidthAsRoot;}
   int getHeightAsRoot() const {return pixHeightAsRoot;}
   int getWidthAsListboxItem() const {return pixWidthAsListboxItem;}
   
   bool getHighlighted() const {return highlighted;}

   static void prepareForDrawingListboxItems(Tk_Window, XRectangle &);
   static void doneDrawingListboxItems(Tk_Window);
   void drawAsRoot(Tk_Window theTkWindow,
		   int theDrawable, // could be offscren pixmap
		   int root_middlex, int topy) const;

   static GC getGCforListboxRay(const whereAxisRootNode &parent,
				const whereAxisRootNode &firstChild);
      // return GC to be used in an XDrawLine call from "parent" down to the
      // listbox of its children; "firstChild" is the node data for the first
      // such child.
   static GC getGCforNonListboxRay(const whereAxisRootNode &parent,
				   const whereAxisRootNode &child);
      // assuming that "parent" is an expanded (explicitly or not) node, return the GC
      // to be used in an XDrawLine call from it down to "child".
   
   void drawAsListboxItem(Tk_Window theTkWindow,
			  int theDrawable, // could be offscreen pixmap
			  int boxLeft, int boxTop,
			  int boxWidth, int boxHeight,
			  int textLeft, int textBaseline) const;

   // Mouse clicks and node expansion
   int pointWithinAsRoot(int xpix, int ypix,
			 int root_centerx, int root_topy) const;
      // return values:
      // 1 -- yes
      // 2 -- no, point is north of root (or northwest or northeast)
      // 3 -- no, point is south of root (or southwest or southeast)
      // 4 -- no, point is west of root (but not north or south of root)
      // 5 -- no, point is east of root (but not north or south or root)

   // The following 3 routines don't redraw:
   void highlight() {highlighted=true;}
   void unhighlight() {highlighted=false;}
   void toggle_highlight() {highlighted = !highlighted;}
};

#endif
