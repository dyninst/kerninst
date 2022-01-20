#include "machineInfo.h"

// Reconstruct from the info shipped through the connection
machineInfo::machineInfo(const presentedMachineInfo &ipmi) :
    pmi(ipmi)
{
}

// Method to convert machineInfo to presentedMachineInfo
void machineInfo::convertTo(presentedMachineInfo *ppmi)
{
    *ppmi = pmi;
}

// The number of CPUs in a (kerninstd) system
unsigned machineInfo::getNumCPUs() const
{
    return pmi.theCpusInfo.size();
}

// Return the physical id of the i-th CPU (may be different from "i")
unsigned machineInfo::getIthCPUId(unsigned i) const
{
    assert(i < pmi.theCpusInfo.size());
    return pmi.theCpusInfo[i].cpuid;
}

// Return the frequency of the i-th CPU
unsigned machineInfo::getCPUClockMHZ(unsigned icpu) const
{
    assert(icpu < pmi.theCpusInfo.size());
    return pmi.theCpusInfo[icpu].megahertz;
}

// Return the maximum physical id of the CPUs
unsigned machineInfo::getMaxCPUId() const
{
    pdvector<cpu_info>::const_iterator iter = pmi.theCpusInfo.begin();
    pdvector<cpu_info>::const_iterator iend = pmi.theCpusInfo.end();
    unsigned max_id = 0;

    for (; iter != iend; iter++) {
	if (iter->cpuid > max_id) {
	    max_id = iter->cpuid;
	}
    }

    return max_id;
}

// Return offset of cpu field in a kthread for the kerninstd machine
unsigned machineInfo::getKernelThreadCpuOffset() const 
{
    return pmi.t_cpu_offset;
}
#ifdef ppc64_unknown_linux2_4
unsigned machineInfo::getKernelPacaCurrentOffset() const 
{
    return pmi.t_pacacurrent_offset;
}
unsigned machineInfo::getKernelPacaAccess() const 
{
    return pmi.paca_access;
}
#endif

// Return offset of process pointer in a kthread for the kerninstd machine
unsigned machineInfo::getKernelThreadProcOffset() const
{
    return pmi.t_procp_offset;
}

// Return offset of pid pointer in a process for the kerninstd machine
unsigned machineInfo::getKernelProcPidOffset() const
{
    return pmi.p_pidp_offset;
}
    
// Return offset of pid id in a pid for the kerninstd machine
unsigned machineInfo::getKernelPidIdOffset() const
{
    return pmi.pid_id_offset;
}

// Return size of a kthread
unsigned machineInfo::getKernelThreadSize() const
{
    return pmi.t_size;
}

// 0 == sparcv8+
unsigned machineInfo::getKerninstdABICode() const
{
    return pmi.kerninstdABICode;
}

// 0 == sparcv8+, 1 == sparcv9
unsigned machineInfo::getKernelABICode() const
{
    return pmi.kernelABICode;
}

// 6 == Solaris 2.6, 7 == Solaris 7, 8 == Solaris 8
unsigned machineInfo::getOSVersion() const
{
    return pmi.os_version;
}

// Return true if the machine has the %stick register
bool machineInfo::hasStick() const
{
    return (pmi.has_stick != 0);
}

// Return the system clock frequency (may not equal %tick frequency
// if %stick is present)
unsigned machineInfo::getSystemClockMHZ() const
{
    return (pmi.system_freq / 1000000);
}

// Interrupt level for running the context switch code
unsigned machineInfo::getDispatcherLevel() const
{
    return pmi.disp_level;
}

// Test if the address is in the nucleus
bool machineInfo::isKernelAddrInNucleus(kptr_t addr) const
{
    return (addr >= pmi.nucleusTextBoundsLow &&
	    addr < pmi.nucleusTextBoundsHi);
}
