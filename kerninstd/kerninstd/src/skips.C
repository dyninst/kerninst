// skips.C

#include "common/h/Vector.h"
#include "skips.h"
#include "util/h/Dictionary.h"
#include "util/h/out_streams.h"
#include <algorithm>

class bypass_entry {
 public:
   enum what_values {cannotAnalyzeButCanParse=0, cannotAnalyzeOrParse=1};
   
 private:
   what_values what;
   pdstring modname, fnname;

 public:
   bypass_entry(what_values iwhat, const pdstring &imodname, const pdstring &ifnname) :
      what(iwhat), modname(imodname), fnname(ifnname) {
   }
   ~bypass_entry() {}
   
   bypass_entry& operator=(const bypass_entry &src) {
      what = src.what;
      modname = src.modname;
      fnname = src.fnname;
      return *this;
   }

   what_values getWhat() const {
      return what;
   }
   
   const pdstring &getModName() const {
      return modname;
   }
   const pdstring &getFnName() const {
      return fnname;
   }

   bool operator==(const std::pair<pdstring, pdstring> &cmp) const {
      return (modname == cmp.first && fnname == cmp.second);
   }
   bool operator<(const std::pair<pdstring, pdstring> &cmp) const {
      if (modname == cmp.first)
         return (fnname < cmp.second);
      return (modname < cmp.first);
   }
   bool operator>(const std::pair<pdstring, pdstring> &cmp) const {
      if (modname == cmp.first)
         return (fnname > cmp.second);
      return (modname > cmp.first);
   }
};

static pdvector<bypass_entry> bypass_kernel_syms;
   // after all insertion is done, this gets sorted

static void skip_rest_of_line(ifstream &infile) {
   // prefer infile.ignore(n, '\n') instead?  But what to pass for n?
   char c;
   do {
      infile.get(c);
   } while (!infile.eof() && c != '\n');
}

static bool parse_quoted_name_chunk(ifstream &infile, pdstring &chunk) {
   // return value:
   // false --> all done; don't parse any more
   // true --> not done; keep parsing name

   char buffer[100];

   bool result = true;

   unsigned lcv=0;
   for (; lcv < 99; lcv++) {
      if (infile.eof()) {
         result = false; // done parsing name
         break;
      }

      char c;
      infile.get(c);
      
      if (c == '"' || c=='\n') {
         result = false; // done parsing name
         infile.putback(c);
         break;
      }
      else
         buffer[lcv] = c;
   }

   buffer[lcv] = '\0';
   chunk = pdstring(buffer);
   return result;
}

static bool parse_quoted_name(ifstream &infile, pdstring &name) {
   char c;
   infile >> c;
   if (c != '"') {
      cout << "expected \" but found " << c << endl;
      return false;
   }

   bool keepparsing;
   do {
      pdstring chunk;
      keepparsing = parse_quoted_name_chunk(infile, chunk);
      name += chunk;
   } while (keepparsing);

   infile >> c;
   if (c != '"') {
      cout << "expected \" but found " << c << endl;
      return false;
   }
   else
      return true;
}

class bypass_entry_cmp {
 public:
   bool operator()(const bypass_entry &entry1, const bypass_entry &entry2) const {
      if (entry1.getModName() == entry2.getModName())
         return entry1.getFnName() < entry2.getFnName();
      else
         return entry1.getModName() < entry2.getModName();
   }
};

template <class oracle>
void parse_skips_like_file(ifstream &infile, oracle &theOracle) {
   while (!infile.eof()) {
      try {
         char c;
         infile >> c; // skips white space
         switch (c) {
            case '/': {
               infile.get(c);
               if (c != '/') {
                  cout << "expected second slash to make a comment" << endl;
                  continue;
               }
               skip_rest_of_line(infile);
               break;
            }
            case '{': {
               pdstring whatCannotBeDone;
               if (!parse_quoted_name(infile, whatCannotBeDone)) {
                  cout << "failed to parse what cannot be done [\"a\" or \"pa\"" << endl;
                  break;
               }
               infile >> c;
               if (c != ',') {
                  cout << "expected comma" << endl;
                  continue;
               }
               
               pdstring modname;
               if (!parse_quoted_name(infile, modname)) {
                  cout << "failed to parse quoted name" << endl;
                  break;
               }
               infile >> c;
               if (c != ',') {
                  cout << "expected comma" << endl;
                  continue;
               }

               pdstring fnname;
               if (!parse_quoted_name(infile, fnname)) {
                  cout << "failed to parse quoted name" << endl;
                  break;
               }
               
               infile >> c;
               if (c != '}') {
                  cout << "expected close brace" << endl;
                  continue;
               }

               infile >> c;
               if (!infile.eof() && c != ',') {
                  cout << "expected comma after close brace" << endl;
                  continue;
               }

               theOracle.process_entry(whatCannotBeDone, modname, fnname);
               break;
            }
            default:
               dout << "unknown start of command character \"" << c << "\"" << endl;
               skip_rest_of_line(infile);
               continue;
         }
      }
      catch (...) {
         cout << "caught exception while parsing skips file" << endl;
         continue;
      }
   }

   theOracle.do_when_done();
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

class skipsFileOracle {
 private:
   bool verbose;
 public:
   skipsFileOracle(bool iverbose) : verbose(iverbose) {
   }
   
   void process_entry(const pdstring &whatCannotBeDone,
                      const pdstring &modname, const pdstring &fnname) {
      if (verbose) {
         cout << "adding skips entry: what=\"" << whatCannotBeDone << "\""
              << " mod=\"" << modname << "\" fn=\"" << fnname << "\""
              << endl;
      }
      
      bypass_entry::what_values what;
      
      static const pdstring whatCannotBeDone_analyze = "a";
      static const pdstring whatCannotBeDone_parseOrAnalyze = "pa";

      if (whatCannotBeDone == whatCannotBeDone_analyze)
         what = bypass_entry::cannotAnalyzeButCanParse;
      else if (whatCannotBeDone == whatCannotBeDone_parseOrAnalyze)
         what = bypass_entry::cannotAnalyzeOrParse;
      else {
         cout << "Cannot process skips entry: unknown 'what' field: "
              << whatCannotBeDone << endl;
         return;
      }

      bypass_kernel_syms += bypass_entry(what, modname, fnname);
   }
   void do_when_done() {
      // Now, sort bypass_kernel_syms! (For fast future searching)
      std::sort(bypass_kernel_syms.begin(), bypass_kernel_syms.end(), bypass_entry_cmp());
   }
};

void parse_skips(ifstream &infile, bool verbose) {
   skipsFileOracle theOracle(verbose);
   parse_skips_like_file(infile, theOracle);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

bool doublestring_cmp(const std::pair<pdstring,pdstring> &first,
                      const std::pair<pdstring,pdstring> &second) {
   if (first.first < second.first)
      return true;
   
   if (first.first == second.first)
      return first.second < second.second;
   
   return false;
}

extern pdvector< std::pair<pdstring,pdstring> > functionNamesThatReturnForTheCaller; // driver.C

class returningFunctionsOracle {
 private:
   bool verbose;
   
 public:
   returningFunctionsOracle(bool iverbose) : verbose(iverbose) {
   }
   
   void process_entry(const pdstring & /* what */,
                      const pdstring &modname, const pdstring &fnname) {
      if (verbose) {
         cout << "Parsed returning-for-caller fns file entry: "
              << modname << '/' << fnname << endl;
      }
      
      functionNamesThatReturnForTheCaller += std::make_pair(modname, fnname);
   }

   void do_when_done() {
      std::sort(functionNamesThatReturnForTheCaller.begin(),
                functionNamesThatReturnForTheCaller.end(),
                doublestring_cmp);
   }
};

void parse_functions_that_return_for_caller(ifstream &infile, bool verbose) {
   returningFunctionsOracle theOracle(verbose);
   parse_skips_like_file(infile, theOracle);
}

// ----------------------------------------------------------------------

extern pdvector<oneFnWithKnownNumLevelsByName> functionsWithKnownNumLevelsByName;

class functionsWithKnownNumLevelsByName_cmp {
 public:
   bool operator()(const oneFnWithKnownNumLevelsByName &first,
                   const oneFnWithKnownNumLevelsByName &second) const {
      // sort by modName then fnname
      if (first.modName < second.modName)
         // first is less, due to modName
         return true;
      else if (second.modName < first.modName)
         // second is less, due to modName
         return false;

      // modName is a tie; check fnName
      return (first.fnName < second.fnName);
   }
};

class functionsWithKnownNumLevelsOracle {
 private:
   bool verbose;

 public:
   functionsWithKnownNumLevelsOracle(bool b) : verbose(b) {
   }
   void process_entry(const pdstring &what, const pdstring &modName, const pdstring &fnname) {
      static const pdstring str_one("1");
      static const pdstring str_two("2");
      static const pdstring str_three("3");

      static const pdstring str_negone("-1");
      static const pdstring str_negtwo("-2");
      static const pdstring str_negthree("-3");

      int num = 0;
      
      if (what == str_one) num = 1;
      else if (what == str_two) num = 2;
      else if (what == str_three) num = 3;
      else if (what == str_negone) num = -1;
      else if (what == str_negtwo) num = -2;
      else if (what == str_negthree) num = -3;
      else {
         cout << "Ignoring unknown value in functions with known levels file: \""
              << what << "\"" << endl;
         return;
      }

      oneFnWithKnownNumLevelsByName s(modName, fnname, num);

      functionsWithKnownNumLevelsByName += s;
   }
   void do_when_done() {
      std::sort(functionsWithKnownNumLevelsByName.begin(),
                functionsWithKnownNumLevelsByName.end(),
                functionsWithKnownNumLevelsByName_cmp());
   }
};


void parse_functions_with_known_numlevels(ifstream &infile, bool verbose) {
   functionsWithKnownNumLevelsOracle theOracle(verbose);
   parse_skips_like_file(infile, theOracle);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static bool bypass_parsing(const pdstring &sym_module, const pdstring &sym_name,
                           const pdvector<bypass_entry> &skips) {
   // assumes that it's sorted!

   const std::pair<pdstring, pdstring> searchee(sym_module, sym_name);
   pdvector<bypass_entry>::const_iterator iter = std::lower_bound(skips.begin(), skips.end(),
                                                                searchee);

   if (iter == skips.end())
      return false;

   if (*iter > searchee)
      return false;
   else if (*iter < searchee)
      assert(false);

   assert(iter->getModName() == sym_module && iter->getFnName() == sym_name);
   if (iter->getWhat() == bypass_entry::cannotAnalyzeButCanParse)
      return false;
   else {
      assert(iter->getWhat() == bypass_entry::cannotAnalyzeOrParse);
      return true;
   }

//   return binary_search(skips.begin(), skips.end(), searchee);
}

bool bypass_parsing(const pdstring &mod_name,
                    const StaticString &primary_name,
                    const StaticString *aliases, unsigned num_aliases) {
   if (bypass_parsing(mod_name, primary_name.c_str(), bypass_kernel_syms))
      return true;

   for (unsigned lcv=0; lcv < num_aliases; lcv++)
      if (bypass_parsing(mod_name, aliases[lcv].c_str(), bypass_kernel_syms))
	 return true;

   return false;
}

static bool bypass_analysis(const pdstring &mod_name, const pdstring &fn_name,
                            const pdvector<bypass_entry> &skips) {
   // assumes that it's sorted!

   const std::pair<pdstring, pdstring> searchee(mod_name, fn_name);
   pdvector<bypass_entry>::const_iterator iter = std::lower_bound(skips.begin(),
                                                                skips.end(),
                                                                searchee);

   if (iter == skips.end())
      return false;

   if (*iter > searchee)
      return false;
   else if (*iter < searchee)
      assert(false);

   assert(iter->getModName() == mod_name && iter->getFnName() == fn_name);
   if (iter->getWhat() == bypass_entry::cannotAnalyzeButCanParse)
      return true;
   else {
      assert(iter->getWhat() == bypass_entry::cannotAnalyzeOrParse);
      return true;
   }
}

bool bypass_analysis(const pdstring &mod_name,
                     const StaticString &primary_name,
                     const StaticString *aliases, unsigned num_aliases) {
   if (bypass_analysis(mod_name, primary_name.c_str(), bypass_kernel_syms))
      return true;
   
   for (unsigned lcv=0; lcv < num_aliases; lcv++)
      if (bypass_analysis(mod_name, aliases[lcv].c_str(), bypass_kernel_syms))
	 return true;

   return false;
}

