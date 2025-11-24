#include "music.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// CONSTANTS
// ============================================================================

// PSG memory location (must match sound.c)
#define PSG_XRAM_ADDR 0xFF00

// Music channels (4-7, leaving 0-3 for sound effects)
#define MUSIC_CHANNEL_START 4
#define MUSIC_CHANNEL_COUNT 4

// Tempo: 120 BPM = 2 beats per second = 1 beat per 30 frames (at 60 FPS)
// This value decreases with each level to speed up the music
#define DEFAULT_FRAMES_PER_BEAT 15
#define MIN_FRAMES_PER_BEAT 5
static int frames_per_beat = DEFAULT_FRAMES_PER_BEAT;

// Waveforms
#define WAVE_SQUARE 1
#define WAVE_TRIANGLE 3
#define WAVE_SAWTOOTH 2
#define WAVE_NOISE 4

// Instrument types
#define INSTRUMENT_NORMAL 0
#define INSTRUMENT_KICK 1
#define INSTRUMENT_HIHAT 2

// ============================================================================
// MUSIC DATA - Simple Melody for Title Screen
// ============================================================================


// A direct translation of the Furnance tracker snippet with the correct arpeggio logic.

// --- Kick Drum (Track 0) ---
// Plays a C3 kick on specific rows. A tracker row is typically a 16th note.
static const Note title_kick[] = {
    // This pattern is directly from Track 0 in your snippet.
    {NOTE_C3, 1}, {NOTE_REST, 5}, // 00: Kick, followed by rests
    {NOTE_C3, 1}, {NOTE_REST, 9}, // 06: Kick
    {NOTE_C3, 1}, {NOTE_REST, 5}, // 10: Kick
    {NOTE_C3, 1}, {NOTE_REST, 3}, // 16: Kick
    {NOTE_C3, 1}, {NOTE_REST, 5}, // 1A: From your snippet, C3 04 78 on row 1A
    {NOTE_C3, 1}, {NOTE_REST, 3}, // 20: Kick
    {NOTE_C3, 1}, {NOTE_REST, 5}, // 26: Kick
    {NOTE_C3, 1}, {NOTE_REST, 9}, // 30: Kick
    {NOTE_C3, 1}, {NOTE_REST, 5}, // 36: Kick
    {NOTE_C3, 1}, {NOTE_REST, 5}, // 3A: From your snippet, C3 04 78 on row 3A
    
    {0, 0}  // End marker
};


// --- Hi-Hat (Track 1) ---
// Plays a C3 hi-hat on specific off-beats.
static const Note title_hihat[] = {
    // This syncopated pattern is directly from Track 1 in your snippet.
    {NOTE_REST, 4}, {NOTE_C6, 1}, {NOTE_REST, 7}, // 04: Hat
    {NOTE_C6, 1},   {NOTE_REST, 7},             // 0C: Hat
    {NOTE_C6, 1},   {NOTE_REST, 7},             // 14: Hat
    {NOTE_C6, 1},   {NOTE_REST, 7},             // 1C: Hat
    {NOTE_C6, 1},   {NOTE_REST, 7},             // 24: Hat
    {NOTE_C6, 1},   {NOTE_REST, 7},             // 2C: Hat
    {NOTE_C6, 1},   {NOTE_REST, 7},             // 34: Hat
    {NOTE_C6, 1},   {NOTE_REST, 7},             // 3C: Hat

    {0, 0}  // End marker
};


// --- Bassline (Track 4) ---
// A continuous 8th-note octave-hopping arpeggio.
// It plays C for the first half, then switches to G in the second half.
static const Note title_bass[] = {
    // Rows 00-1F: Arpeggio on C2
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1}, // Beat 1 & 2
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1}, // Beat 3 & 4
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1}, // ...repeats for 32 rows
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1},
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1},
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1},
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1},
    {NOTE_C2, 1}, {NOTE_C3, 1}, {NOTE_C2, 1}, {NOTE_C3, 1},

    // Rows 20-3F: Arpeggio on G1 (Note change detected at row 0x20)
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1}, // Beat 1 & 2
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1}, // Beat 3 & 4
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1}, // ...repeats for 32 rows
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1},
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1},
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1},
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1},
    {NOTE_G2, 1}, {NOTE_G3, 1}, {NOTE_G2, 1}, {NOTE_G3, 1},

    {0, 0}  // End marker
};

// Main melody with faster notes and more rhythmic interest.
static const Note end_melody[] = {
    // Measure 1
    {NOTE_G4, 1}, {NOTE_C5, 1}, {NOTE_E5, 1}, {NOTE_G5, 1},
    {NOTE_F5, 1}, {NOTE_E5, 1}, {NOTE_D5, 1}, {NOTE_C5, 1},

    // Measure 2
    {NOTE_G4, 1}, {NOTE_B4, 1}, {NOTE_D5, 1}, {NOTE_G5, 1},
    {NOTE_F5, 1}, {NOTE_E5, 1}, {NOTE_D5, 1}, {NOTE_B4, 1},

    // Measure 3
    {NOTE_F4, 1}, {NOTE_A4, 1}, {NOTE_C5, 1}, {NOTE_F5, 1},
    {NOTE_E5, 1}, {NOTE_D5, 1}, {NOTE_C5, 1}, {NOTE_A4, 1},

    // Measure 4 - A classic resolving pattern
    {NOTE_G4, 1}, {NOTE_B4, 1}, {NOTE_D5, 1}, {NOTE_F5, 1},
    {NOTE_E5, 2}, {NOTE_C5, 2}, // End on a strong, held note

    {0, 0}  // End marker
};

// A driving, arpeggiated bass line that follows the chords.
static const Note end_bass[] = {
    // Measure 1: C-major arpeggio
    {NOTE_C3, 1}, {NOTE_G3, 1}, {NOTE_C4, 1}, {NOTE_G3, 1},
    {NOTE_C3, 1}, {NOTE_G3, 1}, {NOTE_C4, 1}, {NOTE_G3, 1},

    // Measure 2: G-major arpeggio
    {NOTE_G3, 1}, {NOTE_D4, 1}, {NOTE_G4, 1}, {NOTE_D4, 1},
    {NOTE_G3, 1}, {NOTE_D4, 1}, {NOTE_G4, 1}, {NOTE_D4, 1},

    // Measure 3: F-major arpeggio
    {NOTE_F3, 1}, {NOTE_C4, 1}, {NOTE_F4, 1}, {NOTE_C4, 1},
    {NOTE_F3, 1}, {NOTE_C4, 1}, {NOTE_F4, 1}, {NOTE_C4, 1},

    // Measure 4: G-major leading back to C
    {NOTE_G3, 1}, {NOTE_D4, 1}, {NOTE_G4, 1}, {NOTE_D4, 1},
    {NOTE_C3, 1}, {NOTE_G3, 1}, {NOTE_C4, 1}, {NOTE_G3, 1}, // Quick turnaround

    {0, 0}  // End marker
};

// A classic "four-on-the-floor" kick drum pattern for high energy.
static const Note end_kick[] = {
    // Measures 1-4 (pattern repeats)
    {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2},
    {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2},
    {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2},
    {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2}, {NOTE_C3, 2},

    {0, 0}  // End marker
};

// Constant eighth-note hi-hats to provide a sense of speed.
static const Note end_hihat[] = {
    // Measures 1-4 (pattern repeats)
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},
    {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1}, {NOTE_C6, 1},

    {0, 0}  // End marker
};

// ============================================================================
// MODULE STATE
// ============================================================================

typedef struct {
    const Note* sequence;   // Note sequence to play
    uint8_t position;       // Current position in sequence
    uint8_t frames_left;    // Frames left in current note
    uint8_t channel;        // PSG channel for this track
    uint8_t instrument;     // Instrument type
    bool active;            // Is this track active
} MusicTrack;

static MusicTrack tracks[MUSIC_CHANNEL_COUNT];
static bool music_playing = false;
static uint16_t master_loop_frames = 0;  // Total frames in one loop
static uint16_t current_frame = 0;        // Current frame in the loop

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

/**
 * Set a note on a PSG channel
 */
static void set_note(uint8_t channel, uint16_t freq, uint8_t instrument)
{
    if (channel < MUSIC_CHANNEL_START || channel >= MUSIC_CHANNEL_START + MUSIC_CHANNEL_COUNT) {
        return;
    }
    
    uint16_t psg_addr = PSG_XRAM_ADDR + (channel * 8);
    
    if (freq == 0) {
        // Rest - gate off
        RIA.addr0 = psg_addr + 6;  // pan_gate offset
        RIA.rw0 = 0x00;
        return;
    }
    
    // Set frequency (Hz * 3)
    uint16_t freq_val = freq * 3;
    RIA.addr0 = psg_addr;
    RIA.step0 = 1;
    RIA.rw0 = freq_val & 0xFF;          // freq low byte
    RIA.rw0 = (freq_val >> 8) & 0xFF;   // freq high byte
    
    // Configure based on instrument type
    if (instrument == INSTRUMENT_HIHAT) {
    // Closed Hi-hat: Noise with a very fast attack and a quick decay to silence.
        RIA.rw0 = 255;                      // Duty cycle (not used for noise)
        RIA.rw0 = (4 << 4) | 0;           // vol_attack: Loudest volume (0), fastest attack (0)
        RIA.rw0 = (15 << 4) | 2;          // vol_decay: Sustain at silence (15), fast decay rate (2)
        RIA.rw0 = (WAVE_NOISE << 4) | 2;  // wave_release: Noise waveform, fast release rate (2)
        RIA.step0 = 0;                    // Frequency (not critical for noise)
        RIA.rw0 = 0x01;
    } else if (instrument == INSTRUMENT_KICK) {
        // Kick drum - noise wave with punch and sustain
        RIA.rw0 = 128;            // duty cycle (lower for tighter sound)
        RIA.rw0 = (0 << 4) | 0;           // vol_attack: Loudest volume (0), fastest attack (0)
        RIA.rw0 = (15 << 4) | 7;          // vol_decay: Sustain at silence (15), medium decay (~240ms)
        RIA.rw0 = (WAVE_TRIANGLE << 4) | 0;  // wave_release (noise, release 0)
        RIA.step0 = 0;
        RIA.rw0 = 0x01;         // pan center, gate on
    } else {
        // Normal note - bass uses triangle wave on channel 5
        // Note: Since we call start_music(NULL, title_bass, NULL, NULL),
        // bass is the only active track and uses channel 5
        uint8_t waveform = WAVE_TRIANGLE;  // Always triangle for bass-only music
        
        RIA.rw0 = 64;          // Set duty cycle (50%)
        RIA.rw0 = (0 << 4) | 1; // Set volume (0 = LOUDEST) and attack (1 = fast)
        RIA.rw0 = (10 << 4) | 2; // Set decay volume (10 = loud sustain) and decay (2)
        RIA.rw0 = (waveform << 4) | 3; // Set waveform and release (3)
        RIA.step0 = 0;
        RIA.rw0 = 0x01;         // Set pan (center) and gate (on)
    }
}

/**
 * Stop a note on a channel
 */
static void stop_note(uint8_t channel)
{
    if (channel < MUSIC_CHANNEL_START || channel >= MUSIC_CHANNEL_START + MUSIC_CHANNEL_COUNT) {
        return;
    }
    
    uint16_t psg_addr = PSG_XRAM_ADDR + (channel * 8) + 6;  // pan_gate offset
    RIA.addr0 = psg_addr;
    RIA.rw0 = 0x00;  // Gate off
}

/**
 * Initialize a music track
 */
static void init_track(uint8_t track_idx, const Note* sequence, uint8_t channel, uint8_t instrument)
{
    if (track_idx >= MUSIC_CHANNEL_COUNT) return;
    
    tracks[track_idx].sequence = sequence;
    tracks[track_idx].position = 0;
    tracks[track_idx].frames_left = 0;
    tracks[track_idx].channel = channel;
    tracks[track_idx].instrument = instrument;
    tracks[track_idx].active = true;
}

/**
 * Update a single music track
 */
static void update_track(MusicTrack* track)
{
    if (!track->active || !track->sequence) return;
    
    // Release note a few frames before the end for decay
    if (track->frames_left == 3) {
        stop_note(track->channel);
    }
    
    // Decrement frame counter
    if (track->frames_left > 0) {
        track->frames_left--;
        return;
    }
    
    // Get next note
    const Note* current = &track->sequence[track->position];
    
    // Check for end of sequence
    if (current->freq == 0 && current->duration == 0) {
        // Loop back to beginning
        track->position = 0;
        current = &track->sequence[0];
    }
    
    // Play the note
    set_note(track->channel, current->freq, track->instrument);
    
    // Set duration (convert beats to frames)
    track->frames_left = current->duration * frames_per_beat;
    
    // Move to next note
    track->position++;
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void init_music(void)
{
    // Clear all music channels
    for (uint8_t i = 0; i < MUSIC_CHANNEL_COUNT; i++) {
        tracks[i].active = false;
        stop_note(MUSIC_CHANNEL_START + i);
    }
    
    music_playing = false;
    current_frame = 0;
}

void start_music(const Note* melody, const Note* bass, const Note* kick, const Note* hihat)
{
    // Initialize melody track on channel 4 (normal instrument - square wave)
    if (melody) {
        init_track(0, melody, MUSIC_CHANNEL_START, INSTRUMENT_NORMAL);
    }
    
    // Initialize bass track on channel 5 (normal instrument - triangle wave)
    if (bass) {
        init_track(1, bass, MUSIC_CHANNEL_START + 1, INSTRUMENT_NORMAL);
    }
    
    // Initialize kick drum track on channel 6
    if (kick) {
        init_track(2, kick, MUSIC_CHANNEL_START + 2, INSTRUMENT_KICK);
    }
    
    // Initialize hi-hat track on channel 7
    if (hihat) {
        init_track(3, hihat, MUSIC_CHANNEL_START + 3, INSTRUMENT_HIHAT);
    }
    
    // Calculate total duration of the loop (from melody track, or first non-null track)
    master_loop_frames = 0;
    const Note* reference_track = melody ? melody : (bass ? bass : (kick ? kick : hihat));
    if (reference_track) {
        const Note* note = reference_track;
        while (note->freq != 0 || note->duration != 0) {
            master_loop_frames += note->duration * frames_per_beat;
            note++;
        }
    }
    
    music_playing = true;
    current_frame = 0;
    
    // Immediately play the first notes of all tracks to ensure sync
    for (uint8_t i = 0; i < MUSIC_CHANNEL_COUNT; i++) {
        if (tracks[i].active && tracks[i].sequence) {
            const Note* first_note = &tracks[i].sequence[0];
            set_note(tracks[i].channel, first_note->freq, tracks[i].instrument);
            tracks[i].frames_left = first_note->duration * frames_per_beat;
            tracks[i].position = 1;  // Move to next note for subsequent updates
        }
    }
}

void start_title_music(void)
{
    start_music(NULL, title_bass, NULL, NULL);
}

void start_gameplay_music(void)
{
    start_music(NULL, title_bass, NULL, NULL);
}

void start_end_music(void)
{
    start_music(end_melody, end_bass, end_kick, end_hihat);
}

void stop_music(void)
{
    music_playing = false;
    
    // Stop all music channels
    for (uint8_t i = 0; i < MUSIC_CHANNEL_COUNT; i++) {
        tracks[i].active = false;
        stop_note(MUSIC_CHANNEL_START + i);
    }
}

void update_music(void)
{
    if (!music_playing) return;
    
    // Check if we've completed a full loop cycle
    if (current_frame >= master_loop_frames) {
        // Reset all tracks to beginning simultaneously
        current_frame = 0;
        for (uint8_t i = 0; i < MUSIC_CHANNEL_COUNT; i++) {
            if (tracks[i].active && tracks[i].sequence) {
                // Immediately play first note of the loop
                const Note* first_note = &tracks[i].sequence[0];
                set_note(tracks[i].channel, first_note->freq, tracks[i].instrument);
                tracks[i].frames_left = first_note->duration * frames_per_beat;
                tracks[i].position = 1;  // Move to next note for subsequent updates
            }
        }
        // Skip update_track this frame since we just started the notes
        current_frame++;
        return;
    }
    
    // Update all active tracks
    for (uint8_t i = 0; i < MUSIC_CHANNEL_COUNT; i++) {
        update_track(&tracks[i]);
    }
    
    current_frame++;
}

bool is_music_playing(void)
{
    return music_playing;
}

void increase_music_tempo(void)
{
    if (frames_per_beat > MIN_FRAMES_PER_BEAT) {
        frames_per_beat--;
    }
}

void reset_music_tempo(void)
{
    frames_per_beat = DEFAULT_FRAMES_PER_BEAT;
}
