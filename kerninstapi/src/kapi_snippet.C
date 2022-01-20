#include <string.h>
#include "tempBufferEmitter.h"
#include "kapi.h"
#include "ast.h"
#include "kludges.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_instr.h"
#include "regSetManager.h"
#include "vtimerMgr.h"
extern vtimerMgr *theGlobalVTimerMgr;
#endif

using namespace kcodegen;

kapi_snippet::kapi_snippet() : node(0), type(kapi_snippet_regular)
{
}

kapi_snippet::kapi_snippet(const kapi_snippet &src)
{
    node = assignAst(src.node);
    type = src.type;
}

kapi_snippet::~kapi_snippet()
{
    if (node != 0) { // can any snippet type have node == 0 ?
	removeAst(node);
    }
}

kapi_snippet& kapi_snippet::operator=(const kapi_snippet &src)
{
    if (node != 0) { // can any snippet type have node == 0 ?
	removeAst(node);
    }
    node = assignAst(src.node);
    type = src.type;
    return *this;
}

AstNode* kapi_snippet::getAst() const
{
    assert(node != 0);
    return node;
}

kapi_snippet_type kapi_snippet::getType() const
{
    return type;
}

kapi_arith_expr::kapi_arith_expr(kapi_arith_operation kind, 
				 const kapi_snippet &lOpd, 
				 const kapi_snippet &rOpd)
{
    opCode op;
    switch(kind) {
    case kapi_atomic_assign:
	op = storeAtomicOp;
	break;
    case kapi_assign:
	op = storeOp;
	break;
    case kapi_plus:
	op = plusOp;
	break;
    case kapi_minus:
	op = minusOp;
	break;
    case kapi_times:
	op = timesOp;
	break;
    case kapi_divide:
	op = divOp;
	break;
    case kapi_bit_and:
	op = andOp;
	break;
    case kapi_bit_or:
	op = orOp;
	break;
    case kapi_shift_left:
	op = shlOp;
	break;
    case kapi_shift_right:
	op = shrOp;
	break;
    default:
	assert(false && "Unknown arithmetic operation"); // FIXME !
    }
    node = new AstNode(op, lOpd.getAst(), rOpd.getAst());
}

kapi_arith_expr::kapi_arith_expr(kapi_unary_operation kind, 
				 const kapi_snippet &opd)
{
    AstNode::operandType op;
    switch(kind) {
    case kapi_deref:
	op = AstNode::DataIndir;
	break;
    default:
	assert(false && "Unknown unary operation"); // FIXME !
    }
    node = new AstNode(op, opd.getAst());
}

kapi_const_expr::kapi_const_expr(kapi_int_t val)
{
    node = new AstNode(AstNode::Constant, val);  
}

kapi_int_variable::kapi_int_variable(kptr_t addr) 
{
    AstNode *addrNode = new AstNode(AstNode::Constant, addr);
    node = new AstNode(AstNode::DataIndir, addrNode);
}

kapi_int_variable::kapi_int_variable(kptr_t baseAddr,
				     const kapi_snippet &index) 
{
    AstNode *baseNode = new AstNode(AstNode::Constant, baseAddr);
    AstNode *indexNode = index.getAst();
    AstNode *addrNode = new AstNode(plusOp, baseNode, indexNode);
    node = new AstNode(AstNode::DataIndir, addrNode);
}

// The most general case: scalar int variable stored at the
// address specified by the given expression
kapi_int_variable::kapi_int_variable(const kapi_snippet &addrExpr)
{
    AstNode *addrNode = addrExpr.getAst();
    node = new AstNode(AstNode::DataIndir, addrNode);
}

#ifdef i386_unknown_linux2_4
kapi_int64_variable::kapi_int64_variable(kptr_t addr)
{
    AstNode *addrNode = new AstNode(AstNode::Constant, addr);
    node = new AstNode(AstNode::DataIndir64, addrNode);
}

kapi_int64_variable::kapi_int64_variable(kptr_t baseAddr,
                                         const kapi_snippet &index) 
{
    AstNode *baseNode = new AstNode(AstNode::Constant, baseAddr);
    AstNode *indexNode = index.getAst();
    AstNode *addrNode = new AstNode(plusOp, baseNode, indexNode);
    node = new AstNode(AstNode::DataIndir64, addrNode);
}

// The most general case: scalar int variable stored at the
// address specified by the given expression
kapi_int64_variable::kapi_int64_variable(const kapi_snippet &addrExpr) 
{
    AstNode *addrNode = addrExpr.getAst();
    node = new AstNode(AstNode::DataIndir64, addrNode);
}
#endif

// Destination of a gotoExpr
kapi_label_expr::kapi_label_expr(char *label)
{
    name = strdup(label);
    node = new AstNode(AstNode::labelNode, name);
}

// Destination of a gotoExpr
kapi_label_expr::~kapi_label_expr()
{
    free(name); // was strdup'ed
}

const char* kapi_label_expr::getName() const
{
    return name;
}

kapi_goto_expr::kapi_goto_expr(const kapi_label_expr &label) 
{
    const char *name = label.getName();
    node = new AstNode(AstNode::gotoNode, name);
}

kapi_sequence_expr::kapi_sequence_expr(const kapi_vector<kapi_snippet> &exprs)
{
    node = 0;
    kapi_vector<kapi_snippet>::const_iterator iter = exprs.begin();
    for (; iter != exprs.end(); iter++) {
	if (node == 0) {
	    node = assignAst(iter->getAst()); 
	}
	else {
	    AstNode *tempAst = new AstNode(node, iter->getAst());
	    removeAst(node);
	    node = tempAst;
	}
    }
}

kapi_bool_expr::kapi_bool_expr(bool value)
{
    unsigned intval = (value ? 1 : 0);
    node = new AstNode(AstNode::Constant, intval);
}

kapi_bool_expr::kapi_bool_expr(kapi_relation kind, const kapi_snippet &lOpd, 
			       const kapi_snippet &rOpd)
{
    opCode astOp;
    switch(kind) {
    case kapi_lt:
        astOp = lessOp;
        break;
    case kapi_eq:
        astOp = eqOp;
        break;
    case kapi_gt:
        astOp = greaterOp;
        break;
    case kapi_le:
        astOp = leOp;
        break;
    case kapi_ne:
        astOp = neOp;
        break;
    case kapi_ge:
        astOp = geOp;
        break;
    case kapi_log_and:
        astOp = andOp;
        break;
    case kapi_log_or:
        astOp = orOp;
        break;
    default:
	assert(false && "Unknown boolean operation"); // FIXME !
    };
    node = new AstNode(astOp, lOpd.getAst(), rOpd.getAst());
}

kapi_null_expr::kapi_null_expr()
{
    node = new AstNode();
}

kapi_if_expr::kapi_if_expr(const kapi_bool_expr &cond, 
			   const kapi_snippet &trueClause)
{
    node = new AstNode(ifOp, cond.getAst(), trueClause.getAst());
}

kapi_if_expr::kapi_if_expr(const kapi_bool_expr &cond, 
			   const kapi_snippet &trueClause, 
			   const kapi_snippet &falseClause)
{
    node = new AstNode(ifOp, cond.getAst(), 
		       trueClause.getAst(), falseClause.getAst());
}

kapi_cpuid_expr::kapi_cpuid_expr()
{
#ifdef sparc_sun_solaris2_7
    node = new AstNode(AstNode::CPUId, (int64_t)0 /* unused */);
#elif defined(i386_unknown_linux2_4)
    node = new AstNode(AstNode::CPUId, (int32_t)0 /* unused */);
#elif defined(ppc64_unknown_linux2_4)
    node = new AstNode(AstNode::CPUId, (int64_t)0 /* unused */);
#endif
}

// Expression which evaluates to the process id of an invoking process
kapi_pid_expr::kapi_pid_expr()
{
#ifdef sparc_sun_solaris2_7
    node = new AstNode(AstNode::ProcessId, (int64_t)0 /* unused */);
#elif defined(i386_unknown_linux2_4)
    node = new AstNode(AstNode::ProcessId, (int32_t)0 /* unused */);
#elif defined(ppc64_unknown_linux2_4)
    node = new AstNode(AstNode::ProcessId, (int64_t)0 /* unused */);
#endif
}

#ifdef sparc_sun_solaris2_7

// Expression which evaluates to the current interrupt priority level (PIL)
kapi_getpil_expr::kapi_getpil_expr()
{
    node = new AstNode(getPIL, (int64_t)0 /* unused */);
}

// Expression useful for its side effects: sets PIL to the given value
kapi_setpil_expr::kapi_setpil_expr(const kapi_snippet &value)
{
    node = new AstNode(setPIL, value.getAst());
}

#elif defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)

// Expression which clears maskable interrupt flag
kapi_disableint_expr::kapi_disableint_expr()
{
    node = new AstNode(disableLocalInterrupts);
} 

// Expression which sets maskable interrupt flag
kapi_enableint_expr::kapi_enableint_expr()
{
    node = new AstNode(enableLocalInterrupts); 
}

#endif

kapi_int_vector::kapi_int_vector(kptr_t iaddr, unsigned icount) :
    addr(iaddr), count(icount)
{
}

kptr_t kapi_int_vector::getBaseAddress() const 
{
    return addr;
}

kapi_hwcounter_expr::kapi_hwcounter_expr(kapi_hwcounter_kind type)
{
    node = new AstNode(AstNode::HwCounterReg, type);
}

void kapi_hwcounter_expr::resetType(kapi_hwcounter_kind type)
{
    AstNode *tempAst = new AstNode(AstNode::HwCounterReg, type);
    removeAst(node);
    node = tempAst;
}

kapi_param_expr::kapi_param_expr(unsigned n)
{
    node = new AstNode(AstNode::Param, n);
}

kapi_retval_expr::kapi_retval_expr(const kapi_function &func)
{
    kptr_t entryAddr = func.getEntryAddr();
    node = new AstNode(AstNode::ReturnVal, entryAddr);
}

kapi_call_expr::kapi_call_expr(kptr_t entryAddr,
			       const kapi_vector<kapi_snippet> &args)
{
    pdvector<AstNode *> ast_args;
    kapi_vector<kapi_snippet>::const_iterator iter = args.begin();
    for (; iter != args.end(); iter++) {
	ast_args.push_back(iter->getAst());
    }

    function_base fb(entryAddr, "invalid");
    node = new AstNode(&fb, ast_args);
}

kapi_vtimer_cswitch_expr::kapi_vtimer_cswitch_expr(kapi_point_location ptype,
						   kptr_t all_vtimers)
{
    switch(ptype) {
    case kapi_cswitch_in:
	type = kapi_snippet_vtimer_in;
	break;
    case kapi_cswitch_out:
	type = kapi_snippet_vtimer_out;
	break;
    default:
	assert(false);
    }
    all_vtimers_addr = all_vtimers;
    node = 0; // To catch when someone calls .generateCode on it
}

kptr_t kapi_vtimer_cswitch_expr::getAllVtimersAddr() const
{
    return all_vtimers_addr;
}

#ifdef sparc_sun_solaris2_7
// vrestart routines expect the following expressions to
// be available
kapi_snippet kapi_vtimer_cswitch_expr::getVRestartAddrExpr() const
{
    assert(type == kapi_snippet_vtimer_in);
    return kapi_raw_register_expr(theGlobalVTimerMgr->getVRestartAddrReg());
}

kapi_snippet kapi_vtimer_cswitch_expr::getAuxdataExpr() const
{
    assert(type == kapi_snippet_vtimer_in);
    return kapi_raw_register_expr(theGlobalVTimerMgr->getAuxdataReg());
}

// vstop routines expect the following expressions to
// be available
kapi_snippet kapi_vtimer_cswitch_expr::getVStopAddrExpr() const
{
    assert(type == kapi_snippet_vtimer_out);
    return kapi_raw_register_expr(theGlobalVTimerMgr->getVStopAddrReg());
}

kapi_snippet kapi_vtimer_cswitch_expr::getStartExpr() const
{
    assert(type == kapi_snippet_vtimer_out);
    return kapi_raw_register_expr(theGlobalVTimerMgr->getStartReg());
}

kapi_snippet kapi_vtimer_cswitch_expr::getIterExpr() const
{
    assert(type == kapi_snippet_vtimer_out);
    return kapi_raw_register_expr(theGlobalVTimerMgr->getIterReg());
}
#endif

kapi_ret_expr::kapi_ret_expr()
{
    node = new AstNode(retOp);
}

kapi_raw_register_expr::kapi_raw_register_expr(int regNum)
{
    node = new AstNode(AstNode::DataReg, regNum);
}

