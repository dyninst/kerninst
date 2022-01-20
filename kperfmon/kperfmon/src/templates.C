// templates.C

#include "kpmChildrenOracle.h"

#include "whereAxis.C"
template class whereAxis<kpmChildrenOracle>;

#include "abstractions.C"
template class abstractions<kpmChildrenOracle>;

#include "where4tree.C"
#include "rootNode.h"
template class where4tree<whereAxisRootNode, kpmChildrenOracle>;

#include "dataHeap.h"
#include "vectorHeap.C"
template class vectorHeap<sample_t>;
template class vectorHeap<dataHeap::timer16>;
template class vectorHeap<vtimer>;

#include "simpSeq.C"
template class simpSeq<unsigned>;

#include "graphicalPath.C"
template class whereNodeGraphicalPath<whereAxisRootNode, kpmChildrenOracle>;

#include "util/src/bucketer.C"
#include "util/src/foldingBucketer.C"
template class bucketer<unsigned>;
template class bucketer<unsigned long long>;
template class bucketer<double>;
template class foldingBucketer<unsigned>;
template class foldingBucketer<unsigned long long>;
template class foldingBucketer<double>;
