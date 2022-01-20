#include "basicblock.h"
#include "funkshun.h"
#include "kapi.h"

kapi_basic_block::kapi_basic_block() : theBasicBlock(0), theFunction(0)
{
}

// Not for public use: construct the kapi object from basicblock_t
kapi_basic_block::kapi_basic_block(const basicblock_t *bb,
				   const function_t *func) : 
    theBasicBlock(bb), theFunction(func)
{
}

// Return the starting address
kptr_t kapi_basic_block::getStartAddr() const
{
    return theBasicBlock->getStartAddr();
}

// Return the exit address of the block. The definition of this address is
// a bit tricky due to the number of various ways a basic block
// can end. But basically you need to insertSnippet at that address
// to catch the exit from the block.
kptr_t kapi_basic_block::getExitAddr() const
{
    return theBasicBlock->getExitPoint(theFunction->getOrigCode());
}

// Return the address after the last address in the block
kptr_t kapi_basic_block::getEndAddrPlus1() const
{
    return theBasicBlock->getEndAddrPlus1();
}
