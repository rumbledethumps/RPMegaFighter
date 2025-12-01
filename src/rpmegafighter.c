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
#include "constants.h"
#include "definitions.h"
#include "screen.h"
#include "random.h"
#include "graphics.h"
#include "highscore.h"
#include "hud.h"
#include "fighters.h"
#include "player.h"
#include "bullets.h"
#include "sbullets.h"
#include "sound.h"
#include "music.h"
#include "bkgstars.h"
#include "pause.h"
#include "title_screen.h"
#include "text.h"
#include "input.h"
#include "screens.h"
#include "powerup.h"

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

// Earth background sprite
int16_t earth_x = 0;
int16_t earth_y = 0;

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

// Storage for sprite config addresses referenced across modules
unsigned POWERUP_CONFIG;

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
    
    // Set up Earth background sprite (VGA Mode 4 - regular sprite)
    EARTH_CONFIG = BITMAP_CONFIG + sizeof(vga_mode3_config_t);
    
    // Initialize Earth sprite centered on screen
    earth_x = SCREEN_WIDTH / 2;
    earth_y = SCREEN_HEIGHT / 2;
    
    xram0_struct_set(EARTH_CONFIG, vga_mode4_sprite_t, x_pos_px, earth_x);
    xram0_struct_set(EARTH_CONFIG, vga_mode4_sprite_t, y_pos_px, earth_y);
    xram0_struct_set(EARTH_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, EARTH_DATA);
    xram0_struct_set(EARTH_CONFIG, vga_mode4_sprite_t, log_size, 5);  // 32x32 sprite (2^5)
    xram0_struct_set(EARTH_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);
    
    // Set up player spacecraft sprite (VGA Mode 4 - affine sprite with rotation)
    SPACECRAFT_CONFIG = EARTH_CONFIG + sizeof(vga_mode4_sprite_t);
    
    // Initialize rotation transform matrix (identity at rotation 0)
    int16_t initial_rotation = get_player_rotation();
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[0],  cos_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[1], -sin_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[2],  t2_fix4[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[3],  sin_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[4],  cos_fix[initial_rotation]);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, transform[5],  t2_fix4[SHIP_ROTATION_MAX - initial_rotation + 1]);
    
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
    
    // Set up super bullet sprites (VGA Mode 4 - regular sprites)
    SBULLET_CONFIG = BULLET_CONFIG + MAX_BULLETS * sizeof(vga_mode4_sprite_t);
    
    for (uint8_t i = 0; i < MAX_SBULLETS; i++) {
        unsigned ptr = SBULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
        
        // Initialize sprite configuration  
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);  // Start offscreen
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
        xram0_struct_set(ptr, vga_mode4_sprite_t, xram_sprite_ptr, SBULLET_DATA);
        xram0_struct_set(ptr, vga_mode4_sprite_t, log_size, 2);  // 4x4 sprite (2^2)
        xram0_struct_set(ptr, vga_mode4_sprite_t, has_opacity_metadata, false);
    }

    // Initialize power-up sprite (VGA Mode 4 - regular sprite)
    POWERUP_CONFIG = SBULLET_CONFIG + MAX_SBULLETS * sizeof(vga_mode4_sprite_t);
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, x_pos_px, -100);  // Start offscreen
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, y_pos_px, -100);
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, POWERUP_DATA);
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, log_size, 3);  // 8x8 sprite (2^3)
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    // Enable sprite modes:
    // First enable Earth sprite (background layer)
    xregn(1, 0, 1, 5, 4, 0, EARTH_CONFIG, 1, 0);
    // Then enable affine sprites (player) - 1 sprite at SPACECRAFT_CONFIG
    xregn(1, 0, 1, 5, 4, 1, SPACECRAFT_CONFIG, 1, 2);
    // Finally enable regular sprites (fighters + ebullets + bullets + sbullets + power ups) - all regular sprites in one call
    xregn(1, 0, 1, 5, 4, 0, FIGHTER_CONFIG, MAX_FIGHTERS + MAX_EBULLETS + MAX_BULLETS + MAX_SBULLETS + 1, 1);

    // Enable text mode for on-screen messages

    TEXT_CONFIG = POWERUP_CONFIG + sizeof(vga_mode4_sprite_t); // 0xEC32; //Config address for text mode
    // Place text message data immediately after text config entries
    text_message_addr = TEXT_CONFIG + NTEXT * sizeof(vga_mode1_config_t); // 0xEC42; // address to store text message

    // Debug: print config addresses and sizes to help diagnose overlaps
    printf("Config addresses:\n");
    printf("  BITMAP_CONFIG=0x%X\n", BITMAP_CONFIG);
    printf("  EARTH_CONFIG=0x%X\n", EARTH_CONFIG);
    printf("  SPACECRAFT_CONFIG=0x%X\n", SPACECRAFT_CONFIG);
    printf("  FIGHTER_CONFIG=0x%X\n", FIGHTER_CONFIG);
    printf("  EBULLET_CONFIG=0x%X\n", EBULLET_CONFIG);
    printf("  BULLET_CONFIG=0x%X\n", BULLET_CONFIG);
    printf("  SBULLET_CONFIG=0x%X\n", SBULLET_CONFIG);
    printf("  POWERUP_CONFIG=0x%X\n", POWERUP_CONFIG);
    printf("  TEXT_CONFIG=0x%X\n", TEXT_CONFIG);
    printf("  text_message_addr=0x%X\n", text_message_addr);
    // Calculate and print end of text storage (MESSAGE_LENGTH * bytes_per_char)
    const unsigned bytes_per_char = 3; // we write 3 bytes per character into text RAM
    unsigned text_storage_end = text_message_addr + MESSAGE_LENGTH * bytes_per_char;
    printf("  text_storage_end=0x%X\n", text_storage_end);
    printf("  GAME_PAD_CONFIG=0x%X\n", GAMEPAD_INPUT);
    printf("  KEYBOARD_CONFIG=0x%X\n", KEYBOARD_INPUT);
    printf("  PSG_CONFIG=0x%X\n", PSG_XRAM_ADDR);

    // printf("Struct sizes: vga_mode4_sprite_t=%u, vga_mode1_config_t=%u\n", (unsigned)sizeof(vga_mode4_sprite_t), (unsigned)sizeof(vga_mode1_config_t));


    for (uint8_t i = 0; i < NTEXT; i++) {

        unsigned ptr = TEXT_CONFIG + i * sizeof(vga_mode1_config_t);

        xram0_struct_set(ptr, vga_mode1_config_t, x_wrap, 0);
        xram0_struct_set(ptr, vga_mode1_config_t, y_wrap, 0);
        xram0_struct_set(ptr, vga_mode1_config_t, x_pos_px, 7); //Bug: first char duplicated if not set to zero
        xram0_struct_set(ptr, vga_mode1_config_t, y_pos_px, 1);
        xram0_struct_set(ptr, vga_mode1_config_t, width_chars, MESSAGE_WIDTH);
        xram0_struct_set(ptr, vga_mode1_config_t, height_chars, MESSAGE_HEIGHT);
        xram0_struct_set(ptr, vga_mode1_config_t, xram_data_ptr, text_message_addr);
        xram0_struct_set(ptr, vga_mode1_config_t, xram_palette_ptr, 0xFFFF);
        xram0_struct_set(ptr, vga_mode1_config_t, xram_font_ptr, 0xFFFF);
    }

    // 4 parameters: text mode, 8-bit, config, plane
    xregn(1, 0, 1, 4, 1, 3, TEXT_CONFIG, 2);

    // Build composed message layout:
    // [pad][player:3][sp][block1:8][sp][game:5][sp][block2:8][sp][enemy:3][pad]
    // Center the 31-char sequence in the MESSAGE_LENGTH (36) buffer.
    const int seq_len = 3 + 1 + 8 + 1 + 5 + 1 + 8 + 1 + 3; // 31
    const int left_pad = (MESSAGE_WIDTH - seq_len) / 2; // integer division

    // Clear message buffer to spaces
    for (int i = 0; i < MESSAGE_LENGTH; ++i) message[i] = ' ';

    // Format scores into small temp buffers
    char player_buf[4] = "000"; // 3 chars + NUL
    char game_buf[6] = "00000";  // 5 chars + NUL
    char enemy_buf[4] = "000";
    char level_buf[3] = "01";   // 2 char + NUL

    // Ensure scores are in range and format zero-padded
    snprintf(player_buf, sizeof(player_buf), "%03d", player_score >= 0 ? player_score : 0);
    snprintf(game_buf, sizeof(game_buf), "%05d", game_score >= 0 ? game_score : 0);
    snprintf(enemy_buf, sizeof(enemy_buf), "%03d", enemy_score >= 0 ? enemy_score : 0);
    snprintf(level_buf, sizeof(level_buf), "%02d", game_level >= 1 ? game_level : 1);

    int idx = left_pad;
    // player 3 chars
    for (int k = 0; k < 3; ++k) message[idx++] = player_buf[k];
    // space after player
    message[idx++] = ' ';
    // block1 8 chars (placeholder: will be rendered as block glyphs)
    int block1_start = idx;
    idx += 8;
    // space before game
    message[idx++] = ' ';
    // game score 5 chars (surrounded by spaces)
    for (int k = 0; k < 5; ++k) message[idx++] = game_buf[k];
    // space after game
    message[idx++] = ' ';
    // block2 8 chars
    int block2_start = idx;
    idx += 8;
    // space before enemy
    message[idx++] = ' ';
    // enemy 3 chars
    for (int k = 0; k < 3; ++k) message[idx++] = enemy_buf[k];

    // Write Level Message
    idx = (MESSAGE_HEIGHT - 1) * MESSAGE_WIDTH +  MESSAGE_WIDTH/2 - 4; // center "LEVEL XX"
    for (int k = 0; k < 5; ++k) message[idx++] = level_message[k];
    for (int k = 0; k < 2; ++k) message[idx++] = level_buf[k];

    // printf("Full message: '%.*s'\n", MESSAGE_LENGTH, message);

    // Now write the MESSAGE_LENGTH characters into text RAM (3 bytes per char)
    RIA.addr0 = text_message_addr;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < MESSAGE_LENGTH; i++) {
        // block1 region
        if ((int)i >= block1_start && (int)i < block1_start + 8) {
            RIA.rw0 = 0xDB; // block glyph
            RIA.rw0 = BLOCK1_ATTR; // palette/attr for first block
            RIA.rw0 = 0x10; // extra byte
        }
        // block2 region
        else if ((int)i >= block2_start && (int)i < block2_start + 8) {
            RIA.rw0 = 0xDB; // block glyph
            RIA.rw0 = BLOCK2_ATTR; // palette/attr for second block (yellow)
            RIA.rw0 = 0x10;
        }
        else {
            RIA.rw0 = message[i];
            RIA.rw0 = 0xE0;
            RIA.rw0 = 0x00;
        }
    }

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
    reset_music_tempo();  // Reset music tempo to default
    game_over = false;
    
    // Reset player position and state
    init_player();
    
    // Initialize entity pools
    init_bullets();
    init_sbullets();
    init_fighters();
    init_stars();

    // Reset Earth position
    earth_x = SCREEN_WIDTH / 2;
    earth_y = SCREEN_HEIGHT / 2;

    // Reset power-up state
    powerup.active = false;
    powerup.timer = 0;
    // If not already, move power-up sprite offscreen
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, x_pos_px, -100);
    xram0_struct_set(POWERUP_CONFIG, vga_mode4_sprite_t, y_pos_px, -100);

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
void render_game(void)
{
    // Draw scrolling star background
    draw_stars(scroll_dx, scroll_dy);
    
    // Update Earth sprite position based on scrolling with wrapping
    earth_x -= scroll_dx;
    earth_y -= scroll_dy;
    
    // Wrap Earth horizontally - same as stars
    if (earth_x <= -30) {
        earth_x += WORLD_X;
    }
    if (earth_x > WORLD_X - 30) {
        earth_x -= WORLD_X;
    }
    
    // Wrap Earth vertically - same as stars
    if (earth_y <= -30) {
        // When wrapping from top, place at bottom minus HUD offset
        earth_y += WORLD_Y;
    }
    if (earth_y > WORLD_Y - 30) {
        // When wrapping from bottom, place below HUD area
        earth_y -= WORLD_Y;
    }
    
    xram0_struct_set(EARTH_CONFIG, vga_mode4_sprite_t, x_pos_px, earth_x);
    xram0_struct_set(EARTH_CONFIG, vga_mode4_sprite_t, y_pos_px, earth_y);
    
    // Update fighter sprite positions
    render_fighters();
    
    // Bullets are drawn in update_bullets() and update_ebullets()
    
    // Update player sprite on screen
    update_player_sprite();

    // Update power-up sprite if active
    render_powerup();

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

// Demo mode parameters
bool demo_mode_active = false;
uint16_t demo_frames = 0;
// Color cycling for demo text
const uint8_t demo_colors[] = {1, 2, 3, 4, 5, 6, 7};
const uint8_t num_demo_colors = sizeof(demo_colors) / sizeof(demo_colors[0]);
// Previous key states for demo mode exit
bool enter_was_down = false;
bool esc_was_down = false;
bool b_was_down = false;

// init_input_system_test() was moved to input.c

int main(void)
{
    printf("\n=== RPMegaFighter ===\n");
    printf("Port of Mega Super Fighter Challenge to RP6502\n\n");
    
    // Initialize systems (one time only)
    init_graphics();
    init_psg();
    init_music();
    
    // Load high scores from file
    load_high_scores();
    
    // Enable keyboard input
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    
    // Enable gamepad input
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    // Initialize input mappings (ensure `button_mappings` are set)
    init_input_system();  // new function to set up input mappings
    
    printf("\nControls:\n");
    printf("  Keyboard: Arrow keys to rotate/thrust, SPACE/SHIFT to fire\n");
    printf("  Gamepad:  Left stick to rotate/thrust, A/B to fire\n");
    printf("  ESC to quit, START to pause\n\n");
    
    uint8_t vsync_last = RIA.vsync;

    // Main game loop - includes title screen and gameplay
    while (true) {
        // Run the input test (non-destructive) then show title screen
    #ifdef INPUT_TEST
        init_input_system_test();
    #endif

        // Show title screen and wait for START
        show_title_screen();

        // Reset demo mode counter
        if (demo_mode_active) {
            demo_frames = 0;
        }

        // Initialize/reset game state
        init_game();
        
        // Start gameplay music
        start_gameplay_music();
        
        printf("Starting game loop...\n\n");
        
        // Gameplay loop
        game_over = false;
        bool demo_input_was_pressed = false;
        while (!game_over) {
            // Wait for vertical sync (60 Hz)
            if (RIA.vsync == vsync_last)
                continue;
            vsync_last = RIA.vsync;
        
            // Read input
            handle_input(); 

            // This prevents the START button from freezing the game during the demo
            if (!demo_mode_active) {
                handle_pause_input();
            }
            
            if (demo_mode_active) {
                demo_frames++;
                
                // A. Prevent the "Pause" effect
                // If handle_input() just paused the game, unpause it immediately.
                // We do this so the internal state in pause.c stays correct (Not Paused).
                if (is_game_paused()) {
                    handle_pause_input(); // Call it again to toggle it back to FALSE
                }

                // B. Check for Exit Conditions
                // Check Semantic Actions (FIRE or PAUSE/START)
                bool input_pressed = is_action_pressed(0, ACTION_FIRE);

                // C. Check for Button Release (Edge Detection)
                // Only exit if input WAS pressed last frame and is NOT pressed now.
                if (demo_input_was_pressed && !input_pressed) {
                    demo_mode_active = false;
                    game_over = true;
                    stop_music(); 
                    printf("Exiting demo mode due to player input\n");
                    
                    // Critical: Reset pause state one last time to ensure 
                    // the real game doesn't start immediately paused.
                    if (is_game_paused()) handle_pause_input();
                }
                
                // Update history for the next frame
                demo_input_was_pressed = input_pressed;
            }
            
            // Check for ESC key to exit
            if (key(KEY_ESC)) {
                printf("Exiting game...\n");
                stop_music();
                break;
            }
            
            // We override this to FALSE if we are in demo mode
            bool currently_paused = is_game_paused();
            if (demo_mode_active) currently_paused = false;

            // Handle pause state and music
            static bool was_paused = false;
            if (currently_paused && !was_paused) {
                // Just paused - stop music
                stop_music();
            } else if (!currently_paused && was_paused) {
                // Just resumed - restart music
                start_gameplay_music();
            }
            was_paused = currently_paused;
            
            // Skip updates if paused
            if (currently_paused) {
                // Check for A+Y buttons pressed together to exit
                if (check_pause_exit()) {
                    printf("\nA+Y pressed - Exiting game...\n");
                    stop_music();
                    break;
                }
                continue;
            }
            
            // Update music
            update_music();
            
            // Update cooldown timers
            decrement_bullet_cooldown();
            decrement_ebullet_cooldown();
            
            // Enemy bullet system
            fire_ebullet();
            
            // Handle player fire buttons
            // Regular bullets: keyboard SPACE or gamepad A button (0x01)
            if (is_action_pressed(0, ACTION_FIRE) || (demo_mode_active)) {
                fire_bullet();
            }
            
            // Super bullets: keyboard Left Shift or gamepad X button (0x08)
            if (is_action_pressed(0, ACTION_SUPER_FIRE) || (demo_mode_active)) {
                fire_sbullet(get_player_rotation());
            }
            
            // Update game logic
            update_player(demo_mode_active);
            update_fighters();
            update_bullets();
            update_sbullets();
            update_ebullets();

            // Update scrolling based on player movement
            update_powerup();
            
            // Render frame
            render_game();
            draw_hud();

            // Demo Overlay Rendering (Kept at bottom to draw on top)
            if (demo_mode_active) {
                // Update demo text color and text only every 20 frames
                if ((demo_frames % 20) == 0) {
                    uint8_t demo_color = demo_colors[(demo_frames / 20) % num_demo_colors];
                    draw_text(SCREEN_WIDTH / 2 - 23, 25, "DEMO MODE", demo_color);
                    draw_text(SCREEN_WIDTH / 2 - 40, SCREEN_HEIGHT - 15, "PRESS FIRE TO EXIT", demo_color);
                }

                if (demo_frames >= DEMO_DURATION_FRAMES) {
                    demo_mode_active = false;
                    game_over = true;
                    stop_music();
                    printf("Exiting demo mode after %d frames\n", DEMO_DURATION_FRAMES);
                }
            }
            
            // Increment frame counter
            game_frame++;
            if (game_frame >= 60) {
                game_frame = 0;
            }
            
            // Check win/lose conditions
            if (player_score >= SCORE_TO_WIN && !demo_mode_active) {
                // Player wins this round - level up!
                game_level++;
                
                // Increase difficulty by reducing enemy bullet cooldown
                increase_fighter_difficulty();
                
                // Speed up the music
                increase_music_tempo();
                
                // Show level up screen
                show_level_up();
                
                // Reset scores for next level
                player_score = 0;
                enemy_score = 0;
                
                // Redraw HUD with reset scores
                draw_hud();
            }
            
            if (enemy_score >= SCORE_TO_WIN && !demo_mode_active) {
                // Enemy wins - game over
                stop_music();  // Stop gameplay music
                reset_music_tempo();  // Reset tempo for next game
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
