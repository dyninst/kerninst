// templates.C

// commented out everything, since this file not compiled

//  #include "common/h/Vector.h"
//  #include "util/h/refCounterK.h"
//  #include "common/h/String.h"
//  #include "util/src/HomogenousHeap.C"

//  template class refCounterK<string_ll>;
//  template class HomogenousHeap<refCounterK<string_ll>::ss, 1>;

//  #include "bitwise_dataflow_fn.h"
//  template class vector<mbdf_window>;

//  #include "sparc_reg.h"
//  template class vector<sparc_reg>;

//  #include "util/src/DictionaryLite.C"
//  #include "sparc_bb.h"
//  template class dictionary_lite<unsigned long, sparc_bb*>;
//  template class vector< vector< dictionary_lite<unsigned long, sparc_bb*>::hash_pair > >;
//  template class vector< dictionary_lite<unsigned long, sparc_bb*>::hash_pair >;
//  template class vector<unsigned>;
//  template class vector<sparc_bb*>;

//  #include "sparc_fn.h"
//  template class vector<sparc_fn::liveRegStuffPerBB>;
//  template class vector<sparc_fn::stuffPerBB>;

//  template class refCounterK<sparc_fn::stuffPerBB_ll>;
//  template class HomogenousHeap<refCounterK<sparc_fn::stuffPerBB_ll>::ss, 1>;
//  HomogenousHeap<refCounterK<sparc_fn::stuffPerBB_ll>::ss, 1> *refCounterK<sparc_fn::stuffPerBB_ll>::ssHeap;

//  template class refCounterK<monotone_bitwise_dataflow_fn_ll>;
//  template class HomogenousHeap<refCounterK<monotone_bitwise_dataflow_fn_ll>::ss, 1>;

//  #include "util/src/SkipList.C"
//  //#include "fnRegistry.h"
//  template class SkipList<unsigned long, sparc_fn*, 16>;
//  template class SkipListNode<unsigned long, sparc_fn*, 16>;
//  template class HomogenousHeap< SkipListNode<unsigned long, sparc_fn*, 16>, 1>;

//  template class vector<bool>;



//  template class vector<sparc_instr>;
//  template class vector< vector<unsigned> >;

//  template class SkipList<orderedbb::KeyType, sparc_bb*, 16>;
//  template class SkipListNode<orderedbb::KeyType, sparc_bb*, 16>;

//  template class HomogenousHeap< SkipListNode<orderedbb::KeyType, sparc_bb*, 16>, 1 >;

//  template class HomogenousHeap<sparc_bb*, 1>;
//  template class HomogenousHeap<sparc_bb*, 2>;
//  template class HomogenousHeap<sparc_bb*, 3>;

//  template class HomogenousHeap<mbdf_window_ll, 1>;

//  template class HomogenousHeap<mbdf_window, 1>;
//  template class HomogenousHeap<mbdf_window, 2>;

//  template class HomogenousHeap<sparc_instr, 2>;
//  template class HomogenousHeap<sparc_instr, 4>;
//  template class HomogenousHeap<sparc_instr, 8>;

//  template class HomogenousHeap<unsigned, 1>; // for ref counter

//  #include "window.h"
//  template class refCounterK<mbdf_window_ll>;
//  template class HomogenousHeap<refCounterK<mbdf_window_ll>::ss, 1>;
//  HomogenousHeap<refCounterK<mbdf_window_ll>::ss, 1> *refCounterK<mbdf_window_ll>::ssHeap;

//  #include "uimm.C"
//  template class uimmediate<1>;
//  template class uimmediate<2>;
//  template class uimmediate<3>;
//  template class uimmediate<4>;
//  template class uimmediate<5>;
//  template class uimmediate<6>;
//  template class uimmediate<8>;
//  template class uimmediate<13>;
//  template class uimmediate<22>;

//  #include "simm.C"
//  template class simmediate<13>;
//  template class simmediate<22>;
//  template class simmediate<30>;

//  template void rollin(unsigned long&, uimmediate<1>);
//  template void rollin(unsigned long&, uimmediate<2>);
//  template void rollin(unsigned long&, uimmediate<3>);
//  template void rollin(unsigned long&, uimmediate<4>);
//  template void rollin(unsigned long&, uimmediate<5>);
//  template void rollin(unsigned long&, uimmediate<6>);
//  template void rollin(unsigned long&, uimmediate<8>);
//  template void rollin(unsigned long&, uimmediate<13>);
//  template void rollin(unsigned long&, uimmediate<22>);

//  template void rollin(unsigned long&, simmediate<13>);
//  template void rollin(unsigned long&, simmediate<22>);
//  template void rollin(unsigned long&, simmediate<30>);

//  template unsigned long& operator|=(unsigned long&, uimmediate<1>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<2>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<3>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<4>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<5>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<6>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<8>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<13>);
//  template unsigned long& operator|=(unsigned long&, uimmediate<22>);

//  template unsigned long& operator|=(unsigned long&, simmediate<13>);
//  template unsigned long& operator|=(unsigned long&, simmediate<22>);
//  template unsigned long& operator|=(unsigned long&, simmediate<30>);

//  #include "minmax.C"
//  template void ipmin(unsigned &, unsigned);

