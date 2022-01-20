// mbdf_window.C
// mbdf stands for monotone bitwise dataflow function

#include "mbdf_window.h"

mbdf_window::Dummy mbdf_window::dummy;

#ifdef sparc_sun_solaris2_7
#include "sparc_mbdf_window.h"
#endif

mbdf_window* mbdf_window::getWindow(XDR *xdr) {
#ifdef sparc_sun_solaris2_7
   return (mbdf_window*)new sparc_mbdf_window(xdr);
#else
   return NULL;
#endif
}

mbdf_window& mbdf_window::operator=(const mbdf_window &src) {
   if (&src != this) {
      *thru = *(src.thru);
      *gen = *(src.gen);
   }
   return *this;
}

pdstring mbdf_window::sprintf(const pdstring &starts_banner,
                              const pdstring &stops_banner,
                              bool print_starts_first) const {
   pdstring result;
   result += print_starts_first ? starts_banner : stops_banner;
   result += ": ";
   //assert(0); - why was this here??
   result += (print_starts_first ? getStarts() : getStops())->disassx();
   result += "\n";

   result += print_starts_first ? stops_banner : starts_banner;
   result += ": ";
   result += (print_starts_first ? getStops() : getStarts())->disassx();
   result += "\n";

   return result;
}
