#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/**
 * sound.h - PSG (Programmable Sound Generator) sound system
 * 
 * Manages 8-channel sound effects with round-robin allocation
 */

// Waveform types
typedef enum {
    PSG_WAVE_SINE = 0,
    PSG_WAVE_SQUARE = 1,
    PSG_WAVE_SAWTOOTH = 2,
    PSG_WAVE_TRIANGLE = 3,
    PSG_WAVE_NOISE = 4
} PSGWaveform;

// Sound effect types (for round-robin channel allocation)
typedef enum {
    SFX_TYPE_PLAYER_FIRE = 0,
    SFX_TYPE_ENEMY_FIRE = 1,
    SFX_TYPE_HIT = 2,
    SFX_TYPE_COUNT = 2  // Only 2 types currently used
} SFXType;

/**
 * Initialize the PSG sound system
 */
void init_psg(void);

/**
 * Play a sound effect with round-robin channel allocation
 * @param sfx_type Sound effect type (determines channel allocation)
 * @param freq Frequency in Hz
 * @param wave Waveform type
 * @param attack Attack rate (0-15)
 * @param decay Decay rate (0-15)
 * @param release Release rate (0-15)
 * @param volume Volume (0-15, where 0=loud, 15=silent)
 */
void play_sound(uint8_t sfx_type, uint16_t freq, uint8_t wave, 
                uint8_t attack, uint8_t decay, uint8_t release, uint8_t volume);

#endif // SOUND_H
