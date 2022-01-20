// whereAxisRootNode.C
// Ariel Tamches

/* $Id: rootNode.C,v 1.5 2004/04/22 21:23:26 mjbrim Exp $ */

#include <assert.h>

#include "whereAxis.h" // for some of its static vrbles / member functions
#include "rootNode.h" // change to whereAxisRootNode.h some day

Tk_Font whereAxisRootNode::theRootItemFontStruct = NULL;
Tk_Font whereAxisRootNode::theListboxItemFontStruct = NULL;
Tk_3DBorder whereAxisRootNode::rootItemTk3DBorder = NULL;
GC whereAxisRootNode::rootItemTextGC = NULL;
Tk_3DBorder  whereAxisRootNode::listboxItem3DBorder = NULL;
GC whereAxisRootNode::listboxItemGC = NULL;
GC whereAxisRootNode::listboxRayGC = NULL;
GC whereAxisRootNode::nonListboxRayGC = NULL;

// Static method:
void whereAxisRootNode::
initializeStaticsIfNeeded(const where4TreeConstants &consts) {
   if (theRootItemFontStruct == NULL)
      // somewhat kludgy
      theRootItemFontStruct = consts.rootTextFontStruct;
   
   if (theListboxItemFontStruct == NULL)
      // somewhat kludgy
      theListboxItemFontStruct = consts.listboxFontStruct;

   if (rootItemTk3DBorder == NULL)
      // somewhat kludgy
      rootItemTk3DBorder = consts.rootNodeBorder;

   if (rootItemTextGC == NULL)
      // somewhat kludgy
      rootItemTextGC = consts.rootItemTextGC;

   if (listboxRayGC == NULL)
      listboxRayGC = consts.listboxRayGC;

   if (nonListboxRayGC == NULL)
      nonListboxRayGC = consts.subchildRayGC;

   if (listboxItem3DBorder == NULL)
      // somewhat kludgy
      listboxItem3DBorder = consts.listboxBorder; // ???

   if (listboxItemGC == NULL)
      listboxItemGC = consts.listboxTextGC;
}




whereAxisRootNode::whereAxisRootNode(unsigned iUniqueId,
                                     unsigned iParentUniqueId,
                                     const pdstring &initStr) :
                                  Name(initStr) {
   uniqueId = iUniqueId;
   parentUniqueId = iParentUniqueId;
   highlighted = false;

   pixWidthAsRoot = borderPix + horizPad +
      Tk_TextWidth(theRootItemFontStruct,
                   Name.c_str(), Name.length()) +
                   horizPad + borderPix;

   Tk_FontMetrics rootItemFontMetrics; // filled in
   Tk_GetFontMetrics(theRootItemFontStruct,
                     &rootItemFontMetrics);

   pixHeightAsRoot = borderPix + vertPad +
                     rootItemFontMetrics.ascent +
                     rootItemFontMetrics.descent +
                     vertPad + borderPix;

   pixWidthAsListboxItem = Tk_TextWidth(theListboxItemFontStruct,
                                        Name.c_str(), Name.length());
}


void whereAxisRootNode::drawAsRoot(Tk_Window theTkWindow,
				   int theDrawable, // may be an offscreen pixmap
				   int root_middlex, int root_topy) const {
   // First, some quick & dirty clipping:
   const int minWindowX = 0;
   const int maxWindowX = Tk_Width(theTkWindow) - 1;
   const int minWindowY = 0;
   const int maxWindowY = Tk_Height(theTkWindow) - 1;

   if (root_topy > maxWindowY)
      return;
   if (root_topy + pixHeightAsRoot - 1 < minWindowY)
      return;

   const int boxLeft = root_middlex - (pixWidthAsRoot / 2);

   if (boxLeft > maxWindowX)
      return;
   if (boxLeft + pixWidthAsRoot - 1 < minWindowX)
      return;

   const int normalRelief = TK_RELIEF_GROOVE;
   const int highlightedRelief = TK_RELIEF_SUNKEN;

   Tk_Fill3DRectangle(theTkWindow, theDrawable,
		      rootItemTk3DBorder,
		      boxLeft, root_topy,
		      pixWidthAsRoot, pixHeightAsRoot,
		      borderPix,
		      highlighted ? highlightedRelief : normalRelief);

   // Third, draw the text
   Tk_FontMetrics rootItemFontMetrics; // filled in
   Tk_GetFontMetrics(theRootItemFontStruct,
                     &rootItemFontMetrics);
   
   const int textLeft = boxLeft + borderPix + horizPad;
   const int textBaseLine = root_topy + borderPix + vertPad +
                            rootItemFontMetrics.ascent - 1;

//     XDrawString(Tk_Display(theTkWindow), theDrawable,
//  	       rootItemTextGC,
//  	       textLeft, textBaseLine,
//  	       Name.c_str(), Name.length());

	Tk_DrawChars(Tk_Display(theTkWindow),
                     theDrawable,
                     rootItemTextGC,
                     theRootItemFontStruct,
                     Name.c_str(), Name.length(),
                     textLeft, textBaseLine );
}


GC whereAxisRootNode::
getGCforListboxRay(const whereAxisRootNode &, // parent
                   const whereAxisRootNode & // 1st child
                   ) {
   // a static member function

   return listboxRayGC;
}


GC whereAxisRootNode::
getGCforNonListboxRay(const whereAxisRootNode &, // parent
                      const whereAxisRootNode &  // 1st child
                      ) {
   // a static member function
   return nonListboxRayGC;
}

void whereAxisRootNode::prepareForDrawingListboxItems(Tk_Window theTkWindow,
						      XRectangle &listboxBounds) {
#if !defined(i386_unknown_nt4_0)
   XSetClipRectangles(Tk_Display(theTkWindow), listboxItemGC,
		     0, 0, &listboxBounds, 1, YXBanded);
  Tk_3DBorder thisStyleTk3DBorder = listboxItem3DBorder;
  XSetClipRectangles(Tk_Display(theTkWindow),
		 Tk_3DBorderGC(theTkWindow,thisStyleTk3DBorder,TK_3D_LIGHT_GC),
		     0, 0, &listboxBounds, 1, YXBanded);
  XSetClipRectangles(Tk_Display(theTkWindow),
		Tk_3DBorderGC(theTkWindow, thisStyleTk3DBorder,TK_3D_DARK_GC),
		     0, 0, &listboxBounds, 1, YXBanded);
  XSetClipRectangles(Tk_Display(theTkWindow),
		 Tk_3DBorderGC(theTkWindow, thisStyleTk3DBorder,TK_3D_FLAT_GC),
		     0, 0, &listboxBounds, 1, YXBanded);
#else // !defined(i386_unknown_nt4_0)
	// TODO - is this needed?
#endif // !defined(i386_unknown_nt4_0)
}


void whereAxisRootNode::doneDrawingListboxItems(Tk_Window theTkWindow) {
   XSetClipMask(Tk_Display(theTkWindow), listboxItemGC, None);
   Tk_3DBorder thisStyleTk3DBorder = listboxItem3DBorder;
   
   XSetClipMask(Tk_Display(theTkWindow),
		Tk_3DBorderGC(theTkWindow, thisStyleTk3DBorder,TK_3D_LIGHT_GC),
		None);
   XSetClipMask(Tk_Display(theTkWindow),
		Tk_3DBorderGC(theTkWindow, thisStyleTk3DBorder, TK_3D_DARK_GC),
		None);
   XSetClipMask(Tk_Display(theTkWindow),
		Tk_3DBorderGC(theTkWindow, thisStyleTk3DBorder, TK_3D_FLAT_GC),
		None);
}


void whereAxisRootNode::drawAsListboxItem(Tk_Window theTkWindow,
					  int theDrawable,
					  int boxLeft, int boxTop,
					  int boxWidth, int boxHeight,
					  int textLeft, int textBaseline) const {
   Tk_Fill3DRectangle(theTkWindow, theDrawable,
                      listboxItem3DBorder,
		         // for a shg-like class, this routine would take in a parameter
		         // and return a varying border.  But the where axis doesn't need
                         // such a feature.
		      boxLeft, boxTop,
		      boxWidth, boxHeight,
		      1, // 2 also looks pretty good; 3 doesn't
		      highlighted ? TK_RELIEF_SUNKEN : TK_RELIEF_RAISED);

//     XDrawString(Tk_Display(theTkWindow), theDrawable,
//                 listboxItemGC,
//  	       textLeft, // boxLeft + tc.listboxHorizPadBeforeText
//  	       textBaseline, // boxTop + tc.listboxVertPadAboveItem +
//                               // tc.listboxFontStruct->ascent - 1
//  	       Name.c_str(), Name.length());
	Tk_DrawChars(Tk_Display(theTkWindow),
                     theDrawable,
                     listboxItemGC,
                     theRootItemFontStruct,
                     Name.c_str(), Name.length(),
                     textLeft, textBaseline );
}



int whereAxisRootNode::pointWithinAsRoot(int xpix, int ypix,
					 int root_centerx, int root_topy) const {
   // return values:
   // 1 -- yes
   // 2 -- no, point is above the root
   // 3 -- no, point is below root
   // 4 -- no, point is to the left of root
   // 5 -- no, point is to the right of root
   
   assert(xpix >= 0 && ypix >= 0);
   
   if (ypix < root_topy) return 2;

   const int root_bottomy = root_topy + pixHeightAsRoot - 1;
   if (ypix > root_bottomy) return 3;

   const int root_leftx = root_centerx - pixWidthAsRoot / 2;
   if (xpix < root_leftx) return 4;

   const int root_rightx = root_leftx + pixWidthAsRoot - 1;
   if (xpix > root_rightx) return 5;

   assert(xpix >= root_leftx && xpix <= root_rightx);
   assert(ypix >= root_topy && ypix <= root_bottomy);
   return 1; // bingo
}
