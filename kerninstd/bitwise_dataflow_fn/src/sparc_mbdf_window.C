// sparc_mbdf_window.C
// mbdf stands for monotone bitwise dataflow function

#include "sparc_mbdf_window.h"

sparc_mbdf_window::sparc_mbdf_window(Save, const sparc_mbdf_window &src) 
  : mbdf_window(dummy) {
   thru = (regset_t *) new sparc_reg_set(sparc_reg_set::save, (sparc_reg_set &)*(src.thru), false);
   gen = (regset_t *) new sparc_reg_set(sparc_reg_set::save, (sparc_reg_set &)*(src.gen), false);
}
// g's gets src's g's; i's get src's o's; l's and o's get STOP (that's
// the meaning of the false/false 3d parameters passed to thru and gen ctors).

sparc_mbdf_window& sparc_mbdf_window::operator=(const sparc_mbdf_window &src) {
   if (&src != this) {
      *thru = *(src.thru);
      *gen = *(src.gen);
   }
   return *this;
}

void sparc_mbdf_window::processRestore(const sparc_mbdf_window &prevWindow) {
   // this's %g's get prevWindow's %g's
   // this's %o's get prevWindow's %i's
   // no change to this's %i's and %l's
   ((sparc_reg_set *)thru)->processRestore((sparc_reg_set &)*(prevWindow.thru));
   ((sparc_reg_set *)gen)->processRestore((sparc_reg_set &)*(prevWindow.gen));
}
