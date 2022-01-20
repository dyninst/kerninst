#ifndef _MACHINE_INFO_H_
#define _MACHINE_INFO_H_

#include "kerninstdClient.xdr.h"

class kernelDriver;

class machineInfo {
 private:
    presentedMachineInfo pmi;

 public:
    machineInfo(const presentedMachineInfo &);

    // Method to convert machineInfo to presentedMachineInfo
    void convertTo(presentedMachineInfo *pmi);

    // The number of CPUs in a (kerninstd) system
    unsigned getNumCPUs() const;

    // Return the physical id of the i-th CPU (may be different from "i")
    unsigned getIthCPUId(unsigned i) const;

    // Return the frequency of the i-th CPU
    unsigned getCPUClockMHZ(unsigned icpu) const;

    // Return the maximum physical id of the CPUs
    unsigned getMaxCPUId() const;

    // 0 == sparcv8+
    unsigned getKerninstdABICode() const;

    // 0 == sparcv8+, 1 == sparcv9
    unsigned getKernelABICode() const;

    // 6 == Solaris 2.6, 7 == Solaris 7, 8 == Solaris 8
    unsigned getOSVersion() const;

    // Return true if the machine has the %stick register
    bool hasStick() const;

    // Return the system clock frequency (may not equal %tick frequency
    // if %stick is present)
    unsigned getSystemClockMHZ() const;

    // Interrupt level for running the context switch code
    unsigned getDispatcherLevel() const;

    // The address of the start of the nucleus text
    kptr_t getNucleusTextBoundsLow() const;

    // The address of the end of the nucleus text
    kptr_t getNucleusTextBoundsHi() const;

    // Test if the address is in the nucleus
    bool isKernelAddrInNucleus(kptr_t addr) const;

    // Return offset of cpu field in a kthread for the kerninstd machine
    unsigned getKernelThreadCpuOffset() const;
#ifdef ppc64_unknown_linux2_4    
    // Return &local_paca.xCurrent - &local_paca for the kerninstd machine
    unsigned getKernelPacaCurrentOffset() const;
    //Return whether paca is accessed using sprg3, r13 or something else
    unsigned getKernelPacaAccess() const;
#endif
    // Return offset of process pointer in a kthread for the kerninstd machine
    unsigned getKernelThreadProcOffset() const;

    // Return offset of pid pointer in a process for the kerninstd machine
    unsigned getKernelProcPidOffset() const;
    
    // Return offset of pid id in a pid for the kerninstd machine
    unsigned getKernelPidIdOffset() const;

    // Return size of a kthread
    unsigned getKernelThreadSize() const;
};
#endif
