// skips.h

#ifndef _SKIPS_H_
#define _SKIPS_H_

#include <fstream.h> // ifstream

#include "common/h/String.h"

#include "util/h/StaticString.h"

bool bypass_parsing(const pdstring &modname,
                    const StaticString &primary_name,
                    const StaticString *aliases, unsigned num_aliases);

bool bypass_analysis(const pdstring &modname,
                     const StaticString &primary_name,
                     const StaticString *aliases, unsigned num_aliases);
                     
void parse_skips(ifstream &, bool verbose);

void parse_functions_that_return_for_caller(ifstream &, bool verbose);

struct oneFnWithKnownNumLevelsByName {
   pdstring modName;
   pdstring fnName;
   int numLevels;
   // negative --> that many pre save frame manufactured (after abs)
   // 0 --> not allowed
   // positive --> that many levels, zero pre-save frames manufactured

   oneFnWithKnownNumLevelsByName(const pdstring &imodName, const pdstring &ifnName,
                                 unsigned inumLevels) :
      modName(imodName), fnName(ifnName), numLevels(inumLevels) {
   }
   void operator=(const oneFnWithKnownNumLevelsByName &src) {
      if (this != &src) {
         modName = src.modName;
         fnName = src.fnName;
         numLevels = src.numLevels;
      }
   }
};

void parse_functions_with_known_numlevels(ifstream &, bool verbose);

#endif
