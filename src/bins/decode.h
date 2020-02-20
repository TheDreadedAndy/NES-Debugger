#ifndef _NES_2A03_DECODE
#define _NES_2A03_DECODE

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../util/data.h"
#include "../cpu/cpu_operation.h"
#include "./decode_state.h"

/*
 * Represents an emulated 6502 CPU. The state of the CPU is managed
 * using a CPU state queue. The CPU must be given a memory object to use
 * when it is initialized.
 */
class Decode {
  private:
    // Used to manage the current state of the CPU emulation.
    DecodeState *state_;

    /* Helper functions for the CPU emulation */
    void DecodeInst(void);
    void DecodeIzpx(CpuOperation op);
    void DecodeZp(CpuOperation op);
    void DecodeImm(CpuOperation op);
    void DecodeAbs(CpuOperation op);
    void DecodeIzpY(CpuOperation op);
    void DecodeZpR(CpuOperation op, CpuReg reg);
    void DecodeAbR(CpuOperation op, CpuReg reg);
    void DecodeNomem(CpuOperation op);
    void DecodeRwZp(CpuOperation op);
    void DecodeRwAbs(CpuOperation op);
    void DecodeRwZpx(CpuOperation op);
    void DecodeRwAbx(CpuOperation op);
    void DecodeWIzpx(CpuOperation op);
    void DecodeWZp(CpuOperation op);
    void DecodeWAbs(CpuOperation op);
    void DecodeWIzpY(CpuOperation op);
    void DecodeWZpR(CpuOperation op, CpuReg reg);
    void DecodeWAbR(CpuOperation op, CpuReg reg);
    void DecodePush(CpuOperation op);
    void DecodePull(CpuOperation op);

  public:
    // Creates a new CPU object.
    Decode(void);

    // Returns the decoded form of the given instruction.
    CpuOperation *DecodeInst(DoubleWord inst);

    // Deletes the CPU object. The associated memory object is not deleted.
    ~Decode(void);
};

#endif
