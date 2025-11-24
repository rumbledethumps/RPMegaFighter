#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>
#include <stdbool.h>

/**
 * music.h - Music playback system for title screen
 * 
 * Uses PSG channels 4-7 (channels 0-3 reserved for sound effects)
 * Implements a simple note sequencer with tempo control
 */

// Music notes (frequency values in Hz)
typedef enum {
    NOTE_REST = 0,
    NOTE_C2 = 65,
    NOTE_CS2 = 69,
    NOTE_D2 = 73,
    NOTE_DS2 = 78,
    NOTE_E2 = 82,
    NOTE_F2 = 87,
    NOTE_FS2 = 93,
    NOTE_G2 = 98,
    NOTE_GS2 = 104,
    NOTE_A2 = 110,
    NOTE_AS2 = 117,
    NOTE_B2 = 123,
    NOTE_C3 = 131,
    NOTE_CS3 = 139,
    NOTE_D3 = 147,
    NOTE_DS3 = 156,
    NOTE_E3 = 165,
    NOTE_F3 = 175,
    NOTE_FS3 = 185,
    NOTE_G3 = 196,
    NOTE_GS3 = 208,
    NOTE_A3 = 220,
    NOTE_AS3 = 233,
    NOTE_B3 = 247,
    NOTE_C4 = 262,
    NOTE_CS4 = 277,
    NOTE_D4 = 294,
    NOTE_DS4 = 311,
    NOTE_E4 = 330,
    NOTE_F4 = 349,
    NOTE_FS4 = 370,
    NOTE_G4 = 392,
    NOTE_GS4 = 415,
    NOTE_A4 = 440,
    NOTE_AS4 = 466,
    NOTE_B4 = 494,
    NOTE_C5 = 523,
    NOTE_CS5 = 554,
    NOTE_D5 = 587,
    NOTE_DS5 = 622,
    NOTE_E5 = 659,
    NOTE_F5 = 698,
    NOTE_FS5 = 740,
    NOTE_G5 = 784,
    NOTE_GS5 = 831,
    NOTE_A5 = 880,
    NOTE_AS5 = 932,
    NOTE_B5 = 988,
    NOTE_C6 = 1047,
    NOTE_CS6 = 1109,
    NOTE_D6 = 1175,
    NOTE_DS6 = 1245,
    NOTE_E6 = 1319,
    NOTE_F6 = 1397,
    NOTE_FS6 = 1480,
    NOTE_G6 = 1568,
    NOTE_GS6 = 1661,
    NOTE_A6 = 1760
} MusicNote;

// Note structure for sequencer
typedef struct {
    uint16_t freq;      // Frequency in Hz (0 = rest)
    uint8_t duration;   // Duration in beats (at 120 BPM, 1 beat = 30 frames)
} Note;

/**
 * Initialize the music system
 * Must be called after init_psg()
 */
void init_music(void);

/**
 * Start playing music with custom tracks
 * Pass NULL for any track to skip it
 * @param melody Melody note sequence (plays on channel 4 with square wave)
 * @param bass Bass note sequence (plays on channel 5 with triangle wave)
 * @param kick Kick drum sequence (plays on channel 6)
 * @param hihat Hi-hat sequence (plays on channel 7)
 */
void start_music(const Note* melody, const Note* bass, const Note* kick, const Note* hihat);

/**
 * Start playing the title screen music
 * Music will loop continuously
 */
void start_title_music(void);

/**
 * Start playing the gameplay music (bass only)
 * Music will loop continuously
 */
void start_gameplay_music(void);

/**
 * Start playing the end/game over screen music
 * Music will loop continuously
 */
void start_end_music(void);

/**
 * Stop all music playback
 */
void stop_music(void);

/**
 * Update music playback - call once per frame
 * Handles tempo timing and note progression
 */
void update_music(void);

/**
 * Check if music is currently playing
 */
bool is_music_playing(void);

/**
 * Increase music tempo by decreasing frames per beat
 * Called when player levels up
 */
void increase_music_tempo(void);

/**
 * Reset music tempo to default
 * Called when game ends or returns to title screen
 */
void reset_music_tempo(void);

#endif // MUSIC_H
