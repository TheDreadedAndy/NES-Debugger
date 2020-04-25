#ifndef _NES_APU
#define _NES_APU

#include <cstdlib>
#include <cstdint>

#include "../sdl/audio_player.h"
#include "../memory/memory.h"
#include "../util/data.h"

/*
 * This class contains all the structures, methods, and data necessary to
 * emulate the NES's APU.
 *
 * The APU supports five channels of sound, and can communicate with the
 * CPU through the generation of IRQ's.
 */
class Apu {
  private:
    // Used to play the generated audio to the user.
    AudioPlayer *audio_;

    // Used to communicate with a memory and cpu object.
    Memory *memory_;
    DataWord *irq_line_;

    // Contains the data related to the operation of an APU pulse channel.
    struct ApuPulse {
      // Memory mapped registers.
      DoubleWord timer;
      DataWord length;
      DataWord sweep;
      bool sweep_reload;
      DataWord control;

      // Internal registers.
      DataWord sweep_counter;
      DataWord pos;
      DoubleWord clock;
      DataWord output;
      DataWord env_clock;
      DataWord env_volume;
      bool env_reset;
    };

    // Contains the data related to the operation of the APU triangle channel.
    struct ApuTriangle {
      // Memory mapped registers.
      DoubleWord timer;
      DataWord length;
      DataWord control;
      bool linear_reload;

      // Internal registers.
      DoubleWord clock;
      DataWord output;
      DataWord linear;
      DataWord pos;
    };

    // Contains the data related to the operation of the APU noise channel.
    struct ApuNoise {
      // Memory mapped registers.
      DataWord period;
      DataWord length;
      DataWord control;

      // Internal registers.
      DoubleWord shift;
      DoubleWord timer;
      DoubleWord clock;
      DataWord env_clock;
      DataWord env_volume;
      DataWord output;
      bool env_reset;
    };

    // Contains the data related to the operation of the APU DMC channel.
    struct ApuDmc {
      // Memory mapped registers.
      DataWord control;
      DataWord rate;
      DataWord level;
      DoubleWord addr;
      DoubleWord length;

      // Internal registers.
      DoubleWord current_addr;
      DoubleWord bytes_remaining;
      DataWord bits_remaining;
      DataWord sample_buffer;
      DataWord output;
      bool silent;

      // The DMC updates whenever this value is greater than the corresponding
      // rate value.
      size_t clock;
    };

    // The individual channels associated with each APU object.
    ApuPulse *pulse_a_;
    ApuPulse *pulse_b_;
    ApuTriangle *triangle_;
    ApuNoise *noise_;
    ApuDmc *dmc_;

    // Used to control the channels of the APU. Set through MMIO
    // in the emulation.
    DataWord frame_control_ = 0;
    DataWord channel_status_ = 0;

    // Tracks if the DMC or the frame counter are currently generating
    // an IRQ.
    bool dmc_irq_ = false;
    bool frame_irq_ = false;

    // Tracks the current frame step of the APU. The step size can be
    // controlled by the emulation.
    size_t frame_clock_ = 0;
    size_t frame_step_ = 0;

    // Most functions of the APU are only clocked on even cycles.
    bool cycle_even_ = false;

    // Tracks when the next sample should be sent to the audio device buffer.
    float sample_clock_ = 0;

    // Sound output is run through two high pass filters and a low pass
    // filter, which are managed using these variables.
    float last_normal_sample_ = 0;
    float last_hpf1_sample_ = 0;
    float last_hpf2_sample_ = 0;
    float last_lpf_sample_ = 0;

    /* Helper functions */
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
    float GetPulseOutput(void);
    float GetTndOutput(void);
    float FilterNextSample(float sample);

  public:
    // Creates a new APU.
    Apu(void);

    // Connects the APU to the rest of the console.
    void Connect(Memory *memory, AudioPlayer *audio, DataWord *irq_line);

    // Returns the number of CPU cycles until the next IRQ from the APU.
    size_t Schedule(void);

    // Runs an APU cycle, updating the channels.
    // Connect() must be called before this function.
    void RunCycle(void);

    // Writes to a memory mapped APU register.
    void Write(DoubleWord reg_addr, DataWord val);

    // Reads from a memory mapped APU register.
    DataWord Read(DoubleWord reg_add);

    // Frees the APU channel structures.
    ~Apu(void);
};

#endif
