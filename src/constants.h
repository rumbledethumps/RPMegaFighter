#ifndef CONSTANTS_H
#define CONSTANTS_H

/**
 * constants.h - Consolidated game constants
 * 
 * This file contains constants used across multiple modules
 * to avoid duplication and maintain consistency.
 */

// Player ship properties
#define SHIP_ROTATION_STEPS 24 // Number of rotation steps
#define SHIP_ROTATION_MAX 23   // Ship rotation constant (24 rotation steps, 0-23)

#define SHIP_ROT_SPEED      3  // Frames per rotation step
#define BOUNDARY_X          100 // Horizontal boundary for player movement
#define BOUNDARY_Y          60 // Vertical boundary for player movement

// Bullet properties
#define MAX_BULLETS         8
#define BULLET_COOLDOWN     8

// Enemy bullet properties
#define MAX_EBULLETS        16       // Enemy bullets
#define NEBULLET_TIMER_MAX  8        // Frames between enemy bullet shots
// #define EFIRE_COOLDOWN_TIMER 16      // Frames a fighter must wait between shots

#define INITIAL_EBULLET_COOLDOWN 30  // Starting cooldown for enemy bullets
#define MIN_EBULLET_COOLDOWN     1   // Minimum cooldown (difficulty cap)
#define EBULLET_COOLDOWN_DECREASE 5  // Decrease per level

// Spread shot bullets
#define MAX_SBULLETS         3        // Spread shot bullets
#define SBULLET_COOLDOWN_MAX 120      // Frames between super bullet shots
#define SBULLET_SPEED_SHIFT  6        // Divide by 64 for bullet speed (~4 pixels/frame)

// Fighter properties
#define MAX_FIGHTERS        32    // Maximum number of enemy fighters
#define FIGHTER_SPAWN_RATE  128   // Frames between fighter spawns
#define INITIAL_FIGHTER_SPEED_MIN 16  // Initial minimum fighter speed
#define INITIAL_FIGHTER_SPEED_MAX 256 // Initial maximum fighter speed
#define FIGHTER_SPEED_INCREASE    32  // Fighter speed increase per level
#define MAX_FIGHTER_SPEED         512 // Maximum cap on fighter speed

// Scoring
#define SCORE_TO_WIN        100
// #define SCORE_BASIC_KILL    1
// #define SCORE_MINE_KILL     5
// #define SCORE_SHIELD_KILL   5
// #define SCORE_MINE_HIT      -10

// High score constants
#define MAX_HIGH_SCORES 10
#define HIGH_SCORE_NAME_LEN 3
#define HIGH_SCORE_FILE "HIGHSCOR.DAT"

// Background star constants
#define NSTAR 32        // Number of stars in the background
#define STARFIELD_X 512 // Size of starfield (how often star pattern repeats)
#define STARFIELD_Y 256
// Size of World
#define WORLD_X 1024 // Size of World (for wrapping of objects such as the Earth)
#define WORLD_Y 512

// Display properties
// Screen dimensions (used by modules)
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       180
#define SCREEN_WIDTH_D2     160
#define SCREEN_HEIGHT_D2    90

// Controller input
#define GAMEPAD_COUNT 4      // Support up to 4 gamepads
#define GAMEPAD_INPUT 0xEC50 // XRAM address for gamepad data
#define GAMEPAD_DATA_SIZE 10      // 10 bytes per gamepad

#define KEYBOARD_INPUT  0xEC20  // XRAM address for keyboard data
#define KEYBOARD_BYTES  32      // 32 bytes for 256 key states

// Button definitions
#define KEY_ESC 0x29
#define KEY_ENTER 0x28     // ENTER key

// Hardware button bit masks - DPAD
#define GP_DPAD_UP        0x01
#define GP_DPAD_DOWN      0x02
#define GP_DPAD_LEFT      0x04
#define GP_DPAD_RIGHT     0x08
#define GP_SONY           0x40  // Sony button faces (Circle/Cross/Square/Triangle)
#define GP_CONNECTED      0x80  // Gamepad is connected

// Hardware button bit masks - ANALOG STICKS
#define GP_LSTICK_UP      0x01
#define GP_LSTICK_DOWN    0x02
#define GP_LSTICK_LEFT    0x04
#define GP_LSTICK_RIGHT   0x08
#define GP_RSTICK_UP      0x10
#define GP_RSTICK_DOWN    0x20
#define GP_RSTICK_LEFT    0x40
#define GP_RSTICK_RIGHT   0x80

// Hardware button bit masks - BTN0 (Face buttons and shoulders)
// Per RP6502 documentation: https://picocomputer.github.io/ria.html#gamepads
#define GP_BTN_A          0x01  // bit 0: A or Cross
#define GP_BTN_B          0x02  // bit 1: B or Circle
#define GP_BTN_C          0x04  // bit 2: C or Right Paddle
#define GP_BTN_X          0x08  // bit 3: X or Square
#define GP_BTN_Y          0x10  // bit 4: Y or Triangle
#define GP_BTN_Z          0x20  // bit 5: Z or Left Paddle
#define GP_BTN_L1         0x40  // bit 6: L1
#define GP_BTN_R1         0x80  // bit 7: R1

// Hardware button bit masks - BTN1 (Triggers and special buttons)
#define GP_BTN_L2         0x01  // bit 0: L2
#define GP_BTN_R2         0x02  // bit 1: R2
#define GP_BTN_SELECT     0x04  // bit 2: Select/Back
#define GP_BTN_START      0x08  // bit 3: Start/Menu
#define GP_BTN_HOME       0x10  // bit 4: Home button
#define GP_BTN_L3         0x20  // bit 5: L3
#define GP_BTN_R3         0x40  // bit 6: R3


// PSG memory location (must match sound.c)
#define PSG_XRAM_ADDR 0xFF00

#endif // CONSTANTS_H
