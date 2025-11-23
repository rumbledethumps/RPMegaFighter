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
#include "highscore.h"
#include "hud.h"
#include "fighters.h"
#include "player.h"

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
#define MAX_EBULLETS        16     // Enemy bullets
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

// PSG memory location in XRAM (high memory, safe from graphics)
// Use upper XRAM at 0xFF00 (well away from all graphics data)
#define PSG_XRAM_ADDR 0xFF00

// Sound effect types (for round-robin allocation)
#define SFX_TYPE_PLAYER_FIRE   0
#define SFX_TYPE_ENEMY_FIRE    1
#define SFX_TYPE_COUNT         2

// Channel allocation (2 channels per effect type for round-robin)
static uint8_t next_channel[SFX_TYPE_COUNT] = {0, 2};

// ============================================================================
// HIGH SCORE SYSTEM
// ============================================================================

// ============================================================================
// GLOBAL GAME STATE
// ============================================================================

// Player state - now in player.c module
extern int16_t player_x, player_y;
extern int16_t player_vx_applied, player_vy_applied;

// Scrolling
int16_t scroll_dx = 0;
int16_t scroll_dy = 0;
int16_t world_offset_x = 0;  // Cumulative world offset from scrolling
int16_t world_offset_y = 0;

// Scores and game state
int16_t player_score = 0;
int16_t enemy_score = 0;
int16_t game_score = 0;     // Skill-based score
int16_t game_level = 1;
uint16_t game_frame = 0;    // Frame counter (0-59)
static bool game_over = false;

// Control mode (from SGDK version)
static uint8_t control_mode = 0;   // 0 = rotational, 1 = directional

// Bullet pools
Bullet bullets[MAX_BULLETS];
static Bullet sbullets[MAX_SBULLETS];
static uint16_t sbullet_cooldown = 0;
uint8_t current_bullet_index = 0;
static uint8_t current_sbullet_index = 0;

// Note: ebullets, fighters, and related state moved to fighters.c

// Input state (keystates and start_button_pressed are in definitions.h)
gamepad_t gamepad[GAMEPAD_COUNT];

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

// Forward declarations
void draw_text(int16_t x, int16_t y, const char* text, uint8_t color);
void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height);

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
    
    // Note: ebullets initialized in init_fighters()
    
    for (uint8_t i = 0; i < MAX_SBULLETS; i++) {
        sbullets[i].status = -1;
    }
}

/**
 * Initialize fighter/enemy pools - based on SGDK implementation
 */


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
    int16_t initial_rotation = get_player_rotation();
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[0],  cos_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[1], -sin_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[2],  t2_fix4[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[3],  sin_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[4],  cos_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[5],  t2_fix4[ri_max - initial_rotation + 1]);
    
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
    start_button_pressed = false;  // Reset to prevent immediate pause after title screen
    
    // Reset player position and state
    init_player();
    
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
        
        // Draw "PUSH A+C TO EXIT" below PAUSED
        draw_text(center_x - 10, center_y + 20, "PUSH A+C TO EXIT", pause_color);
    } else {
        // Clear PAUSED text by drawing black
        for (uint16_t x = center_x; x < center_x + 68; x++) {
            for (uint16_t y = center_y; y < center_y + 12; y++) {
                set(x, y, 0x00);
            }
        }
        
        // Clear "PUSH A+C TO EXIT" text
        clear_rect(center_x - 10, center_y + 20, 90, 5);
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
        if (check_bullet_fighter_collision(bullets[i].x, bullets[i].y, &player_score, &game_score)) {
            // Hit! Remove bullet
            bullets[i].status = -1;
            
            // Move bullet sprite offscreen
            unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            
            goto next_bullet;  // Skip rest of bullet update
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
void draw_text(int16_t x, int16_t y, const char* text, uint8_t color)
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
 * Clear a rectangular area
 */
void clear_rect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    for (int16_t dy = 0; dy < height; dy++) {
        for (int16_t dx = 0; dx < width; dx++) {
            set(x + dx, y + dy, 0x00);
        }
    }
}

/**
 * Update player sprite position and rotation
 */


/**
 * Render all game entities
 */
static void render_game(void)
{
    // Draw scrolling star background
    draw_stars(scroll_dx, scroll_dy);
    
    // Update fighter sprite positions
    render_fighters();
    
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
    
    // Move all active fighters offscreen
    move_fighters_offscreen();
    
    // Move all bullets offscreen
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].status >= 0) {
            unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            bullets[i].status = -1;
        }
    }
    
    // Move all enemy bullets offscreen
    move_ebullets_offscreen();
    
    // Reset player position to center
    reset_player_position();
    
    // Check if player got a high score
    int8_t high_score_pos = check_high_score(game_score);
    if (high_score_pos >= 0) {
        // Get player initials
        char initials[4];
        get_player_initials(initials);
        
        // Insert into high score table
        insert_high_score(high_score_pos, initials, game_score);
        
        // Save to file
        save_high_scores();
    }
    
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
            
            // Clear screen before returning to title screen
            RIA.addr0 = 0;
            RIA.step0 = 1;
            for (unsigned i = vlen; i--;) {
                RIA.rw0 = 0;
            }
            
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
    
    // Draw high scores on right side
    draw_high_scores();
    
    uint8_t vsync_last = RIA.vsync;
    uint16_t flash_counter = 0;
    bool press_start_visible = true;
    const uint16_t flash_interval = 30;  // 0.5 seconds at 60 Hz
    uint8_t current_color = red_color;
    
    // Draw initial "PRESS START" text
    draw_text(center_x - 20, 110, "PRESS START", red_color);
    
    // Draw exit instruction
    draw_text(center_x - 30, 130, "PUSH A+C TO EXIT", blue_color);
    
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
                    
                    // Wait for START button to be released before exiting
                    while (true) {
                        if (RIA.vsync == vsync_last)
                            continue;
                        vsync_last = RIA.vsync;
                        
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
                        
                        // Exit loop when START button is released
                        if (!(gamepad[0].btn1 & 0x02)) {
                            break;
                        }
                    }
                    
                    return;  // Exit title screen
                }
            } else {
                // Button is not pressed
                start_button_was_pressed = false;
            }
        }
        
        // Check for A+C buttons pressed together to exit (0x04 + 0x20 = 0x24)
        if ((gamepad[0].btn0 & 0x04) && (gamepad[0].btn0 & 0x20)) {
            printf("A+C pressed - exiting...\n");
            exit(0);
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
    
    // Initialize systems (one time only)
    init_graphics();
    init_psg();
    
    // Load high scores from file
    load_high_scores();
    
    // Enable keyboard input
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    
    // Enable gamepad input
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);
    
    printf("\nControls:\n");
    printf("  Keyboard: Arrow keys to rotate/thrust, SPACE to fire\n");
    printf("  Gamepad:  Left stick to rotate/thrust, A/B to fire\n");
    printf("  ESC to quit, START to pause\n\n");
    
    uint8_t vsync_last = RIA.vsync;
    
    // Main game loop - includes title screen and gameplay
    while (true) {
        // Show title screen and wait for START
        show_title_screen();
        
        // Initialize/reset game state
        init_game();
        
        printf("Starting game loop...\n\n");
        
        // Gameplay loop
        game_over = false;
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
        decrement_bullet_cooldown();
        decrement_ebullet_cooldown();
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
            increase_fighter_difficulty();
            
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
            
            // Set flag to exit gameplay loop and return to title screen
            game_over = true;
        }
    }
    // Gameplay loop ended - will return to title screen
    }
    
    // Should never reach here unless ESC pressed
    printf("\nExiting game...\n");
    printf("Final Level: %d\n", game_level);
    printf("Final Score: %d\n", game_score);
    
    return 0;
}
