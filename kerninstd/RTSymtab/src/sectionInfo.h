// sectionInfo.h

#ifndef _SECTION_INFO_H_
#define _SECTION_INFO_H_

#include "common/h/String.h" 
#include "elftraits.h"

template<class ELFTraits>
class sectionInfo {
 private:
   typedef typename ELFTraits::ET_Scn ET_Scn;
   typedef typename ELFTraits::ET_Shdr ET_Shdr;
   typedef typename ELFTraits::ET_Data ET_Data;

   pdstring name;
   unsigned ndxInSecHdrTable;
   ET_Scn *sec;
   ET_Shdr *sec_hdr;

 public:
   sectionInfo(const char *iname, unsigned indxInSecHdrTable,
               ET_Scn *isec,
               ET_Shdr *isec_hdr) : name(iname) {
      ndxInSecHdrTable = indxInSecHdrTable;
      sec = isec;
      sec_hdr = isec_hdr;
   }

   ET_Data *getSectionData() {
      return ELFTraits::et_getdata(sec, NULL);
   }
};

#endif
