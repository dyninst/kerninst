// kpmChildrenOracle.h

#ifndef _KPM_CHILDREN_ORACLE_H_
#define _KPM_CHILDREN_ORACLE_H_

#include "rootNode.h"
#include "where4tree.h"

class kpmChildrenOracle {
 public:
   static bool hasChildren(unsigned id);
   static unsigned numChildren(unsigned id);
   
   static bool addChildrenNow(where4tree<whereAxisRootNode, kpmChildrenOracle> *parent,
                              const where4TreeConstants &);
};

#endif
