#ifndef CONSTANTS_H
#define CONSTANTS_H

/**
 * constants.h - Consolidated game constants
 * 
 * This file contains constants used across multiple modules
 * to avoid duplication and maintain consistency.
 */


// XRAM Map Addresses

// 0x0000 - 0xE100 57,600  Pixels  Background      320x180 Bitmap (8bpp)

// Sprite locations
// 0xE100 - 0xE180 128     Pixels  Spaceship       8x8 (16bpp)
// 0xE180 - 0xE980 2,048   Pixels  Earth           32x32 (16bpp)
// 0xE980 - 0xE9A0 32      Pixels  Fighter         4x4 (16bpp)
// 0xE9A0 - 0xE9A8 8       Pixels  Enemy Bullet    2x2 (16bpp)
// 0xE9A8 - 0xE9B0 8       Pixels  Player Bullet   2x2 (16bpp)
// 0xE9B0 - 0xE9D0 32      Pixels  Super Bullet    4x4 (16bpp)
#define SPACESHIP_DATA  0xE100  //Spaceship Sprite (8x8)
#define EARTH_DATA      0xE180  //Earth Sprite (32x32)
#define FIGHTER_DATA    0xE980  //Enemy fighter Sprite (4x4)
#define EBULLET_DATA    0xE9A0  //Enemy bullet Sprite (2x2)
#define BULLET_DATA     0xE9A8  //Player bullet Sprite (2x2)
#define SBULLET_DATA    0xE9B0  //Super bullet Sprite (4x4)
#define BOMBER_DATA     0xEF00  //Bomber Sprite (8x8)

// 0xE9D0 - 0xE9F8 40      Input   Gamepads        4 Pads x 10 bytes
// 0xE9F8 - 0xEA18 32      Input   Keyboard        256 bits
// 0xEA18 - 0xEA20 8       Gap     Safety Padding
#define GAMEPAD_INPUT   0xE9D0 // XRAM address for gamepad data
#define KEYBOARD_INPUT  0xE9F8  // XRAM address for keyboard data

// 0xEA20 - 0xEA2E 14      Config  Bitmap Config   Plane 0 Setup
// 0xEA2E - 0xEA36 8       Config  Earth Config    Plane 2 Setup
// 0xEA36 - 0xEA56 32      Config  Spaceship       Start of Plane 1 Swarm (Affine)
// 0xEA56 - 0xEB56 256     Config  Fighters        32 sprites (Standard)
// 0xEB56 - 0xEBD6 128     Config  E-Bullets       16 sprites
// 0xEBD6 - 0xEC16 64      Config  P-Bullets       8 sprites
// 0xEC16 - 0xEC2E 24      Config  S-Bullets       3 sprites
// 0xEC2E - 0xEC36 8       Config  Powerup         1 sprite
// 0xEC36 - 0xEC3E 8       Config  Bomber          1 sprite
// 0xEC3E - 0xEC46 8       Config  Marker          1 sprite
#define VGA_CONFIG_START 0xEA20         //Start of graphic config addresses (after gamepad and keyboard data)
extern unsigned BITMAP_CONFIG;          //Bitmap Config 
extern unsigned SPACECRAFT_CONFIG;      //Spacecraft Sprite Config - Affine 
extern unsigned EARTH_CONFIG;           //Earth Sprite Config - Standard 
extern unsigned STATION_CONFIG;         //Enemy station sprite config
extern unsigned BATTLE_CONFIG;          //Enemy battle station sprite config 
extern unsigned FIGHTER_CONFIG;         //Enemy fighter sprite config
extern unsigned EBULLET_CONFIG;         //Enemy bullet sprite config
extern unsigned BULLET_CONFIG;          //Player bullet sprite config
extern unsigned SBULLET_CONFIG;         //Super bullet sprite config
extern unsigned POWERUP_CONFIG; 

// 0xEC46 - 0xEC56 16      Config  Text Config     Plane 2 Overlay
// 0xEC56 - 0xED2E 216     Data    Text Buffer     72 chars x 3 bytes
// 0xED2E - 0xED40 18      Gap     Safety Padding
extern unsigned TEXT_CONFIG;            //On screen text configs
extern unsigned text_message_addr;

// 0xED40 - 0xED80 64      Config  Sound (PSG)     Extended Registers
// 0xED80 - 0xEE80 256     Pixels  Explosion       4x32 Anim Strip
// 0xEE80 - 0xEF00 128     Pixels  Powerup         8x8 (16bpp)
// 0xEF00 - 0xEF80 128     Pixels  Bomber          8x8 (16bpp)
// 0xEF80 - 0xF000 128     Pixels  Marker          8x8 (16bpp)
#define PSG_XRAM_ADDR   0xED40    // PSG memory location (must match sound.c)
#define EXPLOSION_DATA  0xED80    // Sprite data for explosion animation (not config data)
extern unsigned BOMBER_CONFIG;      // Bomber Sprite (8x8)

// 0xF000 - 0xFFFF 4,096   Free    Available       Expansion Space
#define EXTRA_SPRITES_START 0xF000 // Next free XRAM address

// Player ship properties
#define SHIP_ROTATION_STEPS 24  // Number of rotation steps
#define SHIP_ROTATION_MAX   23  // Ship rotation constant (24 rotation steps, 0-23)
#define SHIP_ROT_SPEED      3   // Frames per rotation step
#define BOUNDARY_X          100 // Horizontal boundary for player movement
#define BOUNDARY_Y          60  // Vertical boundary for player movement

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

// Fighter properties
#define MAX_FIGHTERS              32  // Maximum number of enemy fighters
#define FIGHTER_SPAWN_RATE        128 // Frames between fighter spawns
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

// Background star constants -- since this is half the world, we use for wrapping
#define NSTAR 32        // Number of stars in the background
#define STARFIELD_X 512 // Size of starfield (how often star pattern repeats)
#define STARFIELD_Y 256
// Size of World
#define WORLD_X 1024 // Size of World (for wrapping of objects such as the Earth)
#define WORLD_Y 1024 
#define WORLD_X2 512
#define WORLD_Y2 512
// Size of world for fighter wrapping
#define FIGHTER_WORLD_X  1024
#define FIGHTER_WORLD_Y  512
#define FIGHTER_WORLD_X2 512
#define FIGHTER_WORLD_Y2 256


// Display properties
// Screen dimensions (used by modules)
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       180
#define SCREEN_WIDTH_D2     160
#define SCREEN_HEIGHT_D2    90

// Controller input
#define GAMEPAD_COUNT 4       // Support up to 4 gamepads
#define GAMEPAD_DATA_SIZE 10  // 10 bytes per gamepad
#define KEYBOARD_BYTES  32    // 32 bytes for 256 key states

// Button definitions
#define KEY_ESC 0x29       // ESC key
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

// Text block palette/attribute constants
// ANSI palette indices: 6 = cyan, 3 = yellow, 8 = gray
#define BLOCK1_ATTR 0x06 // cyan
#define BLOCK2_ATTR 0x03 // yellow
#define BLOCK_EMPTY_ATTR 0x08 // grey

// Level text buffer length (chars)
#define LEVEL_MESSAGE_LENGTH 10

// Demo configuration
#define DEMO_DURATION_FRAMES (60 * 40) // Frames for demo mode (40 seconds)

#endif // CONSTANTS_H
