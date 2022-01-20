// power_controlFlow_headeronly.h

#ifndef _POWER_CONTROLFLOW_HEADERONLY_H_
#define _POWER_CONTROLFLOW_HEADERONLY_H_

#include "util/h/kdrvtypes.h"
#include <inttypes.h> // uint32_t
#include "power_instr.h"
class simplePath;
class kmem;
class function_t;
class parseOracle;

class power_controlFlow {
private:
     /* Declare all other methods here */

public:
   typedef uint16_t bbid_t; // XXX -- ugly
   
   static bool handleControlFlowInstr(bbid_t bb_id,
				      power_instr *instr,
				      kptr_t &instrAddr,
				        // iff returning true, we modify this
				      power_instr::power_cfi *cfi,
				      const simplePath &pathFromEntryBB,
				        // doesn't yet include curr bb
				      kptr_t stumbleOntoAddr,
				      const kmem *km,
				      parseOracle *theParseOracle);

};

#endif
