#ifndef _NES_EMU
#define _NES_EMU

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include "../config/config.h"
#include "../sdl/window.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"

/*
 * Manages the emulation of the NES by creating and managing
 * all the objects associated with it. Only one instance of
 * this object should exist at a time.
 */
class Emulation {
  private:
    // Stores the SDL window used by the emulation.
    Window *window_;

    // Stores the objects for the NES emulation.
    Memory *memory_;
    Cpu *cpu_;
    Ppu *ppu_;
    Apu *apu_;

    // Redefinition of the structure used for timing.
    typedef struct timespec EmuTime;

    // Used to time the emulator and calculate the frame rate.
    EmuTime last_sync_time_ = { 0, 0 };
    EmuTime last_frame_time_ = { 0, 0 };
    long frames_counted_ = 0;

    // Stores the objects provided by the factory create function.
    Emulation(Window *window, Memory *memory, Cpu *cpu, Ppu *ppu, Apu *apu);

    // Syncs the emulation to the given frame rate.
    void SyncFrameRate(void);

    // Updates the frame rate displayed in the SDL window title.
    void UpdateFrameCounter(void);

    // Gets the current time.
    void TimeGet(EmuTime *time);

    // Checks if time1 is greater than time2.
    bool TimeGt(EmuTime *time1, EmuTime *time2);

    // Subtracts the first time from the second time, storing the result.
    void TimeDiff(EmuTime *time1, EmuTime *time2, EmuTime *res);

    // Runs the NES emulation for 1/60th of its clock rate.
    void RunEmulationCycle(void);

  public:
    // Attempts to create the object used to manage the emulation.
    static Emulation *Create(FILE *rom, Config *config);

    // Starts the main emulation loop. This function does not
    // return until the user or OS closes the emulation window.
    void Run(void);

    // Deletes the Emulation object.
    ~Emulation(void);
};

#endif
