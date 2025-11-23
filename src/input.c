#include "input.h"
#include <rp6502.h>
#include <stdint.h>

// External references
extern void handle_pause_input(void);

// Gamepad structure
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

#define GAMEPAD_COUNT 4
#define KEYBOARD_INPUT 0xEC20
#define GAMEPAD_INPUT 0xEC50
#define KEYBOARD_BYTES 32

extern gamepad_t gamepad[GAMEPAD_COUNT];
extern uint8_t keystates[KEYBOARD_BYTES];

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
