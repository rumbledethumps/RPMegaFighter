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
#include "bullets.h"
#include "sound.h"
#include "bkgstars.h"
#include "pause.h"
#include "title_screen.h"
#include "text.h"
#include "input.h"
#include "screens.h"

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

// Bullet structure defined in bullets.h

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
// Note: Moved to sound.c

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

// Bullet pools - now in bullets.c module
extern Bullet bullets[MAX_BULLETS];
extern uint8_t current_bullet_index;

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
 * Note: Moved to bullets.c
 */

/**
 * Initialize fighter/enemy pools - based on SGDK implementation
 * Note: Moved to fighters.c
 */

/**
 * Initialize star field for parallax scrolling background
 * Note: Moved to bkgstars.c
 */

/**
 * Initialize PSG (Programmable Sound Generator)
 * Note: Moved to sound.c
 */

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
    reset_pause_state();  // Reset pause state
    reset_fighter_difficulty();  // Reset fighter difficulty to initial values
    game_over = false;
    
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
 * Note: Moved to pause.c
 */

// ============================================================================
// INPUT HANDLING
// ============================================================================

/**
 * Read keyboard and gamepad input
 * Note: Moved to input.c
 */

// ============================================================================
// GAME LOGIC UPDATES
// ============================================================================

/**
 * Update player ship position, rotation, and physics
 * Note: Moved to player.c
 */

/**
 * Update all active bullets and check collisions
 * Note: Moved to bullets.c
 */

/**
 * Update all active enemy fighters - based on MySegaGame SGDK implementation
 * Fighters chase the player and fire bullets
 * Note: Moved to fighters.c
 */








// ============================================================================
// RENDERING
// ============================================================================

/**
 * Text rendering functions moved to text.c
 */

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
 * Note: Moved to screens.c
 */

/**
 * Display game over screen and wait for fire button
 * Note: Moved to screens.c
 */

// ============================================================================
// TITLE SCREEN
// ============================================================================

/**
 * Display title screen and wait for START button
 * Note: Moved to title_screen.c
 */

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
        if (is_game_paused()) {
            // Check for A+C buttons pressed together to exit
            if (check_pause_exit()) {
                printf("\nA+C pressed - Exiting game...\n");
                break;
            }
            continue;
        }
        
        // Update cooldown timers
        decrement_bullet_cooldown();
        decrement_ebullet_cooldown();
        
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
