/**
 * World and screen sizes
 */

#define MAPSIZE 2048     // Size of world (world coordinates)
#define MMAPSIZE -2048
#define MAPSIZED2 1024 
#define MMAPSIZED2 -1024 
#define MAPSIZEM1 1023 
#define SWIDTH 320       // width of visible screen
#define SHEIGHT 180      // height of visible screen
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

// USB KEYBOARD
#define KEYBOARD_INPUT  0xEC20  // 0xFF10U   // KEYBOARD_BYTES (32 bytes, 256 bits) of key press bitmask data
// 256 bytes HID code max, stored in 32 uint8
#define KEYBOARD_BYTES 32
// keystates[code>>3] gets contents from correct byte in array
// 1 << (code&7) moves a 1 into proper position to mask with byte contents
// final & gives 1 if key is pressed, 0 if not
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))
uint8_t keystates[KEYBOARD_BYTES] = {0};

bool handled_key;

#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

// GAMEPAD SUPPORT
#define GAMEPAD_INPUT 0xEC50  // XRAM address for gamepad data (40 bytes for 4 gamepads)
#define GAMEPAD_COUNT 4       // Support up to 4 gamepads
#define GAMEPAD_DATA_SIZE 10  // 10 bytes per gamepad

// Gamepad structure (10 bytes per gamepad)
typedef struct {
    uint8_t dpad;      // Direction pad + status bits
    uint8_t sticks;    // Left and right stick digital directions
    uint8_t btn0;      // Face buttons and shoulders
    uint8_t btn1;      // L2/R2/Select/Start/Home/L3/R3
    int8_t lx;         // Left stick X analog (-128 to 127)
    int8_t ly;         // Left stick Y analog (-128 to 127)
    int8_t rx;         // Right stick X analog (-128 to 127)
    int8_t ry;         // Right stick Y analog (-128 to 127)
    uint8_t l2;        // Left trigger analog (0-255)
    uint8_t r2;        // Right trigger analog (0-255)
} gamepad_t;

// Gamepad DPAD bits
#define GP_DPAD_UP        0x01
#define GP_DPAD_DOWN      0x02
#define GP_DPAD_LEFT      0x04
#define GP_DPAD_RIGHT     0x08
#define GP_SONY           0x40  // Sony button faces (Circle/Cross/Square/Triangle)
#define GP_CONNECTED      0x80  // Gamepad is connected

// Gamepad STICKS bits
#define GP_LSTICK_UP      0x01
#define GP_LSTICK_DOWN    0x02
#define GP_LSTICK_LEFT    0x04
#define GP_LSTICK_RIGHT   0x08
#define GP_RSTICK_UP      0x10
#define GP_RSTICK_DOWN    0x20
#define GP_RSTICK_LEFT    0x40
#define GP_RSTICK_RIGHT   0x80

// Gamepad BTN0 bits - ACTUAL HARDWARE MAPPING
// Based on testing: A=0x04, B=0x02, C=0x20
#define GP_BTN_A          0x04  // A button on controller
#define GP_BTN_B          0x02  // B button on controller  
#define GP_BTN_C          0x20  // C button on controller
#define GP_BTN_X          0x08  // X or Square (not present)
#define GP_BTN_Y          0x10  // Y or Triangle (not present)
#define GP_BTN_Z          0x01  // Z (not present)
#define GP_BTN_L1         0x40  // (not present)
#define GP_BTN_R1         0x80  // (not present)

// Gamepad BTN1 bits - ACTUAL HARDWARE MAPPING
// Based on testing: START=0x02
#define GP_BTN_L2         0x01  // (not present)
#define GP_BTN_R2         0x02  // START button (actual mapping)
#define GP_BTN_SELECT     0x04  // (not present)
#define GP_BTN_START      0x02  // START button on controller
#define GP_BTN_HOME       0x10
#define GP_BTN_L3         0x20
#define GP_BTN_R3         0x40

static gamepad_t gamepad[GAMEPAD_COUNT];


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
unsigned TEXT_CONFIG;           //On screen text config

#define NSTATION_MAX 5  //Number of enemy battle stations
#define NBATTLE_MAX  5  //Number of portable battle stations 
#define NFIGHTER_MAX 30  //Number of fighters

static uint8_t nstation = NSTATION_MAX; //Battle stations
static int16_t station_x[NSTATION_MAX] = {0};  //Location of battle stations
static int16_t station_y[NSTATION_MAX] = {0}; 
static int8_t  station_status[NSTATION_MAX] = {0}; //Flag to determine if station is alive or dead

static uint8_t nbattle = NBATTLE_MAX; //Moving battle stations
static uint8_t battle_status[NBATTLE_MAX] = {0};  //Flag to determine if station is alive or dead
static int16_t battle_x[NBATTLE_MAX] = {0}; //location
static int16_t battle_y[NBATTLE_MAX] = {0}; 
static int16_t battle_dx[NBATTLE_MAX] = {0};  //speed
static int16_t battle_dy[NBATTLE_MAX] = {0};
static int16_t battle_xrem[NBATTLE_MAX] = {0}; //speed round-off
static int16_t battle_yrem[NBATTLE_MAX] = {0};

#define FIGHTER_RATE 128 //Rate at which Fighters regenerate.
static uint8_t nfighter = NFIGHTER_MAX; //number of fighters
static uint8_t fighter_status[NFIGHTER_MAX] = {0}; //Status of fighter (dead/alive)
static int16_t fighter_x[NFIGHTER_MAX] = {0}; //position
static int16_t fighter_y[NFIGHTER_MAX] = {0}; 
static int16_t fighter_lx1old[NFIGHTER_MAX] = {0}; 
static int16_t fighter_lx2old[NFIGHTER_MAX] = {0}; 
static int16_t fighter_ly1old[NFIGHTER_MAX] = {0}; 
static int16_t fighter_ly2old[NFIGHTER_MAX] = {0}; 
static int16_t fighter_dx[NFIGHTER_MAX] = {0}; 
static int16_t fighter_dy[NFIGHTER_MAX] = {0}; 
static int16_t fighter_vx[NFIGHTER_MAX] = {0}; 
static int16_t fighter_vy[NFIGHTER_MAX] = {0}; 
static int16_t fighter_vxi[NFIGHTER_MAX] = {0}; 
static int16_t fighter_vyi[NFIGHTER_MAX] = {0}; 
static int16_t fighter_xrem[NFIGHTER_MAX] = {0}; 
static int16_t fighter_yrem[NFIGHTER_MAX] = {0}; 

static uint8_t nsprites = 0;

//Boundary for scrolling -- how far we can move the spaceshift until the background scrolls 
#define BBX 100
#define BBY 60
static int16_t BX1 = BBX;
static int16_t BX2 = SWIDTH - BBX;
static int16_t BY1 = BBY;
static int16_t BY2 = SHEIGHT - BBY;

//Frame counter
uint16_t update_sch = 0; 

//Background stars -- it'd be nice to change this to tiles.
#define NSTAR 32 // number of stars in the background
#define STARFIELD_X 512 //Size of starfield (how often star pattern repeats)
#define STARFIELD_Y 256
static int16_t star_x[NSTAR] = {0};      //X-position -- World coordinates
static int16_t star_y[NSTAR] = {0};      //Y-position -- World coordinates
static int16_t star_x_old[NSTAR] = {0};  //prev X-position -- World coordinates
static int16_t star_y_old[NSTAR] = {0};  //prev Y-position -- World coordinates
static uint8_t star_colour[NSTAR] = {0};  //Colour

// Text configs
#define NTEXT 1
int16_t text_message_addr;
static char score_message[6] = "SCORE ";
static char score_value[6] = "00000";
#define MESSAGE_LENGTH 36
static char message[MESSAGE_LENGTH];
static uint16_t score = 0;

// Game pause state
static bool game_paused = false;
static bool start_button_pressed = false;  // For edge detection

static const uint16_t vlen = 57600; // Extended Memory space for bitmap graphics (320x180 @ 8-bits)

// Pre-calulated Angles: 255*sin(theta)
static const int16_t sin_fix[] = {
    0, 65, 127, 180, 220, 246, 255, 246, 220, 180, 127, 65, 
    0, -65, -127, -180, -220, -246, -255, -246, -220, -180, 
    -127, -65, 0
};

// Pre-calulated Angles: 255*cos(theta)
static const int16_t cos_fix[] = {
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

static const int16_t t2_fix4[] = {
    0, 288, 640, 1016, 1384, 1736, 2032, 2264, 2408, 2464, 
    2408, 2264, 2032, 1736, 1384, 1016, 640, 288, 0, -232, 
    -376, -432, -376, -232, 0
};

// Should be able to compact this array 
static const uint8_t dxdy_table[] = {
0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
1, 3, 3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 2, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 1, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 1, 1, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 1, 1, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 0, 1, 1, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5,
0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5,
0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4,
0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4
};

static uint8_t ri = 0;            // current rotation info for spaceship
static const uint8_t ri_max = 23; // max rotations 

// Spacecraft properties
#define SHIP_ROT_SPEED 3 // How fast the spaceship can rotate, must be >= 1. 

// Earth initial position
static int16_t earth_x = SWIDTH/2 - 16;
static int16_t earth_y = SHEIGHT/2 - 16;
static int16_t earth_xc = SWIDTH/2;
static int16_t earth_yc = SHEIGHT/2;

// Initial position and velocity of spacecraft
static int16_t x = 160;  //Screen-coordinates of spacecraft
static int16_t y = 90;
static int16_t vx = 0;   //Requested velocity of spacecraft.
static int16_t vy = 0;
static int16_t vxapp = 0;   //Applied velocity of spacecraft.
static int16_t vyapp = 0;
static int16_t health = 255; //Ship health
static int16_t energy = 255; //Ship energy

// Properties for bullets
#define NBULLET 8  // maximum good-guy bullets
#define NBULLET_TIMER_MAX 8 // Sets interval for new bullets when fire button is held
static const uint8_t bullet_v = 4; //Bullet Speed (not implemented -- fixed at the moment)
static int16_t bvx = 0;                      //Requested velocity
static int16_t bvy = 0;
static int16_t bvxapp = 0;                   //Applied velocity (round-off)
static int16_t bvyapp = 0;
static int16_t bvxrem[NBULLET] = {0};        //Track remaider for smooth motion
static int16_t bvyrem[NBULLET] = {0};
static uint16_t bullet_x[NBULLET] = {0};     //X-position
static uint16_t bullet_y[NBULLET] = {0};     //Y-position
static int8_t bullet_status[NBULLET] = {0};  //Status of bullets
static uint8_t bullet_c = 0;                 //Counter for bullets
static uint16_t bullet_timer = 0;            //delay timer for new bullets

// Properties for enemy bullets
#define NEBULLET 8 // Maximum bad-guy bullets
#define NEBULLET_TIMER_MAX 5 // Sets interval for often the bad guys fire
static uint16_t ebullet_x[NEBULLET] = {0};     //X-position
static uint16_t ebullet_y[NEBULLET] = {0};     //Y-position
static int16_t ebvxrem[NEBULLET] = {0};        //Track remaider for smooth motion
static int16_t ebvyrem[NEBULLET] = {0};
static uint16_t ebullet_timer[NBATTLE_MAX] = {0}; //delay timer for new bullets
static int8_t ebullet_status[NEBULLET] = {0}; //Status of bullets
static uint8_t ebullet_c = 0; //Counter for bullets