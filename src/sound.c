#include "sound.h"
#include "constants.h"
#include <rp6502.h>
#include <stdint.h>

// ============================================================================
// MODULE STATE
// ============================================================================

// Channel allocation (2 channels per effect type for round-robin)
static uint8_t next_channel[SFX_TYPE_COUNT] = {0, 2};

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

/**
 * Stop sound on a channel
 */
static void stop_sound(uint8_t channel)
{
    if (channel > 7) return;
    
    uint16_t psg_addr = PSG_XRAM_ADDR + (channel * 8) + 6;  // pan_gate offset
    RIA.addr0 = psg_addr;
    RIA.rw0 = 0x00;  // Gate off (release)
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void init_psg(void)
{
    // Enable PSG at XRAM address PSG_XRAM_ADDR
    xregn(0, 1, 0x00, 1, PSG_XRAM_ADDR);
    
    // Clear all 8 channels (64 bytes total)
    RIA.addr0 = PSG_XRAM_ADDR;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < 64; i++) {
        RIA.rw0 = 0;
    }
}

void play_sound(uint8_t sfx_type, uint16_t freq, uint8_t wave, 
                uint8_t attack, uint8_t decay, uint8_t release, uint8_t volume)
{
    if (sfx_type >= SFX_TYPE_COUNT) return;
    
    // Get base channel for this effect type (each type has 2 channels)
    uint8_t base_channel = sfx_type * 2;
    uint8_t channel = base_channel + next_channel[sfx_type];
    
    // Release the previous sound on the old channel
    uint8_t old_channel = base_channel + (1 - next_channel[sfx_type]);
    stop_sound(old_channel);
    
    // Toggle to next channel for this effect type
    next_channel[sfx_type] = 1 - next_channel[sfx_type];
    
    uint16_t psg_addr = PSG_XRAM_ADDR + (channel * 8);
    
    // Set frequency (Hz * 3)
    uint16_t freq_val = freq * 3;
    RIA.addr0 = psg_addr;
    RIA.rw0 = freq_val & 0xFF;          // freq low byte
    RIA.rw0 = (freq_val >> 8) & 0xFF;   // freq high byte
    
    // Set duty cycle (50%)
    RIA.rw0 = 128;
    
    // Set volume and attack
    RIA.rw0 = (volume << 4) | (attack & 0x0F);
    
    // Set decay volume to 15 (silent) so sound fades naturally without sustain
    RIA.rw0 = (15 << 4) | (decay & 0x0F);
    
    // Set waveform and release
    RIA.rw0 = (wave << 4) | (release & 0x0F);
    
    // Set pan (center) and gate (on)
    RIA.rw0 = 0x01;  // Center pan, gate on
}
