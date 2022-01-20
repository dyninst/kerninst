// sparc_controlFlow_headeronly.h

#ifndef _SPARC_CONTROLFLOW_HEADERONLY_H_
#define _SPARC_CONTROLFLOW_HEADERONLY_H_

#include "util/h/kdrvtypes.h"
#include <inttypes.h> // uint32_t
#include "sparc_instr.h"

class simplePath;
class kmem;
class function_t;
class parseOracle;

typedef uint16_t bbid_t;

class sparc_controlFlow {
private:
     /* Declare all other methods here */

public:
   static bool handleControlFlowInstr(bbid_t bb_id,
				      sparc_instr *instr,
				      kptr_t &instrAddr,
				        // iff returning true, we modify this
				      sparc_instr::sparc_cfi *cfi,
				      const simplePath &pathFromEntryBB,
				        // doesn't yet include curr bb
				      kptr_t stumbleOntoAddr,
				      const kmem *km,
				      parseOracle *theParseOracle);

};

#endif
