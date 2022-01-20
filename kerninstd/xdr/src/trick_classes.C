// trick_classes.C

#include "trick_classes.h"

//---------------------

trick_mbdf::trick_mbdf(const trick_mbdf &tm) : fn(tm.get()) {}
   // OK to do pointer copy for trick class, since it doesn't use/delete it

trick_mbdf::trick_mbdf(const monotone_bitwise_dataflow_fn *ifn) : fn(ifn) {}
   // OK to do pointer copy for trick class, since it doesn't use/delete it

trick_mbdf::~trick_mbdf() {}

const monotone_bitwise_dataflow_fn* trick_mbdf::get() const { return fn; }

//---------------------

trick_fn::trick_fn(const function_t *ifn) : fn(ifn) {}
   // OK to do pointer copy for trick class, since it doesn't use/delete it

trick_fn::~trick_fn() {}

const function_t* trick_fn::get() const { return fn; }

//---------------------

trick_regset::trick_regset() : rs(0) {}

trick_regset::trick_regset(const trick_regset &tr) : rs(tr.get()) {}
   // OK to do pointer copy for trick class, since it doesn't use/delete it

trick_regset::trick_regset(const regset_t *irs) : rs(irs) {}
   // OK to do pointer copy for trick class, since it doesn't use/delete it

trick_regset::~trick_regset() {}

const regset_t* trick_regset::get() const { return rs; }

//---------------------

trick_relocatableCode::trick_relocatableCode(const trick_relocatableCode &tr) 
   : rc(tr.get()) {}
   // OK to do pointer copy for trick class, since it doesn't use/delete it

trick_relocatableCode::trick_relocatableCode(const relocatableCode_t *irc) 
   : rc(irc) {}
   // OK to do pointer copy for trick class, since it doesn't use/delete it

trick_relocatableCode::~trick_relocatableCode() {}

const relocatableCode_t* trick_relocatableCode::get() const { return rc; }
