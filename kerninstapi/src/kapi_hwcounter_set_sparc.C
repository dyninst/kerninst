#include "instrumenter.h"
#include "kapi.h"

#define KIND_OF(t) (t & 0x00007fff) // redefined instead of including vtimer.h

typedef struct {
    kapi_hwcounter_kind kind; // For assertion checking
    unsigned pcrval; // Value to write into %pic
    int picnum; // 0 for %pic0, 1 for %pic1, 2 for any pic, -1 for no pic
} hwcmap_t;

static const int any_pic= 2;
static const int no_pic = -1;
static const int numMapEntries = 14; // sync-up with id2state !
static const hwcmap_t id2state[] = {{HWC_NONE, 0, no_pic},
				    {HWC_TICKS, 0, no_pic},
				    {HWC_DCACHE_VREADS, 0x9, 0},
				    {HWC_DCACHE_VWRITES, 0xa, 0},
				    {HWC_DCACHE_VREAD_HITS, 0x9, 1},
				    {HWC_DCACHE_VWRITE_HITS, 0xa, 1},
				    {HWC_ICACHE_VREFS, 0x8, 0},
				    {HWC_ICACHE_VHITS, 0x8, 1},
				    {HWC_ICACHE_STALL_CYCLES, 0x2, 0},
				    {HWC_BRANCH_MISPRED_VSTALL_CYCLES, 0x2, 1},
				    {HWC_ECACHE_VREFS, 0xc, 0},
				    {HWC_ECACHE_VHITS, 0xc, 1},
				    {HWC_ECACHE_VREAD_HITS, 0xf, 0},
				    {HWC_VINSNS, 0x1, any_pic}};

bool usesTick(kapi_hwcounter_kind kind)
{
    int real_kind = KIND_OF(kind);
    assert(real_kind > 0 && 
           real_kind < numMapEntries && 
           id2state[real_kind].kind == real_kind);
    return (real_kind == HWC_TICKS);
}

bool usesPic(kapi_hwcounter_kind kind, int num)
{
    int real_kind = KIND_OF(kind);
    
    if (id2state[real_kind].picnum == any_pic) {
	// This event can use this pic, but does it in practice? Read %pcr.
	extern instrumenter *theGlobalInstrumenter;
	assert(theGlobalInstrumenter != 0);
        assert(real_kind > 0 && 
               real_kind < numMapEntries && 
               id2state[real_kind].kind == real_kind);
	pcrData pcr = theGlobalInstrumenter->getPcr();
	return ((num == 0 && pcr.s0 == id2state[real_kind].pcrval) ||
		(num == 1 && pcr.s1 == id2state[real_kind].pcrval));
    }
    else {
	return (id2state[real_kind].picnum == num);
    }
}

// Create an empty counter set
kapi_hwcounter_set::kapi_hwcounter_set()
{
    for (unsigned i=0; i<numCounters; i++) {
	state[i] = HWC_NONE;
    }
}
    
// Insert a counter in the set. If its slot is already occupied,
// force our counter in this slot. Return the old value of the slot
kapi_hwcounter_kind kapi_hwcounter_set::insert(kapi_hwcounter_kind kind)
{
    int real_kind = KIND_OF(kind);

    assert(real_kind > 0 && 
           real_kind < numMapEntries && 
           id2state[real_kind].kind == real_kind);

    int picnum = id2state[real_kind].picnum;
    if (picnum == no_pic) {
	// This hwcounter does not modify %pcr
	return HWC_NONE;
    }
    else if (picnum == any_pic) {
	// Try to find an unused slot
	picnum = 0;
	for (unsigned j=0; j<numCounters; j++) {
	    if (state[j] == HWC_NONE) {
		picnum = j;
	    }
	}
    }
    kapi_hwcounter_kind rv = state[picnum];
    state[picnum] = (kapi_hwcounter_kind)real_kind;
    return rv;
}

// Check to see if the given counter can be enabled with no conflicts
bool kapi_hwcounter_set::conflictsWith(kapi_hwcounter_kind kind) const
{
    int real_kind = KIND_OF(kind);

    assert(real_kind > 0 && 
           real_kind < numMapEntries && 
           id2state[real_kind].kind == real_kind);

    int picnum = id2state[real_kind].picnum;
    if (picnum == no_pic) {
	// This hwcounter does not modify %pcr
	return false;
    }
    else if (picnum == any_pic) {
	// Try to find an unused slot
	picnum = 0;
	for (unsigned j=0; j<numCounters; j++) {
	    if (state[j] == HWC_NONE || state[j] == real_kind) {
		picnum = j;
	    }
	}
    }
    return (state[picnum] != HWC_NONE && state[picnum] != real_kind);
}

// Check to see if two sets are equal
bool kapi_hwcounter_set::equals(const kapi_hwcounter_set &hwset) const
{
    for (unsigned i=0; i<numCounters; i++) {
	if (state[i] != hwset.state[i]) {
	    return false;
	}
    }
    return true;
}

// Free a slot occupied by the counter
void kapi_hwcounter_set::free(kapi_hwcounter_kind kind)
{
    int real_kind = KIND_OF(kind);

    for (unsigned i=0; i<numCounters; i++) {
	if (state[i] == real_kind) {
	    state[i] = HWC_NONE;
	}
    }
}

// Find what counters are enabled in the hardware
ierr kapi_hwcounter_set::readConfig()
{
    extern instrumenter *theGlobalInstrumenter;
    assert(theGlobalInstrumenter != 0);

    pcrData pcr = theGlobalInstrumenter->getPcr();

    for (int i=0; i<numMapEntries; i++) {
	assert(id2state[i].kind == i);
	if ((id2state[i].picnum == 0 || id2state[i].picnum == any_pic) &&
	    pcr.s0 == id2state[i].pcrval) {
	    state[0] = id2state[i].kind;
	}
	if ((id2state[i].picnum == 1 || id2state[i].picnum == any_pic) &&
	    pcr.s1 == id2state[i].pcrval) {
	    state[1] = id2state[i].kind;
	}
    }
    return 0;
}

// Do the actual programming of the counters
ierr kapi_hwcounter_set::writeConfig()
{
    extern instrumenter *theGlobalInstrumenter;
    assert(theGlobalInstrumenter != 0);
    pcrData pcr = theGlobalInstrumenter->getPcr();

    bool modified = false;
    kapi_hwcounter_kind i0 = state[0];
    if (id2state[i0].picnum == 0 || id2state[i0].picnum == any_pic) {
	pcr.s0 = id2state[i0].pcrval;
	modified = true;
    }
    kapi_hwcounter_kind i1 = state[1];
    if (id2state[i1].picnum == 1 || id2state[i1].picnum == any_pic) {
	pcr.s1 = id2state[i1].pcrval;
	modified = true;
    }
    if (modified) {
	pcr.st = true;
	theGlobalInstrumenter->setPcr(pcr);
	pcrData newpcr = theGlobalInstrumenter->getPcr();
    }

    return 0;
}

