/**
 * World and screen sizes
 */

#include "input.h"

// Screen dimensions (used by modules)
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       180
#define SCREEN_WIDTH_D2     160
#define SCREEN_HEIGHT_D2    90

static int16_t dx = 0;   // scrolling of visible screen relative to World map
static int16_t dy = 0;

//Sprite locations
#define SPACESHIP_DATA  0xE100  //Spaceship Sprite (8x8)
#define EARTH_DATA      0xE180  //Earth Sprite (32x32)
#define STATION_DATA    0xE980  //Enemy Station Sprite (16x16)
#define BATTLE_DATA     0xEB80  //Enemy battle station Sprite (8x8)
#define FIGHTER_DATA    0xEC00  //Enemy fighter Sprite (4x4)
#define EBULLET_DATA    0xEC20  //Enemy bullet Sprite (2x2)
#define BULLET_DATA     0xEC28  //Player bullet Sprite (2x2)
#define SBULLET_DATA    0xEC30  //Super bullet Sprite (4x4)

//XRAM Memory addresses
#define VGA_CONFIG_START 0xECA0 //Start of graphic config addresses (after gamepad data)
unsigned BITMAP_CONFIG;         //Bitmap Config 
unsigned SPACECRAFT_CONFIG;     //Spacecraft Sprite Config - Affine 
unsigned EARTH_CONFIG;          //Earth Sprite Config - Standard 
unsigned STATION_CONFIG;        //Enemy station sprite config
unsigned BATTLE_CONFIG;         //Enemy battle station sprite config 
unsigned FIGHTER_CONFIG;        //Enemy fighter sprite config
unsigned EBULLET_CONFIG;        //Enemy bullet sprite config
unsigned BULLET_CONFIG;         //Player bullet sprite config
unsigned SBULLET_CONFIG;        //Super bullet sprite config
unsigned TEXT_CONFIG;           //On screen text configs


// Star arrays (defined here, declared in bkgstars.h)
int16_t star_x[32] = {0};
int16_t star_y[32] = {0};
int16_t star_x_old[32] = {0};
int16_t star_y_old[32] = {0};
uint8_t star_colour[32] = {0};

// Text configs
#define NTEXT 1
int16_t text_message_addr;
static char score_message[6] = "SCORE ";
static char score_value[6] = "00000";
#define MESSAGE_LENGTH 36
static char message[MESSAGE_LENGTH];
static uint16_t score = 0;

const uint16_t vlen = 57600; // Extended Memory space for bitmap graphics (320x180 @ 8-bits)

// Pre-calulated Angles: 255*sin(theta)
const int16_t sin_fix[] = {
    0, 65, 127, 180, 220, 246, 255, 246, 220, 180, 127, 65, 
    0, -65, -127, -180, -220, -246, -255, -246, -220, -180, 
    -127, -65, 0
};

// Pre-calulated Angles: 255*cos(theta)
const int16_t cos_fix[] = {
    255, 246, 220, 180, 127, 65, 0, -65, -127, -180, -220, -246, 
    -255, -246, -220, -180, -127, -65, 0, 65, 127, 180, 
    220, 246, 255
};

// // Pre-calulated Affine offsets: 181*sin(theta - pi/4) + 127
// static const int16_t t2_fix8[] = {
//     0, 576, 1280, 2032, 2768, 3472, 4064, 4528, 4816, 4928, 
//     4816, 4528, 4064, 3472, 2768, 2032, 1280, 576, 0, -464, 
//     -752, -864, -752, -464, 0
// };

const int16_t t2_fix4[] = {
    0, 288, 640, 1016, 1384, 1736, 2032, 2264, 2408, 2464, 
    2408, 2264, 2032, 1736, 1384, 1016, 640, 288, 0, -232, 
    -376, -432, -376, -232, 0
};

// Earth initial position
static int16_t earth_x = SCREEN_WIDTH/2 - 16;
static int16_t earth_y = SCREEN_HEIGHT/2 - 16;
static int16_t earth_xc = SCREEN_WIDTH/2;
static int16_t earth_yc = SCREEN_HEIGHT/2;
