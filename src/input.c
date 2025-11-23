#include "input.h"
#include "usb_hid_keys.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>

// External references
extern void handle_pause_input(void);
extern gamepad_t gamepad[GAMEPAD_COUNT];

// Keyboard state (defined in definitions.h, but declared here)
uint8_t keystates[KEYBOARD_BYTES] = {0};
bool handled_key = false;

// Button mapping storage
ButtonMapping button_mappings[GAMEPAD_COUNT][ACTION_COUNT];

/**
 * Read keyboard and gamepad input
 */
void handle_input(void)
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
    
    // Handle pause button (moved to pause.c)
    handle_pause_input();
}

/**
 * Initialize input system with default button mappings
 */
void init_input_system(void)
{
    // Set default button mappings for all players
    for (uint8_t player = 0; player < GAMEPAD_COUNT; player++) {
        reset_button_mappings(player);
    }
}

/**
 * Reset to default button mappings for a specific player
 */
void reset_button_mappings(uint8_t player_id)
{
    if (player_id >= GAMEPAD_COUNT) return;
    
    // ACTION_THRUST: Up Arrow or Left Stick Up
    button_mappings[player_id][ACTION_THRUST].keyboard_key = KEY_UP;
    button_mappings[player_id][ACTION_THRUST].gamepad_button = 1; // sticks field
    button_mappings[player_id][ACTION_THRUST].gamepad_mask = GP_LSTICK_UP;
    
    // ACTION_REVERSE_THRUST: Down Arrow or Left Stick Down
    button_mappings[player_id][ACTION_REVERSE_THRUST].keyboard_key = KEY_DOWN;
    button_mappings[player_id][ACTION_REVERSE_THRUST].gamepad_button = 1; // sticks field
    button_mappings[player_id][ACTION_REVERSE_THRUST].gamepad_mask = GP_LSTICK_DOWN;
    
    // ACTION_ROTATE_LEFT: Left Arrow or Left Stick Left
    button_mappings[player_id][ACTION_ROTATE_LEFT].keyboard_key = KEY_LEFT;
    button_mappings[player_id][ACTION_ROTATE_LEFT].gamepad_button = 1; // sticks field
    button_mappings[player_id][ACTION_ROTATE_LEFT].gamepad_mask = GP_LSTICK_LEFT;
    
    // ACTION_ROTATE_RIGHT: Right Arrow or Left Stick Right
    button_mappings[player_id][ACTION_ROTATE_RIGHT].keyboard_key = KEY_RIGHT;
    button_mappings[player_id][ACTION_ROTATE_RIGHT].gamepad_button = 1; // sticks field
    button_mappings[player_id][ACTION_ROTATE_RIGHT].gamepad_mask = GP_LSTICK_RIGHT;
    
    // ACTION_FIRE: Space or A button
    button_mappings[player_id][ACTION_FIRE].keyboard_key = KEY_SPACE;
    button_mappings[player_id][ACTION_FIRE].gamepad_button = 2; // btn0 field
    button_mappings[player_id][ACTION_FIRE].gamepad_mask = GP_BTN_A;
    
    // ACTION_PAUSE: ESC or START button
    button_mappings[player_id][ACTION_PAUSE].keyboard_key = KEY_ESC;
    button_mappings[player_id][ACTION_PAUSE].gamepad_button = 3; // btn1 field
    button_mappings[player_id][ACTION_PAUSE].gamepad_mask = GP_BTN_START;
}

/**
 * Check if a game action is active for a specific player
 */
bool is_action_pressed(uint8_t player_id, GameAction action)
{
    if (player_id >= GAMEPAD_COUNT || action >= ACTION_COUNT) {
        return false;
    }
    
    ButtonMapping* mapping = &button_mappings[player_id][action];
    
    // Check keyboard (player 0 only for now)
    if (player_id == 0 && key(mapping->keyboard_key)) {
        return true;
    }
    
    // Check gamepad
    uint8_t gamepad_value = 0;
    switch (mapping->gamepad_button) {
        case 0: gamepad_value = gamepad[player_id].dpad; break;
        case 1: gamepad_value = gamepad[player_id].sticks; break;
        case 2: gamepad_value = gamepad[player_id].btn0; break;
        case 3: gamepad_value = gamepad[player_id].btn1; break;
    }
    
    return (gamepad_value & mapping->gamepad_mask) != 0;
}

/**
 * Set button mapping for a specific action
 */
void set_button_mapping(uint8_t player_id, GameAction action,
                       uint8_t keyboard_key, uint8_t gamepad_button, uint8_t gamepad_mask)
{
    if (player_id >= GAMEPAD_COUNT || action >= ACTION_COUNT) {
        return;
    }
    
    button_mappings[player_id][action].keyboard_key = keyboard_key;
    button_mappings[player_id][action].gamepad_button = gamepad_button;
    button_mappings[player_id][action].gamepad_mask = gamepad_mask;
}

/**
 * Get current button mapping for an action
 */
ButtonMapping get_button_mapping(uint8_t player_id, GameAction action)
{
    ButtonMapping empty = {0, 0, 0};
    
    if (player_id >= GAMEPAD_COUNT || action >= ACTION_COUNT) {
        return empty;
    }
    
    return button_mappings[player_id][action];
}
