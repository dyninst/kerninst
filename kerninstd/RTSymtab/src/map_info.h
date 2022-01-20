// map_info.h
// May just be temporary.

#ifndef _MAP_INFO_H_
#define _MAP_INFO_H_

#include "util/h/Dictionary.h"
#include "common/h/String.h"
#include "sectionInfo.h"
#include "util/h/hashFns.h"

template<class ELFTraits>
class map_info {
 private:
   typedef sectionInfo<ELFTraits> secInfo;
   dictionary_hash<pdstring, secInfo> sections;

 public:
   typedef dictionary_hash<pdstring, secInfo>::const_iterator const_iterator;

   map_info() : sections(stringHash) {
   }

   void add_section(const pdstring &secName, const secInfo &info) {
      sections.set(secName, info);
   }
   
   const_iterator begin() {
      return sections.begin();
   }
   
   const_iterator end() {
      return sections.end();
   }
   
   bool sectionNameExists(const pdstring &r) const {
      return sections.defines(r);
   }

   const secInfo &getSectionInfoByName(const pdstring &r) const {
      return sections.get(r);
   }
   
   secInfo &getSectionInfoByName(const pdstring &r) {
      return sections.get(r);
   }
};

#endif
