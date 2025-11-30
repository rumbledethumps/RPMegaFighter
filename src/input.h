#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

// ============================================================================
// KEYBOARD SUPPORT
// ============================================================================


// Keyboard state array and macro
extern uint8_t keystates[KEYBOARD_BYTES];
extern bool handled_key;

// Macro to check if a key is pressed
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

// ============================================================================
// GAMEPAD SUPPORT
// ============================================================================



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



// ============================================================================
// BUTTON MAPPING SYSTEM
// ============================================================================

// Game actions that can be mapped
typedef enum {
    ACTION_THRUST,
    ACTION_REVERSE_THRUST,
    ACTION_ROTATE_LEFT,
    ACTION_ROTATE_RIGHT,
    ACTION_FIRE,
    ACTION_SUPER_FIRE,
    ACTION_PAUSE,
    ACTION_COUNT  // Total number of actions
} GameAction;

// Button mapping structure
typedef struct {
    uint8_t keyboard_key;     // USB HID keycode
    uint8_t gamepad_button;   // Which gamepad field (0=dpad, 1=sticks, 2=btn0, 3=btn1)
    uint8_t gamepad_mask;     // Bit mask for the button
} ButtonMapping;

    
// Gamepad Field Offsets (for ButtonMapping.gamepad_button)
#define GP_FIELD_DPAD    0  // D-Pad and Status
#define GP_FIELD_STICKS  1  // Digital Sticks
#define GP_FIELD_BTN0    2  // Face Buttons (A,B,X,Y)
#define GP_FIELD_BTN1    3  // Triggers/Select/Start

// Button mapping arrays (one set per player/gamepad)
extern ButtonMapping button_mappings[GAMEPAD_COUNT][ACTION_COUNT];

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialize input system with default button mappings
void init_input_system(void);

// Quick interactive input test: reports mapped action press/releases and exits on PAUSE
#ifdef INPUT_TEST
void init_input_system_test(void);
#endif

// Read keyboard and gamepad input
void handle_input(void);

// Check if a game action is active for a specific player
bool is_action_pressed(uint8_t player_id, GameAction action);

// Set button mapping for a specific action
void set_button_mapping(uint8_t player_id, GameAction action, 
                       uint8_t keyboard_key, uint8_t gamepad_button, uint8_t gamepad_mask);

// Get current button mapping for an action
ButtonMapping get_button_mapping(uint8_t player_id, GameAction action);

// Reset to default button mappings
void reset_button_mappings(uint8_t player_id);

// Load joystick configuration from file (returns true if successful)
bool load_joystick_config(void);

// Save joystick configuration to file
bool save_joystick_config(void);

#endif // INPUT_H
