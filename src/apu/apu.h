#ifndef _NES_APU
#define _NES_APU

#include <cstdlib>

#include "../util/data.h"

/*
 * Contains the data related to the operation of an APU pulse channel.
 */
typedef struct pulse {
  // Memory mapped registers.
  dword_t timer;
  word_t length;
  word_t sweep;
  bool sweep_reload;
  word_t control;

  // Internal registers.
  word_t sweep_counter;
  word_t pos;
  dword_t clock;
  word_t output;
  word_t env_clock;
  word_t env_volume;
} ApuPulse;

/*
 * Contains the data related to the operation of the APU triangle channel.
 */
typedef struct triangle {
  // Memory mapped registers.
  dword_t timer;
  word_t length;
  word_t control;
  bool linear_reload;

  // Internal registers.
  dword_t clock;
  word_t output;
  word_t linear;
  word_t pos;
} ApuTriangle;

/*
 * Contains the data related to the operation of the APU noise channel.
 */
typedef struct noise {
  // Memory mapped registers.
  word_t period;
  word_t length;
  word_t control;

  // Internal registers.
  dword_t shift;
  dword_t timer;
  dword_t clock;
  word_t env_clock;
  word_t env_volume;
  word_t output;
} ApuNoise;

/*
 * Contains the data related to the operation of the APU DMC channel.
 */
typedef struct dmc {
  // Memory mapped registers.
  word_t control;
  word_t rate;
  word_t level;
  dword_t addr;
  dword_t length;

  // Internal registers.
  dword_t current_addr;
  dword_t bytes_remaining;
  word_t bits_remaining;
  word_t sample_buffer;
  word_t output;
  bool silent;

  // The DMC updates whenever this value is greater than the corresponding
  // rate value.
  size_t clock;
} ApuDmc;

/*
 * TODO
 */
class Apu {
  private:
    // The individual channels associated with each APU object.
    ApuPulse *pulse_a_;
    ApuPulse *pulse_b_;
    ApuTriangle *triangle_;
    ApuNoise *noise_;
    ApuDmc *dmc_;

    // Used to control the channels of the APU. Set through MMIO
    // in the emulation.
    DataWord frame_control_;
    DataWord channel_status_;

    // Tracks if the DMC or the frame counter are currently generating
    // an IRQ.
    bool dmc_irq_;
    bool frame_irq_;

    // Tracks the current frame step of the APU. The step size can be
    // controlled by the emulation.
    size_t frame_clock_;
    size_t frame_step_;

    // Most functions of the APU are only clocked on even cycles.
    bool cycle_even_;

    // Used to quickly obtain the correct output of the APU.
    float *pulse_table_;
    float *tndmc_table_;

    /* Helper functions */
    void InitPulseTable(void);
    void InitTndmcTable(void);
    void RunFrameStep(void);
    void UpdateSweep(ApuPulse *pulse);
    DoubleWord GetPulseTarget(ApuPulse *pulse);
    void UpdatePulseEnvelope(ApuPulse *pulse);
    void UpdateNoiseEnvelope(void);
    void UpdateTriangleLinear(void);
    void UpdateLength(void);
    void IncFrame(void);
    void UpdatePulse(ApuPulse *pulse);
    void UpdateTriangle(void);
    void UpdateNoise(void);
    void UpdateNoiseShift(void);
    void UpdateDmc(void);
    void StatusWrite(DataWord val);
    void PlaySample(void);

  public:
    // Creates a new APU.
    // FIXME: needs memory.
    Apu(void);

    // Runs an APU cycle, updating the channels.
    void RunCycle(void);

    // Writes to a memory mapped APU register.
    void Write(DoubleWord reg_addr, DataWord val);

    // Reads from a memory mapped APU register.
    DataWord Read(DoubleWord reg_add);

    // Frees the APU channel structures.
    ~Apu(void);
};

#endif
