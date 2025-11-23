#include "player.h"
#include "bullets.h"
#include "sound.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       180
#define SCREEN_WIDTH_D2     160
#define SCREEN_HEIGHT_D2    90

#define SHIP_ROTATION_STEPS 24
#define SHIP_ROT_SPEED      3

#define BOUNDARY_X          60
#define BOUNDARY_Y          40

#define MAX_BULLETS         8
#define BULLET_COOLDOWN     8

// ============================================================================
// TYPES
// ============================================================================

// Bullet type defined in bullets.h

// ============================================================================
// EXTERNAL DEPENDENCIES
// ============================================================================

// Keyboard and gamepad input
#define KEYBOARD_BYTES 32
extern uint8_t keystates[KEYBOARD_BYTES];
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

// USB HID key codes we use
#define KEY_SPACE   0x2C  // Spacebar
#define KEY_UP      0x52  // Up arrow  
#define KEY_LEFT    0x50  // Left arrow
#define KEY_RIGHT   0x4F  // Right arrow

// Gamepad bits
#define GP_LSTICK_UP    0x01
#define GP_LSTICK_DOWN  0x02
#define GP_LSTICK_LEFT  0x04
#define GP_LSTICK_RIGHT 0x08

// Gamepad structure - must match definitions.h
typedef struct {
    uint8_t dpad;
    uint8_t sticks;
    uint8_t btn0;
    uint8_t btn1;
    int8_t lx;
    int8_t ly;
    int8_t rx;
    int8_t ry;
    uint8_t l2;
    uint8_t r2;
} gamepad_t;

extern gamepad_t gamepad[4];

// Lookup tables from definitions.h
extern const int16_t sin_fix[25];
extern const int16_t cos_fix[25];
extern const int16_t t2_fix4[25];
extern const uint8_t ri_max;

// Sprite configuration addresses
extern unsigned SPACECRAFT_CONFIG;
extern unsigned BULLET_CONFIG;

// Bullet array from main
extern Bullet bullets[MAX_BULLETS];
extern uint8_t current_bullet_index;

// World scrolling state (modified by player movement)
extern int16_t scroll_dx, scroll_dy;
extern int16_t world_offset_x, world_offset_y;

// Sound system (types defined in sound.h)
extern void play_sound(uint8_t type, uint16_t frequency, uint8_t waveform, 
                       uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release);

// ============================================================================
// MODULE STATE
// ============================================================================

// Player position (exported for other modules)
int16_t player_x = SCREEN_WIDTH_D2;
int16_t player_y = SCREEN_HEIGHT_D2;
int16_t player_vx_applied = 0;
int16_t player_vy_applied = 0;

// Player internal state
static int16_t player_vx = 0, player_vy = 0;
static int16_t player_x_rem = 0, player_y_rem = 0;
static int16_t player_rotation = 0;
static int16_t player_rotation_frame = 0;
static int16_t player_thrust_x = 0;
static int16_t player_thrust_y = 0;
static int16_t player_thrust_delay = 0;
static int16_t player_thrust_count = 0;

// Bullet cooldown
static uint16_t bullet_cooldown = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Get velocity components from rotation angle
 * Rotation 0 = pointing up (negative Y), increases clockwise
 */
static inline void get_velocity_from_rotation(uint8_t rotation, int16_t* vx_out, int16_t* vy_out)
{
    rotation = rotation % SHIP_ROTATION_STEPS;
    *vx_out = -sin_fix[rotation];
    *vy_out = -cos_fix[rotation];
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void init_player(void)
{
    player_x = SCREEN_WIDTH_D2;
    player_y = SCREEN_HEIGHT_D2;
    player_vx = 0;
    player_vy = 0;
    player_vx_applied = 0;
    player_vy_applied = 0;
    player_x_rem = 0;
    player_y_rem = 0;
    player_rotation = 0;
    player_rotation_frame = 0;
    player_thrust_x = 0;
    player_thrust_y = 0;
    player_thrust_delay = 0;
    player_thrust_count = 0;
    bullet_cooldown = 0;
}

void reset_player_position(void)
{
    player_x = SCREEN_WIDTH_D2;
    player_y = SCREEN_HEIGHT_D2;
    player_vx = 0;
    player_vy = 0;
    player_thrust_x = 0;
    player_thrust_y = 0;
    
    // Update sprite position
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, x_pos_px, player_x);
    xram0_struct_set(SPACECRAFT_CONFIG, vga_mode4_asprite_t, y_pos_px, player_y);
}

void update_player(void)
{
    // Handle player rotation
    player_rotation_frame++;
    if (player_rotation_frame >= SHIP_ROT_SPEED) {
        player_rotation_frame = 0;
        
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
        int16_t thrust_vx, thrust_vy;
        get_velocity_from_rotation(player_rotation, &thrust_vx, &thrust_vy);
        
        player_vx = thrust_vx;
        player_vy = thrust_vy;
        
        player_thrust_delay = 0;
        
        int16_t new_thrust_x = player_thrust_x + (thrust_vx >> 4);
        int16_t new_thrust_y = player_thrust_y + (thrust_vy >> 4);
        
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
    
    player_vx_applied = (total_vx + player_x_rem) >> 9;
    player_vy_applied = (total_vy + player_y_rem) >> 9;
    
    player_x_rem = total_vx + player_x_rem - (player_vx_applied << 9);
    player_y_rem = total_vy + player_y_rem - (player_vy_applied << 9);
    
    // Apply friction when not thrusting
    if (!thrust) {
        player_thrust_count++;
        if (player_thrust_count > 50 && player_thrust_delay < 8) {
            player_thrust_delay++;
            player_thrust_count = 0;
            
            if (player_vx == 0) {
                player_thrust_x >>= 1;
            }
            if (player_vy == 0) {
                player_thrust_y >>= 1;
            }
        }
        
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

void update_player_sprite(void)
{
    // Update sprite position
    RIA.step0 = sizeof(vga_mode4_asprite_t);
    RIA.step1 = sizeof(vga_mode4_asprite_t);
    RIA.addr0 = SPACECRAFT_CONFIG + 12;
    RIA.addr1 = SPACECRAFT_CONFIG + 13;
    
    RIA.rw0 = player_x & 0xFF;
    RIA.rw1 = (player_x >> 8) & 0xFF;
    
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

void fire_bullet(void)
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
        
        unsigned ptr = BULLET_CONFIG + current_bullet_index * sizeof(vga_mode4_sprite_t);
        xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, bullets[current_bullet_index].x);
        xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, bullets[current_bullet_index].y);
        
        play_sound(SFX_TYPE_PLAYER_FIRE, 110, PSG_WAVE_SQUARE, 0, 3, 4, 0);
        
        current_bullet_index++;
        if (current_bullet_index >= MAX_BULLETS) {
            current_bullet_index = 0;
        }
        
        bullet_cooldown = BULLET_COOLDOWN;
    }
}

void decrement_bullet_cooldown(void)
{
    if (bullet_cooldown > 0) {
        bullet_cooldown--;
    }
}

int16_t get_player_rotation(void)
{
    return player_rotation;
}
