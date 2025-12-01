/*
 * Gamepad Button Mapping Tool for RP6502
 * Maps physical buttons to game actions for any controller
 * Based on RP6502-RIA gamepad specification
 */

#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "definitions.h"
#include "usb_hid_keys.h" 

// Gamepad input structure
static gamepad_t gamepad[GAMEPAD_COUNT];

// Configuration file
#define JOYSTICK_CONFIG_FILE "JOYSTICK.DAT"

// Game actions to map
static const char* game_actions[] = {
    "THRUST (UP)",
    "REVERSE THRUST (DOWN)", 
    "ROTATE LEFT (LEFT)",
    "ROTATE RIGHT (RIGHT)",
    "FIRE (BUTTON 1)",
    "SPREAD SHOT (BUTTON 2)",
    "PAUSE/START (START)",
    NULL  // End marker
};

// Joystick button mapping (for file storage)
typedef struct {
    uint8_t action_id;  // Index into game_actions array
    uint8_t field;      // 0=dpad, 1=sticks, 2=btn0, 3=btn1
    uint8_t mask;       // Bit mask
} JoystickMapping;

static JoystickMapping mappings[10];  // Store up to 10 mappings
static uint8_t num_mappings = 0;

// Wait for any button press and return which one
static bool wait_for_any_button(uint8_t* field, uint8_t* mask)
{
    uint8_t vsync_last = RIA.vsync;
    uint8_t prev_dpad = 0;
    uint8_t prev_sticks = 0;
    uint8_t prev_btn0 = 0;
    uint8_t prev_btn1 = 0;
    
    // Initial read to set baseline
    RIA.addr0 = GAMEPAD_INPUT;
    RIA.step0 = 1;
    prev_dpad = RIA.rw0 & 0x0F;
    prev_sticks = RIA.rw0;
    prev_btn0 = RIA.rw0;
    prev_btn1 = RIA.rw0;
    for (uint8_t i = 0; i < 6; i++) {
        RIA.rw0;  // Skip analog values
    }
    
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        // Read gamepad
        RIA.addr0 = GAMEPAD_INPUT;
        RIA.step0 = 1;
        gamepad[0].dpad = RIA.rw0;
        gamepad[0].sticks = RIA.rw0;
        gamepad[0].btn0 = RIA.rw0;
        gamepad[0].btn1 = RIA.rw0;
        for (uint8_t i = 0; i < 6; i++) {
            RIA.rw0;  // Skip analog values
        }
        
        uint8_t current_dpad = gamepad[0].dpad & 0x0F;
        
        // Detect new button press (not release)
        if ((current_dpad & ~prev_dpad) != 0) {
            *field = 0;
            *mask = current_dpad & ~prev_dpad;
            return true;
        }
        if ((gamepad[0].sticks & ~prev_sticks) != 0) {
            *field = 1;
            *mask = gamepad[0].sticks & ~prev_sticks;
            return true;
        }
        if ((gamepad[0].btn0 & ~prev_btn0) != 0) {
            *field = 2;
            *mask = gamepad[0].btn0 & ~prev_btn0;
            return true;
        }
        if ((gamepad[0].btn1 & ~prev_btn1) != 0) {
            *field = 3;
            *mask = gamepad[0].btn1 & ~prev_btn1;
            return true;
        }
        
        prev_dpad = current_dpad;
        prev_sticks = gamepad[0].sticks;
        prev_btn0 = gamepad[0].btn0;
        prev_btn1 = gamepad[0].btn1;
    }
}

// Get button name for display
static const char* get_button_name(uint8_t field, uint8_t mask)
{
    static char name[32];
    
    if (field == 0) {
        if (mask & 0x01) return "D-PAD UP";
        if (mask & 0x02) return "D-PAD DOWN";
        if (mask & 0x04) return "D-PAD LEFT";
        if (mask & 0x08) return "D-PAD RIGHT";
    } else if (field == 1) {
        if (mask & 0x01) return "LEFT STICK UP";
        if (mask & 0x02) return "LEFT STICK DOWN";
        if (mask & 0x04) return "LEFT STICK LEFT";
        if (mask & 0x08) return "LEFT STICK RIGHT";
        if (mask & 0x10) return "RIGHT STICK UP";
        if (mask & 0x20) return "RIGHT STICK DOWN";
        if (mask & 0x40) return "RIGHT STICK LEFT";
        if (mask & 0x80) return "RIGHT STICK RIGHT";
    } else if (field == 2) {
        if (mask & 0x01) return "BUTTON A/CROSS";
        if (mask & 0x02) return "BUTTON B/CIRCLE";
        if (mask & 0x04) return "BUTTON C";
        if (mask & 0x08) return "BUTTON X/SQUARE";
        if (mask & 0x10) return "BUTTON Y/TRIANGLE";
        if (mask & 0x20) return "BUTTON Z";
        if (mask & 0x40) return "L1/LEFT SHOULDER";
        if (mask & 0x80) return "R1/RIGHT SHOULDER";
    } else if (field == 3) {
        if (mask & 0x01) return "L2/LEFT TRIGGER";
        if (mask & 0x02) return "R2/RIGHT TRIGGER";
        if (mask & 0x04) return "SELECT/BACK";
        if (mask & 0x08) return "START/MENU";
        if (mask & 0x10) return "HOME";
        if (mask & 0x20) return "L3/LEFT STICK BTN";
        if (mask & 0x40) return "R3/RIGHT STICK BTN";
    }
    
    sprintf(name, "UNKNOWN (field=%d mask=0x%02X)", field, mask);
    return name;
}

int main(void)
{
    printf("\n=== RP6502 Gamepad Button Mapping Tool ===\n");
    printf("Map your controller buttons to game actions\n\n");
    
    // Enable keyboard
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    
    // Enable gamepad input
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);
    
    // Wait for any button press to start
    printf("=== CONTROLLER DETECTION ===\n");
    printf("Press any button on your controller to begin...\n\n");
    
    uint8_t vsync_last = RIA.vsync;
    bool controller_detected = false;
    
    while (!controller_detected) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        RIA.addr0 = GAMEPAD_INPUT;
        RIA.step0 = 1;
        gamepad[0].dpad = RIA.rw0;
        gamepad[0].sticks = RIA.rw0;
        gamepad[0].btn0 = RIA.rw0;
        gamepad[0].btn1 = RIA.rw0;
        for (uint8_t i = 0; i < 6; i++) {
            RIA.rw0;
        }
        
        // Check if any button is pressed
        if ((gamepad[0].dpad & 0x8F) ||  // Connected bit OR any dpad/stick/button
            gamepad[0].sticks ||
            gamepad[0].btn0 ||
            gamepad[0].btn1) {
            controller_detected = true;
            printf("Controller detected!\n");
            
            // Check if Sony controller
            if (gamepad[0].dpad & GP_SONY) {
                printf("Type: PlayStation (Circle/Cross/Square/Triangle)\n");
            } else {
                printf("Type: Generic/Xbox (A/B/X/Y)\n");
            }
        }
    }
    
    // Map each action
    printf("\n=== BUTTON MAPPING ===\n");
    printf("For each action, press the button you want to use.\n");
    printf("You can assign the same button to multiple actions.\n\n");
    
    uint8_t action_id = 0;
    for (const char** action = game_actions; *action != NULL; action++, action_id++) {
        printf("Action: %s\n", *action);
        printf("  Press button: ");
        fflush(stdout);
        
        uint8_t field, mask;
        wait_for_any_button(&field, &mask);
        
        // Store mapping
        mappings[num_mappings].action_id = action_id;
        mappings[num_mappings].field = field;
        mappings[num_mappings].mask = mask;
        num_mappings++;
        
        printf("%s\n\n", get_button_name(field, mask));
        
        // Wait for button release
        vsync_last = RIA.vsync;
        while (true) {
            if (RIA.vsync == vsync_last)
                continue;
            vsync_last = RIA.vsync;
            
            RIA.addr0 = GAMEPAD_INPUT;
            RIA.step0 = 1;
            uint8_t d = RIA.rw0 & 0x0F;
            uint8_t s = RIA.rw0;
            uint8_t b0 = RIA.rw0;
            uint8_t b1 = RIA.rw0;
            for (uint8_t i = 0; i < 6; i++) {
                RIA.rw0;
            }
            
            bool released = true;
            if (field == 0 && (d & mask)) released = false;
            if (field == 1 && (s & mask)) released = false;
            if (field == 2 && (b0 & mask)) released = false;
            if (field == 3 && (b1 & mask)) released = false;
            
            if (released) break;
        }
    }
    
        // Save the file
        FILE* fp = fopen(JOYSTICK_CONFIG_FILE, "wb");
        if (fp) {
            fwrite(&num_mappings, sizeof(uint8_t), 1, fp);
            fwrite(mappings, sizeof(JoystickMapping), num_mappings, fp);
            fclose(fp);
            printf("\n\nConfiguration saved to %s!\n", JOYSTICK_CONFIG_FILE);
        } else {
            printf("\n\nError: Could not save configuration.\n");
        }
        
        printf("Press any key to exit...\n");    // Display summary
    printf("\n=== MAPPING SUMMARY ===\n");
    for (uint8_t i = 0; i < num_mappings; i++) {
        printf("%-20s -> %s (field=%d, mask=0x%02X)\n",
               game_actions[mappings[i].action_id],
               get_button_name(mappings[i].field, mappings[i].mask),
               mappings[i].field,
               mappings[i].mask);
    }
    
    printf("\n=== MAPPING COMPLETE ===\n");
    printf("Configuration saved to %s\n", JOYSTICK_CONFIG_FILE);
    printf("Press any key to exit...\n");
    
    // Wait for key press
    while (true) {
        if (RIA.vsync == vsync_last)
            continue;
        vsync_last = RIA.vsync;
        
        RIA.addr0 = KEYBOARD_INPUT;
        uint8_t any_key = 0;
        for (int i = 0; i < 32; i++) {
            any_key |= RIA.rw0;
        }
        if (any_key) break;
    }
    
    return 0;
}
