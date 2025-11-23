/*
 * RPMegaFighter - A port of Mega Super Fighter Challenge to the RP6502
 * Based on the Sega Genesis game by Jason Rowe
 * 
 * Platform: RP6502 Picocomputer
 * Graphics: VGA Mode 3 (320x180 bitmap) + Mode 4 (sprites)
 */

#include <rp6502.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "usb_hid_keys.h"
#include "definitions.h"
#include "random.h"
#include "graphics.h"

// ============================================================================
// GAME CONSTANTS
// ============================================================================

// Screen dimensions
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       180
#define SCREEN_WIDTH_D2     160
#define SCREEN_HEIGHT_D2    90

// World/map dimensions  
#define MAP_SIZE            1024
#define MAP_SIZE_M1         1023
#define MAP_SIZE_D2         512
#define MAP_SIZE_NEG        -1024
#define MAP_SIZE_NEG_D2     -512

// Scroll boundaries
#define BOUNDARY_X          100
#define BOUNDARY_Y          80

// Player/Ship properties
#define SHIP_ROTATION_STEPS 24    // Number of rotation angles (matches sin/cos table)
#define SHIP_ROT_SPEED      3     // Frames between rotation updates

// Bullet properties
#define MAX_BULLETS         8
#define BULLET_COOLDOWN     8
#define MAX_EBULLETS        8     // Enemy bullets
#define EBULLET_COOLDOWN    8
#define INITIAL_EBULLET_COOLDOWN 10  // Starting cooldown for enemy bullets
#define MIN_EBULLET_COOLDOWN     1   // Minimum cooldown (difficulty cap)
#define EBULLET_COOLDOWN_DECREASE 2  // Decrease per level
#define MAX_SBULLETS        3     // Spread shot bullets
#define SBULLET_COOLDOWN    45

// Enemy fighter properties
#define MAX_FIGHTERS        30
#define FIGHTER_SPAWN_RATE  128   // Frames between fighter spawns
#define EFIRE_COOLDOWN_TIMER 16   // Frames a fighter must wait between shots

// Scoring
#define SCORE_TO_WIN        100
#define SCORE_BASIC_KILL    1
#define SCORE_MINE_KILL     5
#define SCORE_SHIELD_KILL   5
#define SCORE_MINE_HIT      -10

// ============================================================================
// GAME STRUCTURES
// ============================================================================

// Bullet structure
typedef struct {
    int16_t x, y;           // Position
    int16_t status;         // -1 = inactive, 0-23 = active with direction
    int16_t vx_rem, vy_rem; // Velocity remainder for sub-pixel movement
} Bullet;

// Fighter/Enemy structure  
typedef struct {
    int16_t x, y;           // Position
    int16_t vx, vy;         // Velocity
    int16_t vx_i, vy_i;     // Initial velocity (for randomization)
    int16_t vx_rem, vy_rem; // Velocity remainder
    int16_t status;         // 0 = dead, 1+ = alive (various states)
    int16_t dx, dy;         // Delta position for scrolling
    int16_t frame;          // Animation frame
    int16_t lx1, ly1;       // Line draw positions (for tractor beam effect)
    int16_t lx2, ly2;
} Fighter;

// ============================================================================
// SOUND SYSTEM - PSG
// ============================================================================

// PSG channel structure (8 bytes per channel)
typedef struct {
    uint16_t freq;          // Frequency (Hz * 3)
    uint8_t duty;           // Duty cycle 0-255
    uint8_t vol_attack;     // bits 7-4: volume, bits 3-0: attack rate
    uint8_t vol_decay;      // bits 7-4: volume, bits 3-0: decay rate  
    uint8_t wave_release;   // bits 7-4: waveform, bits 3-0: release rate
    uint8_t pan_gate;       // bits 7-1: pan, bit 0: gate
    uint8_t unused;         // Padding
} ria_psg_t;

// Waveforms
#define PSG_WAVE_SINE     0
#define PSG_WAVE_SQUARE   1
#define PSG_WAVE_SAWTOOTH 2
#define PSG_WAVE_TRIANGLE 3
#define PSG_WAVE_NOISE    4

// PSG memory location in XRAM
#define PSG_XRAM_ADDR 0x0100

// Sound effect types (for round-robin allocation)
#define SFX_TYPE_PLAYER_FIRE   0
#define SFX_TYPE_ENEMY_FIRE    1
#define SFX_TYPE_ENEMY_HIT     2
#define SFX_TYPE_PLAYER_HIT    3
#define SFX_TYPE_COUNT         4

// Channel allocation (2 channels per effect type for round-robin)
static uint8_t next_channel[SFX_TYPE_COUNT] = {0, 2, 4, 6};

// ============================================================================
// GLOBAL GAME STATE
// ============================================================================

// Player state
static int16_t player_x = SCREEN_WIDTH_D2;
static int16_t player_y = SCREEN_HEIGHT_D2;
static int16_t player_vx = 0, player_vy = 0;
static int16_t player_vx_applied = 0, player_vy_applied = 0;
static int16_t player_x_rem = 0, player_y_rem = 0;
static int16_t player_rotation = 0;         // 0 to SHIP_ROTATION_STEPS-1
static int16_t player_rotation_frame = 0;   // Frame counter for rotation speed
static int16_t player_thrust_x = 0;         // Momentum
static int16_t player_thrust_y = 0;
static int16_t player_thrust_delay = 0;
static int16_t player_thrust_count = 0;
static bool player_shield_active = false;
static bool player_boost_active = false;

// Scrolling
static int16_t scroll_dx = 0;
static int16_t scroll_dy = 0;
static int16_t world_offset_x = 0;  // Cumulative world offset from scrolling
static int16_t world_offset_y = 0;

// Scores and game state
static int16_t player_score = 0;
static int16_t enemy_score = 0;
static int16_t game_score = 0;     // Skill-based score
static int16_t game_level = 1;
static uint16_t game_frame = 0;    // Frame counter (0-59)
static bool game_over = false;

// Control mode (from SGDK version)
static uint8_t control_mode = 0;   // 0 = rotational, 1 = directional

// Bullet pools
static Bullet bullets[MAX_BULLETS];
static Bullet ebullets[MAX_EBULLETS];
static Bullet sbullets[MAX_SBULLETS];
static uint16_t bullet_cooldown = 0;
static uint16_t ebullet_cooldown = 0;
static uint16_t max_ebullet_cooldown = INITIAL_EBULLET_COOLDOWN;  // Decreases with level
static uint16_t sbullet_cooldown = 0;
static uint8_t current_bullet_index = 0;
static uint8_t current_ebullet_index = 0;
static uint8_t current_sbullet_index = 0;

// Fighter pool
static Fighter fighters[MAX_FIGHTERS];
static int16_t active_fighter_count = 0;
static int16_t fighter_speed_1 = 128;
static int16_t fighter_speed_2 = 256;

// Input state (keystates and start_button_pressed are in definitions.h)
static gamepad_t gamepad[GAMEPAD_COUNT];

// ============================================================================
// SINE/COSINE LOOKUP TABLES (24 steps for rotation)
// ============================================================================
// The lookup tables are defined in definitions.h:
// - sin_fix[25]: 255 * sin(theta) for 24 rotation angles (0-23) + wrap
// - cos_fix[25]: 255 * cos(theta) for 24 rotation angles (0-23) + wrap  
// - t2_fix4[25]: Affine transform offsets for sprite rotation
//
// These provide smooth rotation in 15-degree increments (360/24 = 15 degrees)
// Values are scaled by 255 for fixed-point math without floating point

// ============================================================================
// ROTATION UTILITIES
// ============================================================================

/**
 * Get velocity components for a given rotation angle
 * @param rotation Rotation index (0-23)
 * @param vx_out Pointer to store X velocity component
 * @param vy_out Pointer to store Y velocity component
 * 
 * Example: rotation 0 = pointing up (negative Y direction)
 *          rotation 6 = pointing right (positive X direction)
 *          rotation 12 = pointing down (positive Y direction)
 *          rotation 18 = pointing left (negative X direction)
 */
static inline void get_velocity_from_rotation(uint8_t rotation, int16_t* vx_out, int16_t* vy_out)
{
    // Ensure rotation is in valid range (0-23)
    rotation = rotation % SHIP_ROTATION_STEPS;
    
    // For velocity: pointing "up" (rotation 0) should give negative Y
    // sin_fix[0] = 0, cos_fix[0] = 255
    *vx_out = -sin_fix[rotation];  // Negative because screen X increases right
    *vy_out = -cos_fix[rotation];  // Negative because screen Y increases down
}

/**
 * Normalize rotation angle to valid range (0-23)
 */
static inline uint8_t normalize_rotation(int16_t rotation)
{
    while (rotation < 0) {
        rotation += SHIP_ROTATION_STEPS;
    }
    return rotation % SHIP_ROTATION_STEPS;
}

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * Initialize bullet pools - mark all bullets as inactive
 */
static void init_bullets(void)
{
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        bullets[i].status = -1;
        bullets[i].x = 0;
        bullets[i].y = 0;
        bullets[i].vx_rem = 0;
        bullets[i].vy_rem = 0;
    }
    
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        ebullets[i].status = -1;
        ebullets[i].x = 0;
        ebullets[i].y = 0;
        ebullets[i].vx_rem = 0;
        ebullets[i].vy_rem = 0;
    }
    
    for (uint8_t i = 0; i < MAX_SBULLETS; i++) {
        sbullets[i].status = -1;
    }
}

/**
 * Initialize fighter/enemy pools - based on SGDK implementation
 */
static void init_fighters(void)
{
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        fighters[i].vx_i = random(16, 256);  // Random speed component
        fighters[i].vy_i = random(16, 256);
        fighters[i].vx = 0;  // Will be set by AI
        fighters[i].vy = 0;
        fighters[i].status = 1;  // Active
        
        // Spawn offscreen relative to player starting position
        fighters[i].x = random(SCREEN_WIDTH_D2, SCREEN_WIDTH) + SCREEN_WIDTH + 144;
        fighters[i].y = random(SCREEN_HEIGHT_D2, SCREEN_HEIGHT) + SCREEN_HEIGHT + 104;
        
        // Randomize which side they spawn on
        if (random(0, 1)) {
            fighters[i].x = -fighters[i].x;
        }
        if (random(0, 1)) {
            fighters[i].y = -fighters[i].y;
        }
        
        fighters[i].vx_rem = 0;
        fighters[i].vy_rem = 0;
        fighters[i].dx = 0;
        fighters[i].dy = 0;
        fighters[i].frame = random(0, 1);  // Animation frame
    }
    active_fighter_count = MAX_FIGHTERS;
}

/**
 * Initialize star field for parallax scrolling background
 */
static void init_stars(void)
{
    for (uint8_t i = 0; i < NSTAR; i++) {
        star_x[i] = random(1, STARFIELD_X);
        // Keep stars away from HUD area (top 10 pixels)
        star_y[i] = random(11, STARFIELD_Y);
        star_colour[i] = random(1, 255);
        star_x_old[i] = star_x[i];
        star_y_old[i] = star_y[i];
    }
}

/**
 * Update and draw stars with parallax scrolling
 * @param dx Horizontal scroll amount
 * @param dy Vertical scroll amount
 */
static void draw_stars(int16_t dx, int16_t dy)
{
    for (uint8_t i = 0; i < NSTAR; i++) {
        // Clear previous star position
        if (star_x_old[i] > 0 && star_x_old[i] < SCREEN_WIDTH && 
            star_y_old[i] > 0 && star_y_old[i] < SCREEN_HEIGHT) {
            set(star_x_old[i], star_y_old[i], 0x00);
        }
        
        // Update star position based on scroll
        star_x[i] = star_x_old[i] - dx;
        if (star_x[i] <= 0) {
            star_x[i] += STARFIELD_X;
        }
        if (star_x[i] > STARFIELD_X) {
            star_x[i] -= STARFIELD_X;
        }
        star_x_old[i] = star_x[i];
        
        star_y[i] = star_y_old[i] - dy;
        if (star_y[i] <= 10) {
            // When wrapping from top, place at bottom minus HUD offset
            star_y[i] += (STARFIELD_Y - 10);
        }
        if (star_y[i] > STARFIELD_Y) {
            // When wrapping from bottom, place below HUD area
            star_y[i] = (star_y[i] - STARFIELD_Y) + 11;
        }
        star_y_old[i] = star_y[i];
        
        // Draw star at new position if on screen (avoid HUD area at top)
        if (star_x[i] > 0 && star_x[i] < SCREEN_WIDTH && 
            star_y[i] > 10 && star_y[i] < SCREEN_HEIGHT) {
            set(star_x[i], star_y[i], star_colour[i]);
        }
    }
}

/**
 * Initialize PSG (Programmable Sound Generator)
 */
static void init_psg(void)
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

/**
 * Play sound effect with round-robin channel allocation
 * @param sfx_type Sound effect type (0-3)
 * @param freq Frequency in Hz
 * @param wave Waveform (0=sine, 1=square, 2=saw, 3=tri, 4=noise)
 * @param attack Attack rate (0-15)
 * @param decay Decay rate (0-15)
 * @param release Release rate (0-15)
 * @param volume Volume (0-15, where 0=loud, 15=silent)
 */
static void play_sound(uint8_t sfx_type, uint16_t freq, uint8_t wave, 
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

/**
 * Initialize graphics system
 * Sets up bitmap mode for background and sprite mode for entities
 */
static void init_graphics(void)
{
    // Set up bitmap configuration for background (VGA Mode 3)
    BITMAP_CONFIG = VGA_CONFIG_START;
    
    // Set 320x180 canvas
    xregn(1, 0, 0, 1, 2);
    
    // Configure bitmap parameters
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, x_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, y_pos_px, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, width_px, 320);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, height_px, 180);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, xram_data_ptr, 0);
    xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, xram_palette_ptr, 0xFFFF);
    
    // Enable Mode 3 bitmap (4-bit color)
    xregn(1, 0, 1, 4, 3, 3, BITMAP_CONFIG, 1);
    
    // Set up player spacecraft sprite (VGA Mode 4 - affine sprite with rotation)
    SPACECRAFT_CONFIG = BITMAP_CONFIG + sizeof(vga_mode3_config_t);
    
    // Initialize rotation transform matrix (identity at rotation 0)
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[0],  cos_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[1], -sin_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[2],  t2_fix4[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[3],  sin_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[4],  cos_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[5],  t2_fix4[ri_max - player_rotation + 1]);
    
    // Set sprite position and properties
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, x_pos_px, player_x);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, y_pos_px, player_y);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, xram_sprite_ptr, SPACESHIP_DATA);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, log_size, 3);  // 8x8 sprite (2^3)
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, has_opacity_metadata, false);
    
    // Set up fighter sprites (VGA Mode 4 - regular sprites)
    FIGHTER_CONFIG = SPACECRAFT_CONFIG + sizeof(vga_mode4_asprite_t);
    
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        unsigned ptr = FIGHTER_CONFIG + i * sizeof(vga_mode4_sprite_t);
        
        // Initialize sprite configuration
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);  // Start offscreen
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, FIGHTER_DATA);
        xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, 2);  // 4x4 sprite (2^2)
        xram0_struct_set(ptr, vga_mode4_sprite_t, has_opacity_metadata, false);
    }
    
    // Set up enemy bullet sprites (VGA Mode 4 - regular sprites)
    EBULLET_CONFIG = FIGHTER_CONFIG + MAX_FIGHTERS * sizeof(vga_mode4_sprite_t);
    
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        unsigned ptr = EBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
        
        // Initialize sprite configuration  
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);  // Start offscreen
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, EBULLET_DATA);
        xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, 1);  // 2x2 sprite (2^1)
        xram0_struct_set(ptr, vga_mode4_sprite_t, has_opacity_metadata, false);  // Match fighter config
    }
    
    // Set up player bullet sprites (VGA Mode 4 - regular sprites)
    BULLET_CONFIG = EBULLET_CONFIG + MAX_EBULLETS * sizeof(vga_mode4_sprite_t);
    
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
        
        // Initialize sprite configuration  
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);  // Start offscreen
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, BULLET_DATA);
        xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, 1);  // 2x2 sprite (2^1)
        xram0_struct_set(ptr, vga_mode4_sprite_t, has_opacity_metadata, false);
    }
    
    // Enable sprite modes:
    // First enable affine sprites (player) - 1 sprite at SPACECRAFT_CONFIG
    xregn(1, 0, 1, 5, 4, 1, SPACECRAFT_CONFIG, 1, 2);
    // Then enable regular sprites (fighters + ebullets + bullets) - all regular sprites in one call
    xregn(1, 0, 1, 5, 4, 0, FIGHTER_CONFIG, MAX_FIGHTERS + MAX_EBULLETS + MAX_BULLETS, 1);
    
    // Clear bitmap memory
    RIA.addr0 = 0;
    RIA.step0 = 1;
    for (unsigned i = vlen; i--;) {
        RIA.rw0 = 0;
    }
    
    printf("Graphics initialized: 320x180 bitmap + player sprite\n");
}

/**
 * Initialize game state
 */
static void init_game(void)
{
    // Reset scores
    player_score = 0;
    enemy_score = 0;
    game_score = 0;
    game_level = 1;
    game_frame = 0;
    game_paused = false;
    game_over = false;
    
    // Reset player position and state
    player_x = SCREEN_WIDTH_D2;
    player_y = SCREEN_HEIGHT_D2;
    player_vx = 0;
    player_vy = 0;
    player_rotation = 0;
    player_thrust_x = 0;
    player_thrust_y = 0;
    player_shield_active = false;
    
    // Initialize entity pools
    init_bullets();
    init_fighters();
    init_stars();
    
    printf("Game initialized\n");
}

// ============================================================================
// PAUSE SCREEN
// ============================================================================

/**
 * Display or clear the pause message on screen
 * @param show_paused If true, draws "PAUSED", if false clears it
 */
static void display_pause_message(bool show_paused)
{
    const uint8_t pause_color = 0xFF;
    const uint16_t center_x = 120;
    const uint16_t center_y = 85;
    
    if (show_paused) {
        // Draw "PAUSED" using simple block letters
        // P
        for (uint16_t x = center_x; x < center_x + 3; x++) {
            for (uint16_t y = center_y; y < center_y + 12; y++) {
                set(x, y, pause_color);
            }
        }
        for (uint16_t x = center_x; x < center_x + 8; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 6, pause_color);
        }
        for (uint16_t y = center_y; y < center_y + 7; y++) {
            set(center_x + 8, y, pause_color);
        }
        
        // A
        for (uint16_t y = center_y + 3; y < center_y + 12; y++) {
            set(center_x + 12, y, pause_color);
            set(center_x + 20, y, pause_color);
        }
        for (uint16_t x = center_x + 12; x < center_x + 21; x++) {
            set(x, center_y + 3, pause_color);
            set(x, center_y + 7, pause_color);
        }
        
        // U
        for (uint16_t y = center_y; y < center_y + 12; y++) {
            set(center_x + 24, y, pause_color);
            set(center_x + 32, y, pause_color);
        }
        for (uint16_t x = center_x + 24; x < center_x + 33; x++) {
            set(x, center_y + 11, pause_color);
        }
        
        // S
        for (uint16_t x = center_x + 36; x < center_x + 44; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 6, pause_color);
            set(x, center_y + 11, pause_color);
        }
        for (uint16_t y = center_y; y < center_y + 7; y++) {
            set(center_x + 36, y, pause_color);
        }
        for (uint16_t y = center_y + 6; y < center_y + 12; y++) {
            set(center_x + 44, y, pause_color);
        }
        
        // E
        for (uint16_t y = center_y; y < center_y + 12; y++) {
            set(center_x + 48, y, pause_color);
        }
        for (uint16_t x = center_x + 48; x < center_x + 56; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 6, pause_color);
            set(x, center_y + 11, pause_color);
        }
        
        // D
        for (uint16_t y = center_y; y < center_y + 12; y++) {
            set(center_x + 60, y, pause_color);
        }
        for (uint16_t x = center_x + 60; x < center_x + 67; x++) {
            set(x, center_y, pause_color);
            set(x, center_y + 11, pause_color);
        }
        for (uint16_t y = center_y + 1; y < center_y + 11; y++) {
            set(center_x + 67, y, pause_color);
        }
    } else {
        // Clear PAUSED text by drawing black
        for (uint16_t x = center_x; x < center_x + 68; x++) {
            for (uint16_t y = center_y; y < center_y + 12; y++) {
                set(x, y, 0x00);
            }
        }
    }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

/**
 * Read keyboard and gamepad input
 */
static void handle_input(void)
{
    // Read keyboard state
    RIA.addr0 = KEYBOARD_INPUT;
    RIA.step0 = 2;
    keystates[0] = RIA.rw0;
    RIA.step0 = 1;
    keystates[2] = RIA.rw0;
    
    // Read gamepad data
    RIA.addr0 = GAMEPAD_INPUT;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
        gamepad[i].dpad = RIA.rw0;
        gamepad[i].sticks = RIA.rw0;
        gamepad[i].btn0 = RIA.rw0;
        gamepad[i].btn1 = RIA.rw0;
        gamepad[i].lx = RIA.rw0;
        gamepad[i].ly = RIA.rw0;
        gamepad[i].rx = RIA.rw0;
        gamepad[i].ry = RIA.rw0;
        gamepad[i].l2 = RIA.rw0;
        gamepad[i].r2 = RIA.rw0;
    }
    
    // Handle pause button (Start) - BTN1 bit 0x02
    if (gamepad[0].dpad & GP_CONNECTED) {
        if (gamepad[0].btn1 & 0x02) {  // START button = 0x02 in BTN1
            if (!start_button_pressed) {
                game_paused = !game_paused;
                display_pause_message(game_paused);
                start_button_pressed = true;
                printf("\nGame %s\n", game_paused ? "PAUSED" : "RESUMED");
            }
        } else {
            start_button_pressed = false;
        }
    }
}

// ============================================================================
// GAME LOGIC UPDATES
// ============================================================================

/**
 * Update player ship position, rotation, and physics
 */
static void update_player(void)
{
    // Handle player rotation (only update every SHIP_ROT_SPEED frames for smoother control)
    player_rotation_frame++;
    if (player_rotation_frame >= SHIP_ROT_SPEED) {
        player_rotation_frame = 0;
        
        // Check for rotation input (keyboard or gamepad)
        bool rotate_left = key(KEY_LEFT) || (gamepad[0].sticks & GP_LSTICK_LEFT);
        bool rotate_right = key(KEY_RIGHT) || (gamepad[0].sticks & GP_LSTICK_RIGHT);
        
        if (rotate_left) {
            player_rotation++;
            if (player_rotation >= SHIP_ROTATION_STEPS) {
                player_rotation = 0;
            }
        }
        if (rotate_right) {
            player_rotation--;
            if (player_rotation < 0) {
                player_rotation = SHIP_ROTATION_STEPS - 1;
            }
        }
    }
    
    // Handle thrust/acceleration
    bool thrust = key(KEY_UP) || (gamepad[0].sticks & GP_LSTICK_UP);
    
    if (thrust) {
        // Get velocity components based on current rotation
        int16_t thrust_vx, thrust_vy;
        get_velocity_from_rotation(player_rotation, &thrust_vx, &thrust_vy);
        
        // Apply thrust (scaled for game balance)
        player_vx = thrust_vx;
        player_vy = thrust_vy;
        
        // Reset momentum delay when actively thrusting
        player_thrust_delay = 0;
        
        // Accumulate momentum (scaled down to prevent too much speed)
        int16_t new_thrust_x = player_thrust_x + (thrust_vx >> 4);
        int16_t new_thrust_y = player_thrust_y + (thrust_vy >> 4);
        
        // Clamp momentum to prevent infinite acceleration
        if (new_thrust_x > -1024 && new_thrust_x < 1024) {
            player_thrust_x = new_thrust_x;
        }
        if (new_thrust_y > -1024 && new_thrust_y < 1024) {
            player_thrust_y = new_thrust_y;
        }
    } else {
        player_vx = 0;
        player_vy = 0;
    }
    
    // Apply momentum and friction
    int16_t total_vx = player_vx + player_thrust_x;
    int16_t total_vy = player_vy + player_thrust_y;
    
    // Calculate applied velocity with sub-pixel precision (divide by 512 for fixed-point)
    player_vx_applied = (total_vx + player_x_rem) >> 9;
    player_vy_applied = (total_vy + player_y_rem) >> 9;
    
    // Store remainder for next frame
    player_x_rem = total_vx + player_x_rem - (player_vx_applied << 9);
    player_y_rem = total_vy + player_y_rem - (player_vy_applied << 9);
    
    // Apply friction when not thrusting
    if (!thrust) {
        player_thrust_count++;
        if (player_thrust_count > 50 && player_thrust_delay < 8) {
            player_thrust_delay++;
            player_thrust_count = 0;
            
            // Apply friction by halving momentum
            if (player_vx == 0) {
                player_thrust_x >>= 1;
            }
            if (player_vy == 0) {
                player_thrust_y >>= 1;
            }
        }
        
        // Stop momentum after enough friction
        if (player_thrust_delay >= 8) {
            player_thrust_x = 0;
            player_thrust_y = 0;
        }
    }
    
    // Update player position
    int16_t new_x = player_x + player_vx_applied;
    int16_t new_y = player_y + player_vy_applied;
    
    // Handle screen boundaries (scrolling behavior)
    if (new_x > BOUNDARY_X && new_x < (SCREEN_WIDTH - BOUNDARY_X)) {
        player_x = new_x;
        scroll_dx = 0;
    } else {
        scroll_dx = new_x - player_x;
        world_offset_x += scroll_dx;
    }
    
    if (new_y > BOUNDARY_Y && new_y < (SCREEN_HEIGHT - BOUNDARY_Y)) {
        player_y = new_y;
        scroll_dy = 0;
    } else {
        scroll_dy = new_y - player_y;
        world_offset_y += scroll_dy;
    }
}

/**
 * Update all active bullets and check collisions
 */
static void update_bullets(void)
{
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].status < 0) {
            // Move sprite offscreen when inactive
            unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            continue;  // Bullet is inactive
        }
        
        // Check collision with fighters before moving
        for (uint8_t f = 0; f < MAX_FIGHTERS; f++) {
            if (fighters[f].status > 0) {
                // Convert fighter world position to screen position
                int16_t fighter_screen_x = fighters[f].x - world_offset_x;
                int16_t fighter_screen_y = fighters[f].y - world_offset_y;
                
                // Simple AABB collision: fighter sprite is 4x4 pixels, bullet is 2x2 pixels
                // Check if bullet overlaps with fighter (6x6 hit box for better detection)
                if (bullets[i].x >= fighter_screen_x - 2 && bullets[i].x < fighter_screen_x + 6 &&
                    bullets[i].y >= fighter_screen_y - 2 && bullets[i].y < fighter_screen_y + 6) {
                    
                    // Hit! Remove bullet and fighter
                    bullets[i].status = -1;
                    
                    // Play explosion sound effect (noise crash)
                    play_sound(SFX_TYPE_ENEMY_HIT, 80, PSG_WAVE_NOISE, 0, 2, 5, 1);
                    
                    // Clear tractor beam if fighter was attacking (status == 2)
                    if (fighters[f].status == 2) {
                        // TODO: Clear line at lx1, ly1, lx2, ly2
                    }
                    
                    // Set status to 0 to start respawn timer
                    fighters[f].status = 0;
                    active_fighter_count--;
                    
                    // Award points
                    player_score += SCORE_BASIC_KILL;
                    game_score += SCORE_BASIC_KILL;
                    
                    // Move bullet sprite offscreen
                    unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
                    xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
                    xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
                    
                    goto next_bullet;  // Skip rest of bullet update
                }
            }
        }
        
        // Get velocity components based on bullet direction
        int16_t bvx = -sin_fix[bullets[i].status];
        int16_t bvy = -cos_fix[bullets[i].status];
        
        // Apply velocity with fixed-point math (divide by 64 for bullet speed)
        int16_t bvx_applied = (bvx + bullets[i].vx_rem) >> 6;
        int16_t bvy_applied = (bvy + bullets[i].vy_rem) >> 6;
        
        // Update remainder
        bullets[i].vx_rem = bvx + bullets[i].vx_rem - (bvx_applied << 6);
        bullets[i].vy_rem = bvy + bullets[i].vy_rem - (bvy_applied << 6);
        
        // Update bullet position
        bullets[i].x += bvx_applied;
        bullets[i].y += bvy_applied;
        
        // Check if bullet is still on screen
        if (bullets[i].x > 0 && bullets[i].x < SCREEN_WIDTH && 
            bullets[i].y > 0 && bullets[i].y < SCREEN_HEIGHT) {
            // Update sprite hardware position
            unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, bullets[i].x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, bullets[i].y);
        } else {
            // Bullet went off screen, deactivate it
            bullets[i].status = -1;
            // Move sprite offscreen
            unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        }
        
    next_bullet:
        continue;
    }
}

/**
 * Update all active enemy fighters - based on MySegaGame SGDK implementation
 * Fighters chase the player and fire bullets
 */
static void update_fighters(void)
{
    int16_t fvx_applied, fvy_applied;
    
    // Calculate player's world position
    int16_t player_world_x = player_x + world_offset_x;
    int16_t player_world_y = player_y + world_offset_y;
    
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        // Handle respawning dead fighters
        if (fighters[i].status <= 0) {
            // Respawn counter (counts down to -FIGHTER_SPAWN_RATE)
            fighters[i].status--;
            if (fighters[i].status <= -FIGHTER_SPAWN_RATE) {
                // Respawn fighter off-screen relative to current viewport
                fighters[i].vx_i = random(16, 256);
                fighters[i].vy_i = random(16, 256);
                
                // Choose random edge: 0=right, 1=left, 2=bottom, 3=top
                uint8_t edge = random(0, 3);
                
                if (edge == 0) {
                    // Spawn off right edge
                    fighters[i].x = world_offset_x + SCREEN_WIDTH + random(20, 100);
                    fighters[i].y = world_offset_y + random(20, SCREEN_HEIGHT - 20);
                } else if (edge == 1) {
                    // Spawn off left edge
                    fighters[i].x = world_offset_x - random(20, 100);
                    fighters[i].y = world_offset_y + random(20, SCREEN_HEIGHT - 20);
                } else if (edge == 2) {
                    // Spawn off bottom edge
                    fighters[i].x = world_offset_x + random(20, SCREEN_WIDTH - 20);
                    fighters[i].y = world_offset_y + SCREEN_HEIGHT + random(20, 100);
                } else {
                    // Spawn off top edge
                    fighters[i].x = world_offset_x + random(20, SCREEN_WIDTH - 20);
                    fighters[i].y = world_offset_y - random(20, 100);
                }
                
                fighters[i].status = 1;
                active_fighter_count++;
            }
            continue;
        }
        
        // Check collision with player (both in screen coordinates)
        int16_t fighter_screen_x = fighters[i].x - world_offset_x;
        int16_t fighter_screen_y = fighters[i].y - world_offset_y;
        
        // 4x4 fighter vs 8x8 player sprite collision
        if (fighter_screen_x + 4 > player_x && fighter_screen_x < player_x + 8 &&
            fighter_screen_y + 4 > player_y && fighter_screen_y < player_y + 8) {
            // Collision! Fighter dies, player takes damage
            fighters[i].status = 0;
            active_fighter_count--;
            enemy_score++;  // Enemy gets a point for hitting player
            
            // Play deep crash sound effect
            play_sound(SFX_TYPE_PLAYER_HIT, 60, PSG_WAVE_NOISE, 0, 1, 6, 0);
            
            continue;
        }
        
        // Update velocity every 30 frames using AI decision
        if (game_frame == 0) {
            // Calculate distance to player (in world coordinates)
            int16_t fdx = player_world_x - fighters[i].x;
            int16_t fdy = player_world_y - fighters[i].y;
            
            // Chase player in X direction (always)
            if (fdx > 0) {
                fighters[i].vx = fighters[i].vx_i;  // Move toward player
            } else if (fdx < 0) {
                fighters[i].vx = -fighters[i].vx_i;
            } else {
                fighters[i].vx = 0;
            }
            
            // Chase player in Y direction (always)
            if (fdy > 0) {
                fighters[i].vy = fighters[i].vy_i;  // Move toward player
            } else if (fdy < 0) {
                fighters[i].vy = -fighters[i].vy_i;
            } else {
                fighters[i].vy = 0;
            }
        }
        
        // Apply velocity with fixed-point math (divide by 256 using fighter_speed_1 = 8)
        fvx_applied = (fighters[i].vx + fighters[i].vx_rem) >> 8;
        fvy_applied = (fighters[i].vy + fighters[i].vy_rem) >> 8;
        
        fighters[i].vx_rem = fighters[i].vx + fighters[i].vx_rem - (fvx_applied << 8);
        fighters[i].vy_rem = fighters[i].vy + fighters[i].vy_rem - (fvy_applied << 8);
        
        fighters[i].dx = fvx_applied;
        fighters[i].dy = fvy_applied;
        
        // Update position
        fighters[i].x += fvx_applied;
        fighters[i].y += fvy_applied;
        
        // Wrap around relative to viewport - if fighter goes too far off one edge, wrap to opposite edge
        int16_t fighter_offset_x = fighters[i].x - world_offset_x;
        int16_t fighter_offset_y = fighters[i].y - world_offset_y;
        
        if (fighter_offset_x > SCREEN_WIDTH + 200) {
            fighters[i].x = world_offset_x - 150;
        } else if (fighter_offset_x < -200) {
            fighters[i].x = world_offset_x + SCREEN_WIDTH + 150;
        }
        
        if (fighter_offset_y > SCREEN_HEIGHT + 200) {
            fighters[i].y = world_offset_y - 150;
        } else if (fighter_offset_y < -200) {
            fighters[i].y = world_offset_y + SCREEN_HEIGHT + 150;
        }
    }
}

/**
 * Fire a bullet from the player ship
 */
static void fire_bullet(void)
{
    if (bullet_cooldown > 0) {
        return;
    }
    
    if (bullets[current_bullet_index].status < 0) {
        bullets[current_bullet_index].status = player_rotation;
        bullets[current_bullet_index].x = player_x + 4;
        bullets[current_bullet_index].y = player_y + 4;
        bullets[current_bullet_index].vx_rem = 0;
        bullets[current_bullet_index].vy_rem = 0;
        
        // Set sprite hardware position immediately
        unsigned ptr = BULLET_CONFIG + current_bullet_index * sizeof(vga_mode4_sprite_t);
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, bullets[current_bullet_index].x);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, bullets[current_bullet_index].y);
        
        // Play deep tone sound effect
        play_sound(SFX_TYPE_PLAYER_FIRE, 110, PSG_WAVE_SQUARE, 0, 3, 4, 0);
        
        current_bullet_index++;
        if (current_bullet_index >= MAX_BULLETS) {
            current_bullet_index = 0;
        }
        
        bullet_cooldown = BULLET_COOLDOWN;
    }
}

/**
 * Fire enemy bullet - based on SGDK implementation
 * Fighters fire bullets at predicted player position
 */
static void fire_ebullet(void)
{
    if (ebullet_cooldown > 0) {
        return;
    }
    
    ebullet_cooldown = max_ebullet_cooldown;
    
    // Calculate player's world position
    int16_t player_world_x = player_x + world_offset_x;
    int16_t player_world_y = player_y + world_offset_y;
    
    if (ebullets[current_ebullet_index].status < 0) {
        // Find a fighter that can fire
        for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
            if (fighters[i].status == 1) {
                // Check if fighter is on screen
                int16_t screen_x = fighters[i].x - world_offset_x;
                int16_t screen_y = fighters[i].y - world_offset_y;
                
                // Only fire if fighter is actually visible on screen (not at edge)
                if (screen_x > 0 && screen_x < SCREEN_WIDTH - 4 &&
                    screen_y > 0 && screen_y < SCREEN_HEIGHT - 4) {
                    
                    // Calculate distance to player (in world coordinates)
                    int16_t fdx = player_world_x - fighters[i].x;
                    int16_t fdy = -(player_world_y - fighters[i].y);  // Flip Y
                    int16_t distance = abs(fdx) + abs(fdy);
                    
                    if (distance > 0) {
                        // Predict player position (time to impact)
                        int16_t tti_frames = distance / 4;  // ENEMY_BULLET_SPEED = 4
                        if (tti_frames == 0) tti_frames = 1;
                        
                        int16_t pre_player_x = player_world_x + 4 + (player_vx_applied * tti_frames);
                        int16_t pre_player_y = player_world_y + 4 + (player_vy_applied * tti_frames);
                        
                        // Recalculate to predicted position
                        fdx = pre_player_x - fighters[i].x;
                        fdy = -pre_player_y + fighters[i].y;
                        
                        // Find best rotation angle using dot product
                        int16_t best_index = 0;
                        int32_t max_dot = -8388608;  // Very negative number
                        
                        for (uint8_t j = 0; j < SHIP_ROTATION_STEPS; j++) {
                            int32_t current_dot = (int32_t)fdx * cos_fix[j] + (int32_t)fdy * sin_fix[j];
                            if (current_dot > max_dot) {
                                max_dot = current_dot;
                                best_index = j;
                            }
                        }
                        
                        // Fire the bullet (spawn at fighter's screen position)
                        ebullets[current_ebullet_index].status = best_index;
                        ebullets[current_ebullet_index].x = screen_x;
                        ebullets[current_ebullet_index].y = screen_y;
                        ebullets[current_ebullet_index].vx_rem = 0;
                        ebullets[current_ebullet_index].vy_rem = 0;
                        
                        // Update sprite hardware position immediately
                        unsigned bullet_ptr = EBULLET_CONFIG + current_ebullet_index * sizeof(vga_mode4_sprite_t);
                        xram0_struct_set(bullet_ptr, vga_mode4_sprite_t, x_pos_px, screen_x);
                        xram0_struct_set(bullet_ptr, vga_mode4_sprite_t, y_pos_px, screen_y);
                        
                        // Play higher pitched enemy fire sound
                        play_sound(SFX_TYPE_ENEMY_FIRE, 440, PSG_WAVE_TRIANGLE, 0, 4, 3, 2);
                        
                        // Set fighter to cooldown state
                        fighters[i].status = 2;
                        
                        current_ebullet_index++;
                        if (current_ebullet_index >= MAX_EBULLETS) {
                            current_ebullet_index = 0;
                        }
                        
                        break;
                    }
                }
            } else if (fighters[i].status > 1) {
                // Cooldown counter
                fighters[i].status++;
                if (fighters[i].status > EFIRE_COOLDOWN_TIMER) {
                    fighters[i].status = 1;  // Ready to fire again
                }
            }
        }
    }
}

/**
 * Update enemy bullets - based on SGDK implementation
 */
static void update_ebullets(void)
{
    // Adjust all active ebullets for scrolling
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        if (ebullets[i].status >= 0) {
            ebullets[i].x -= scroll_dx;
            ebullets[i].y -= scroll_dy;
        }
    }
    
    for (uint8_t i = 0; i < MAX_EBULLETS; i++) {
        unsigned ptr = EBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
        
        if (ebullets[i].status < 0) {
            // Move sprite offscreen if inactive
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            continue;
        }
        
        // Check collision with player
        if (player_x < ebullets[i].x + 2 &&
            player_x + 8 > ebullets[i].x &&
            player_y < ebullets[i].y + 2 &&
            player_y + 8 > ebullets[i].y) {
            
            // Hit! Remove bullet and damage player
            ebullets[i].status = -1;
            
            // Award point to enemy
            enemy_score++;
            
            // Move sprite offscreen
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            
            continue;
        }
        
        // Get velocity from stored direction
        int16_t bvx = cos_fix[ebullets[i].status];
        int16_t bvy = -sin_fix[ebullets[i].status];
        
        // Apply velocity (divide by 64 for speed = 4)
        int16_t bvx_applied = (bvx + ebullets[i].vx_rem) >> 6;
        int16_t bvy_applied = (bvy + ebullets[i].vy_rem) >> 6;
        
        ebullets[i].vx_rem = bvx + ebullets[i].vx_rem - (bvx_applied << 6);
        ebullets[i].vy_rem = bvy + ebullets[i].vy_rem - (bvy_applied << 6);
        
        // Update position
        ebullets[i].x += bvx_applied;
        ebullets[i].y += bvy_applied;
        
        // Check if still on screen
        if (ebullets[i].x > -10 && ebullets[i].x < SCREEN_WIDTH + 10 &&
            ebullets[i].y > -10 && ebullets[i].y < SCREEN_HEIGHT + 10) {
            // Update sprite position
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, ebullets[i].x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, ebullets[i].y);
        } else {
            // Bullet went offscreen, deactivate
            ebullets[i].status = -1;
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        }
    }
}

// ============================================================================
// RENDERING
// ============================================================================

/**
 * Draw a simple character at position (x, y)
 */
static void draw_char(int16_t x, int16_t y, char c, uint8_t color)
{
    // Simple 3x5 font for uppercase letters and digits
    // Each byte represents a row: bit 2 = left, bit 1 = middle, bit 0 = right
    static const uint8_t font[][5] = {
        // Digits 0-9
        {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
        {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
        {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
        {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
        {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
        {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
        {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
        {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
        {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
        {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
        // Letters (A=10, B=11, etc.)
        {0b111, 0b101, 0b111, 0b101, 0b101}, // A
        {0b110, 0b101, 0b110, 0b101, 0b110}, // B
        {0b111, 0b100, 0b100, 0b100, 0b111}, // C
        {0b110, 0b101, 0b101, 0b101, 0b110}, // D
        {0b111, 0b100, 0b110, 0b100, 0b111}, // E
        {0b111, 0b100, 0b110, 0b100, 0b100}, // F
        {0b111, 0b100, 0b101, 0b101, 0b111}, // G
        {0b101, 0b101, 0b111, 0b101, 0b101}, // H
        {0b111, 0b010, 0b010, 0b010, 0b111}, // I
        {0b001, 0b001, 0b001, 0b101, 0b111}, // J
        {0b101, 0b110, 0b100, 0b110, 0b101}, // K
        {0b100, 0b100, 0b100, 0b100, 0b111}, // L
        {0b101, 0b111, 0b111, 0b101, 0b101}, // M
        {0b101, 0b111, 0b111, 0b111, 0b101}, // N
        {0b111, 0b101, 0b101, 0b101, 0b111}, // O
        {0b111, 0b101, 0b111, 0b100, 0b100}, // P
        {0b111, 0b101, 0b101, 0b111, 0b011}, // Q
        {0b111, 0b101, 0b110, 0b110, 0b101}, // R
        {0b111, 0b100, 0b111, 0b001, 0b111}, // S
        {0b111, 0b010, 0b010, 0b010, 0b010}, // T
        {0b101, 0b101, 0b101, 0b101, 0b111}, // U
        {0b101, 0b101, 0b101, 0b101, 0b010}, // V
        {0b101, 0b101, 0b111, 0b111, 0b101}, // W
        {0b101, 0b101, 0b010, 0b101, 0b101}, // X
        {0b101, 0b101, 0b010, 0b010, 0b010}, // Y
        {0b111, 0b001, 0b010, 0b100, 0b111}, // Z
    };
    
    int idx = -1;
    if (c >= '0' && c <= '9') {
        idx = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        idx = 10 + (c - 'A');
    } else if (c >= 'a' && c <= 'z') {
        idx = 10 + (c - 'a');
    }
    
    if (idx >= 0 && idx < 36) {
        for (int row = 0; row < 5; row++) {
            uint8_t pattern = font[idx][row];
            for (int col = 0; col < 3; col++) {
                if (pattern & (1 << (2 - col))) {
                    set(x + col, y + row, color);
                }
            }
        }
    }
}

/**
 * Draw a string at position (x, y)
 */
static void draw_text(int16_t x, int16_t y, const char* text, uint8_t color)
{
    int16_t offset = 0;
    while (*text) {
        if (*text == ' ') {
            offset += 4;
        } else {
            draw_char(x + offset, y, *text, color);
            offset += 4;
        }
        text++;
    }
}

/**
 * Draw a horizontal bar (health/score meter) - fills left to right
 */
static void draw_bar(int16_t x, int16_t y, int16_t width, int16_t value, int16_t max_value, uint8_t fill_color, uint8_t bg_color)
{
    int16_t filled_width = (value * width) / max_value;
    if (filled_width > width) filled_width = width;
    
    // Draw filled portion
    for (int16_t i = 0; i < filled_width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, fill_color);
        }
    }
    
    // Draw empty portion
    for (int16_t i = filled_width; i < width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, bg_color);
        }
    }
}

/**
 * Draw a horizontal bar (health/score meter) - fills right to left
 */
static void draw_bar_rtl(int16_t x, int16_t y, int16_t width, int16_t value, int16_t max_value, uint8_t fill_color, uint8_t bg_color)
{
    int16_t filled_width = (value * width) / max_value;
    if (filled_width > width) filled_width = width;
    
    // Draw empty portion (on the left)
    for (int16_t i = 0; i < width - filled_width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, bg_color);
        }
    }
    
    // Draw filled portion (on the right)
    for (int16_t i = width - filled_width; i < width; i++) {
        for (int16_t j = 0; j < 5; j++) {
            set(x + i, y + j, fill_color);
        }
    }
}

/**
 * Clear a rectangular area
 */
static void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    for (int16_t dy = 0; dy < height; dy++) {
        for (int16_t dx = 0; dx < width; dx++) {
            set(x + dx, y + dy, 0x00);
        }
    }
}

/**
 * Draw the HUD (score, health, etc.)
 */
static void draw_hud(void)
{
    static int16_t prev_player_score = -1;
    static int16_t prev_enemy_score = -1;
    static int16_t prev_game_score = -1;
    static int16_t prev_game_level = -1;
    
    const uint8_t hud_y = 2;
    const uint8_t text_color = 0xFF;
    const uint8_t player_bar_color = 0x1F;  // Blue
    const uint8_t enemy_bar_color = 0x03;   // Pure red (RGB: R=3 bits, G=3 bits, B=2 bits)
    const uint8_t bar_bg_color = 0x08;      // Dark gray
    
    // Check if anything changed
    if (prev_player_score == player_score && 
        prev_enemy_score == enemy_score && 
        prev_game_score == game_score &&
        prev_game_level == game_level) {
        return;  // No changes, skip update
    }
    
    // Update previous values
    prev_player_score = player_score;
    prev_enemy_score = enemy_score;
    prev_game_score = game_score;
    prev_game_level = game_level;
    
    // Format with longer bars for better spacing
    
    // Draw static text (only on first draw)
    static bool first_draw = true;
    if (first_draw) {
        draw_text(5, hud_y, "YOU", text_color);
        draw_text(290, hud_y, "THEM", text_color);
        first_draw = false;
    }
    
    // Clear and draw player score (3 digits)
    clear_rect(30, hud_y, 12, 5);
    char score_buf[4];
    score_buf[0] = '0' + (player_score / 100) % 10;
    score_buf[1] = '0' + (player_score / 10) % 10;
    score_buf[2] = '0' + player_score % 10;
    score_buf[3] = '\0';
    draw_text(30, hud_y, score_buf, text_color);
    
    // Draw BAR1 (player progress bar) - longer bar (60 pixels)
    draw_bar(55, hud_y, 80, player_score, SCORE_TO_WIN, player_bar_color, bar_bg_color);
    
    // Clear and draw game score (5 digits)
    clear_rect(145, hud_y, 20, 5);
    char game_score_buf[6];
    game_score_buf[0] = '0' + (game_score / 10000) % 10;
    game_score_buf[1] = '0' + (game_score / 1000) % 10;
    game_score_buf[2] = '0' + (game_score / 100) % 10;
    game_score_buf[3] = '0' + (game_score / 10) % 10;
    game_score_buf[4] = '0' + game_score % 10;
    game_score_buf[5] = '\0';
    draw_text(145, hud_y, game_score_buf, text_color);
    
    // Draw BAR2 (enemy progress bar) - fills right to left
    draw_bar_rtl(175, hud_y, 80, enemy_score, SCORE_TO_WIN, enemy_bar_color, bar_bg_color);
    
    // Clear and draw enemy score (3 digits)
    clear_rect(270, hud_y, 12, 5);
    score_buf[0] = '0' + (enemy_score / 100) % 10;
    score_buf[1] = '0' + (enemy_score / 10) % 10;
    score_buf[2] = '0' + enemy_score % 10;
    draw_text(270, hud_y, score_buf, text_color);
    
    // Draw level indicator at bottom of screen
    const uint16_t level_y = 170;
    
    // Clear level area before drawing (wider to cover both text and number)
    clear_rect(10, level_y, 50, 5);
    
    draw_text(10, level_y, "LEVEL", text_color);
    char level_buf[3];
    level_buf[0] = '0' + (game_level / 10) % 10;
    level_buf[1] = '0' + game_level % 10;
    level_buf[2] = '\0';
    draw_text(50, level_y, level_buf, text_color);
}

/**
 * Update player sprite position and rotation
 */
static void update_player_sprite(void)
{
    // Update sprite position (offset 12-15 in config structure)
    RIA.step0 = sizeof(vga_mode4_asprite_t);
    RIA.step1 = sizeof(vga_mode4_asprite_t);
    RIA.addr0 = SPACECRAFT_CONFIG + 12;
    RIA.addr1 = SPACECRAFT_CONFIG + 13;
    
    // Write X position (16-bit)
    RIA.rw0 = player_x & 0xFF;
    RIA.rw1 = (player_x >> 8) & 0xFF;
    
    // Write Y position (16-bit)
    RIA.addr0 = SPACECRAFT_CONFIG + 14;
    RIA.addr1 = SPACECRAFT_CONFIG + 15;
    RIA.rw0 = player_y & 0xFF;
    RIA.rw1 = (player_y >> 8) & 0xFF;
    
    // Update rotation transform matrix
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[0],  cos_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[1], -sin_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[2],  t2_fix4[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[3],  sin_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[4],  cos_fix[player_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[5],  t2_fix4[ri_max - player_rotation + 1]);
}

/**
 * Render all game entities
 */
static void render_game(void)
{
    // Draw scrolling star background
    draw_stars(scroll_dx, scroll_dy);
    
    // Update fighter sprite positions
    for (uint8_t i = 0; i < MAX_FIGHTERS; i++) {
        if (fighters[i].status > 0) {
            // Fighters are in world coordinates
            // Player screen position + world offset = player world position
            // Fighter screen position = fighter world position - player world position + player screen position
            int16_t screen_x = fighters[i].x - world_offset_x;
            int16_t screen_y = fighters[i].y - world_offset_y;
            
            // Update sprite position
            unsigned ptr = FIGHTER_CONFIG + i * sizeof(vga_mode4_sprite_t);
            
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, screen_x);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, screen_y);
        } else {
            // Move offscreen if inactive
            unsigned ptr = FIGHTER_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        }
    }
    
    // Bullets are drawn in update_bullets() and update_ebullets()
    
    // Update player sprite on screen
    update_player_sprite();
}

// ============================================================================
// GAME SCREENS
// ============================================================================

/**
 * Display level up message and wait briefly
 */
static void show_level_up(void)
{
    const uint8_t blue_color = 0x1F;
    const uint16_t center_x = 120;
    const uint16_t center_y = 80;
    
    // Draw "LEVEL UP" message
    draw_text(center_x, center_y, "LEVEL UP", blue_color);
    
    printf("\n*** LEVEL UP! Now on level %d ***\n", game_level);
    
    // Wait for 2 seconds (120 frames at 60 Hz)
    uint8_t vsync_last = RIA.vsync;
    uint16_t wait_frames = 0;
    while (wait_frames < 120) {
        if (RIA.vsync != vsync_last) {
            vsync_last = RIA.vsync;
            wait_frames++;
        }
    }
    
    // Clear the message
    clear_rect(center_x, center_y, 80, 5);
}

/**
 * Display game over screen and wait for fire button
 */
static void show_game_over(void)
{
    const uint8_t red_color = 0x03;
    const uint16_t center_x = 100;
    
    // Draw "GAME OVER" message
    draw_text(center_x, 70, "GAME OVER", red_color);
    draw_text(center_x - 30, 90, "HIT FIRE TO CONTINUE", red_color);
    
    printf("\n*** GAME OVER ***\n");
    printf("Final Level: %d\n", game_level);
    printf("Final Score: %d\n", game_score);
    
    uint8_t vsync_last = RIA.vsync;
    bool fire_button_released = false;
    
    // Wait for fire button
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        // Read gamepad input
        RIA.addr0 = GAMEPAD_INPUT;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
            gamepad[i].dpad = RIA.rw0;
            gamepad[i].sticks = RIA.rw0;
            gamepad[i].btn0 = RIA.rw0;
            gamepad[i].btn1 = RIA.rw0;
            gamepad[i].lx = RIA.rw0;
            gamepad[i].ly = RIA.rw0;
            gamepad[i].rx = RIA.rw0;
            gamepad[i].ry = RIA.rw0;
            gamepad[i].l2 = RIA.rw0;
            gamepad[i].r2 = RIA.rw0;
        }
        
        // Read keyboard
        RIA.addr0 = KEYBOARD_INPUT;
        RIA.step0 = 2;
        keystates[0] = RIA.rw0;
        RIA.step0 = 1;
        keystates[2] = RIA.rw0;
        
        // Check if fire button is released first
        bool fire_pressed = key(KEY_SPACE) ||
                           (gamepad[0].btn0 & 0x04) ||  // A
                           (gamepad[0].btn0 & 0x02) ||  // B
                           (gamepad[0].btn0 & 0x20);    // C
        
        if (!fire_pressed) {
            fire_button_released = true;
        } else if (fire_button_released) {
            // Fire button pressed after being released
            printf("Fire button pressed - continuing...\n");
            return;
        }
        
        // Check for ESC to exit
        if (key(KEY_ESC)) {
            printf("ESC pressed - exiting...\n");
            exit(0);
        }
    }
}

// ============================================================================
// TITLE SCREEN
// ============================================================================

/**
 * Display title screen and wait for START button
 */
static void show_title_screen(void)
{
    const uint8_t red_color = 0x03;      // Pure red
    const uint8_t blue_color = 0x1F;     // Blue
    const uint16_t center_x = 110;       // X position for centered text
    
    // Clear screen
    RIA.addr0 = 0;
    RIA.step0 = 1;
    for (unsigned i = vlen; i--;) {
        RIA.rw0 = 0;
    }
    
    // Draw title text
    draw_text(center_x, 40, "MEGA", red_color);
    draw_text(center_x, 55, "SUPER", red_color);
    draw_text(center_x, 70, "FIGHTER", red_color);
    draw_text(center_x, 85, "CHALLENGE", blue_color);
    
    uint8_t vsync_last = RIA.vsync;
    uint16_t flash_counter = 0;
    bool press_start_visible = true;
    const uint16_t flash_interval = 30;  // 0.5 seconds at 60 Hz
    uint8_t current_color = red_color;
    
    // Draw initial "PRESS START" text
    draw_text(center_x - 20, 110, "PRESS START", red_color);
    
    printf("Title screen displayed. Press START to begin...\n");
    
    // Title screen loop - wait for START button
    bool start_button_was_pressed = false;  // Track button state for edge detection
    while (true) {
        // Wait for vertical sync
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        // Read input
        RIA.addr0 = KEYBOARD_INPUT;
        RIA.step0 = 2;
        keystates[0] = RIA.rw0;
        RIA.step0 = 1;
        keystates[2] = RIA.rw0;
        
        RIA.addr0 = GAMEPAD_INPUT;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
            gamepad[i].dpad = RIA.rw0;
            gamepad[i].sticks = RIA.rw0;
            gamepad[i].btn0 = RIA.rw0;
            gamepad[i].btn1 = RIA.rw0;
            gamepad[i].lx = RIA.rw0;
            gamepad[i].ly = RIA.rw0;
            gamepad[i].rx = RIA.rw0;
            gamepad[i].ry = RIA.rw0;
            gamepad[i].l2 = RIA.rw0;
            gamepad[i].r2 = RIA.rw0;
        }
        
        // Check for START button (BTN1 bit 0x02)
        if (gamepad[0].dpad & GP_CONNECTED) {
            if (gamepad[0].btn1 & 0x02) {
                // Button is currently pressed
                if (!start_button_was_pressed) {
                    // This is a new press (edge detection)
                    start_button_was_pressed = true;
                    // Clear entire screen before exiting
                    RIA.addr0 = 0;
                    RIA.step0 = 1;
                    for (unsigned i = vlen; i--;) {
                        RIA.rw0 = 0;
                    }
                    printf("START pressed - beginning game!\n");
                    return;  // Exit title screen
                }
            } else {
                // Button is not pressed
                start_button_was_pressed = false;
            }
        }
        
        // Check for ESC to exit game
        if (key(KEY_ESC)) {
            printf("ESC pressed - exiting...\n");
            exit(0);
        }
        
        // Flash "PRESS START" every 5 seconds
        flash_counter++;
        if (flash_counter >= flash_interval) {
            flash_counter = 0;
            press_start_visible = !press_start_visible;
            
            if (press_start_visible) {
                // Alternate color between red and blue
                current_color = (current_color == red_color) ? blue_color : red_color;
                draw_text(center_x - 20, 110, "PRESS START", current_color);
            } else {
                // Clear the text
                clear_rect(center_x - 20, 110, 120, 5);
            }
        }
    }
}

// ============================================================================
// MAIN GAME LOOP
// ============================================================================

int main(void)
{
    printf("\n=== RPMegaFighter ===\n");
    printf("Port of Mega Super Fighter Challenge to RP6502\n\n");
    
    // Initialize systems
    init_graphics();
    init_psg();
    init_game();
    
    // Enable keyboard input
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    
    // Enable gamepad input
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);
    
    printf("\nControls:\n");
    printf("  Keyboard: Arrow keys to rotate/thrust, SPACE to fire\n");
    printf("  Gamepad:  Left stick to rotate/thrust, A/B to fire\n");
    printf("  ESC to quit, START to pause\n\n");
    
    // Show title screen and wait for START
    show_title_screen();
    
    printf("Starting game loop...\n\n");
    
    uint8_t vsync_last = RIA.vsync;
    
    // Main game loop
    while (!game_over) {
        // Wait for vertical sync (60 Hz)
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        // Read input
        handle_input();
        
        // Check for ESC key to exit
        if (key(KEY_ESC)) {
            printf("Exiting game...\n");
            break;
        }
        
        // Skip updates if paused
        if (game_paused) {
            // Check for A+C buttons pressed together to exit (0x04 + 0x20 = 0x24)
            if ((gamepad[0].btn0 & 0x04) && (gamepad[0].btn0 & 0x20)) {
                printf("\nA+C pressed - Exiting game...\n");
                break;
            }
            continue;
        }
        
            // Update cooldown timers
        if (bullet_cooldown > 0) bullet_cooldown--;
        if (ebullet_cooldown > 0) ebullet_cooldown--;
        if (sbullet_cooldown > 0) sbullet_cooldown--;
        
        // Enemy bullet system
        fire_ebullet();
        
        // Handle player fire button (keyboard SPACE or gamepad A/B/C)
        // A=0x04, B=0x02, C=0x20
        if (key(KEY_SPACE) || 
            (gamepad[0].btn0 & 0x04) ||  // A button
            (gamepad[0].btn0 & 0x02) ||  // B button
            (gamepad[0].btn0 & 0x20)) {  // C button
            fire_bullet();
        }
        
            // Update game logic
            update_player();
            update_fighters();
            update_bullets();
            update_ebullets();
        
        // Render frame
        render_game();
        draw_hud();
        
        // Increment frame counter
        game_frame++;
        if (game_frame >= 60) {
            game_frame = 0;
        }
        
        // Check win/lose conditions
        if (player_score >= SCORE_TO_WIN) {
            // Player wins this round - level up!
            game_level++;
            
            // Increase difficulty by reducing enemy bullet cooldown
            max_ebullet_cooldown -= EBULLET_COOLDOWN_DECREASE;
            if (max_ebullet_cooldown < MIN_EBULLET_COOLDOWN) {
                max_ebullet_cooldown = MIN_EBULLET_COOLDOWN;
            }
            
            // Show level up screen
            show_level_up();
            
            // Reset scores for next level
            player_score = 0;
            enemy_score = 0;
            
            // Redraw HUD with reset scores
            draw_hud();
        }
        
        if (enemy_score >= SCORE_TO_WIN) {
            // Enemy wins - game over
            show_game_over();
            game_over = true;
        }
    }
    
    printf("\nExiting game...\n");
    printf("Final Level: %d\n", game_level);
    printf("Final Score: %d\n", game_score);
    
    return 0;
}
