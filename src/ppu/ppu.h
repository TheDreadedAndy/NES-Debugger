#ifndef _NES_PPU
#define _NES_PPU

#include <cstdlib>

#include "../util/data.h"
#include "../memory/palette.h"
#include "../memory/memory.h"
#include "../sdl/renderer.h"

/*
 * Emulates the graphics chip of the NES, executing a clock cycle whenever
 * RunCycle is called.
 */
class Ppu {
  private:
    // Constants for the SOAM buffer.
    static const size_t kNumSoamBuffers_ = 2;

    // Constants for rendering.
    static const size_t kScreenWidth_ = 256;
    static const size_t kTileBufferSize_ = 264;
    static const size_t kTileWidth_ = 8;
    static const size_t kTilePlanes_ = 2;

    // Internal PPU registers.
    DoubleWord vram_addr_ = 0;
    DoubleWord temp_vram_addr_ = 0;
    bool write_toggle_ = false;
    DataWord fine_x_ = 0;

    // Memory mapped PPU registers.
    DataWord bus_ = 0;
    DataWord vram_buf_ = 0;
    DataWord ctrl_ = 0;
    DataWord mask_ = 0;
    DataWord status_ = 0;
    DataWord oam_addr_ = 0;

    // Working memory for the PPU.
    DataWord *primary_oam_;
    DataWord soam_render_buf_ = 0;
    DataWord *soam_buffer_[kNumSoamBuffers_];

    // Temporary storage used in rendering.
    Pixel *render_buffer_;
    DataWord *tile_buffer_;
    DataWord next_tile_[kTilePlanes_];
    DataWord next_palette_;

    // MDR and write toggle, used for 2-cycle r/w system.
    DataWord mdr_ = 0;
    bool mdr_write_ = false;

    // Tracks the current scanline/cycle the PPU emulation is on.
    size_t current_scanline_ = 261;
    size_t current_cycle_ = 0;
    bool frame_odd_ = false;

    // Holds the Memory class to be used to access VRAM.
    Memory *memory_;

    // Holds a pointer to the current set of palette data in the pixel format.
    const Pixel *pixel_data_;

    // Holds the Renderer class to be used to draw pixels to the screen.
    Renderer *renderer_;

    // Holds the NMI line used to communicate with the CPU.
    bool *nmi_line_;

    // Helper functions for the PPU emulation.
    bool IsDisabled(void);
    size_t UpdateCounters(size_t &cycles, size_t &next_current_cycle);
    void Disabled(size_t delta);
    void DrawBackground(size_t delta);
    void Render(size_t delta);
    void RenderVisible(size_t delta);
    void RenderUpdateFrame(bool output);
    void RenderFetchTiles(size_t delta, bool alt_buffer);
    void RenderUpdateTileBuffer(void);
    DataWord RenderGetAttribute(void);
    DataWord RenderGetTile(DataWord index, bool plane_high);
    void RenderDrawPixels(size_t delta);
    void RenderUpdateHori(void);
    void RenderDummyNametableAccess(size_t delta);
    void RenderXinc(void);
    void RenderYinc(void);
    void RenderBlank(size_t delta);
    void RenderPre(size_t delta);
    void RenderUpdateVert(void);
    void EvalClearSoam(void);
    void EvalSprites(void);
    bool EvalInRange(DataWord sprite_y);
    void EvalFillSoamBuffer(DataWord *sprite_data, bool is_zero);
    void EvalGetSprite(DataWord *sprite_data, DataWord *pat_lo,
                                              DataWord *pat_hi);
    void EvalFetchSprites(void);
    void Signal(void);
    void MmioScrollWrite(DataWord val);
    void MmioAddrWrite(DataWord val);
    void MmioVramAddrInc(void);

  public:
    // Creates a PPU object.
    Ppu(void);

    // Connects the PPU to the rest of the emulation.
    void Connect(Memory *memory, Renderer *render, bool *nmi_line);

    // Determines how many cycles can be run before the execution will
    // update the state of another chip. Used to schedule emulator execution.
    size_t Schedule(void);

    // Emulates the specified number of PPU cycles.
    // Connect() must be called before this function can be used.
    void RunSchedule(size_t cycles);

    // Reads from a memory mapped PPU register.
    DataWord Read(DoubleWord reg_addr);

    // Writes to a memory mapper PPU register.
    void Write(DoubleWord reg_addr, DataWord val);

    // Directly writes to OAM with the given value.
    // The current OAM address is incremented by this operation.
    void OamDma(DataWord val);

    ~Ppu(void);
};

#endif
