#include "demo.h"
#include "constants.h"
#include "input.h"
#include "player.h"
#include "screen.h"
#include "bullets.h"
#include "sbullets.h"
#include "fighters.h"
#include "bkgstars.h"
#include "hud.h"
#include "music.h"
#include "usb_hid_keys.h"
#include "random.h"
#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* forward declarations for functions defined in rpmegafighter.c */
extern void render_game(void);

// Demo configuration
#define DEMO_DURATION_FRAMES (60 * 40) // 40 seconds
// Disable player input simulation for initial demo: set to 1 to enable
#define DEMO_SIMULATE_INPUTS 0

// External globals from rpmegafighter.c
extern int16_t scroll_dx;
extern int16_t scroll_dy;
extern const uint16_t vlen;

// Bullet pool and configuration (defined in other translation units)
extern Bullet bullets[];
extern unsigned BULLET_CONFIG;
extern unsigned SPACECRAFT_CONFIG;

// Lookup tables from definitions.h (defined in one TU)
extern const int16_t sin_fix[25];
extern const int16_t cos_fix[25];
extern const int16_t t2_fix4[25];

// Player exported state (defined in player.c)
extern int16_t player_x;
extern int16_t player_y;
extern int16_t player_vx_applied;
extern int16_t player_vy_applied;
extern int16_t world_offset_x;
extern int16_t world_offset_y;

// Scores (defined in `rpmegafighter.c`)
extern int16_t player_score;
extern int16_t enemy_score;
extern int16_t game_score;

// Input arrays from input.c
extern uint8_t keystates[KEYBOARD_BYTES];
extern gamepad_t gamepad[GAMEPAD_COUNT];

static void demo_update_inputs(unsigned frame)
{
    // No-op when input simulation is disabled.
    // If `DEMO_SIMULATE_INPUTS` is enabled, this function can be
    // implemented to populate `keystates[]`/`gamepad[]` for player control.
#if DEMO_SIMULATE_INPUTS
    // (Simulation code previously lived here.)
#endif
}

void demo_init(void)
{
    // No dynamic initialization for now. Placeholder for future state.
}

void run_demo(void)
{
    printf("Entering demo mode...\n");

    // Stop whatever title music was playing and start gameplay state
    stop_music();

    // Clear screen area
    RIA.addr0 = 0;
    RIA.step0 = 1;
    for (unsigned i = vlen; i--;) {
        RIA.rw0 = 0;
    }

    /* Initialize a fresh game state for demo by initializing public
     * subsystems. `init_game()` in `rpmegafighter.c` is static, so use
     * the publicly-declared initialization functions instead. */
    init_player();
    init_bullets();
    init_sbullets();
    init_fighters();
    init_stars();
    reset_fighter_difficulty();
    reset_music_tempo();
    start_gameplay_music();

    uint8_t vsync_last = RIA.vsync;

    int abort_count = 0;
    unsigned frames = 0;
    while (frames < DEMO_DURATION_FRAMES) {
        // Wait for vertical sync — only advance `frames` when a new
        // vsync is observed so CPU spin loops do not count as frames.
        if (RIA.vsync == vsync_last) {
            continue;
        }
        vsync_last = RIA.vsync;
        ++frames;

        // Read input so we can exit demo early if the player provides input.
        RIA.addr0 = KEYBOARD_INPUT;
        RIA.step0 = 1;
        for (uint8_t i = 0; i < KEYBOARD_BYTES; i++) {
            keystates[i] = RIA.rw0;
        }

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

        // Detect any input (keyboard or gamepad). Ignore keyboard sentinel 0xFF
        // and ignore gamepad connected bit (GP_CONNECTED) — match title-screen behavior.
        bool any_input = false;
        // Check keyboard
        for (uint8_t i = 0; i < KEYBOARD_BYTES; i++) {
            if (keystates[i] && keystates[i] != 0xFF) { any_input = true; break; }
        }
        // Check gamepad 0 quickly
        if (!any_input) {
            if (gamepad[0].dpad != 0xFF) {
                uint8_t gp_dpad_raw = gamepad[0].dpad;
                uint8_t gp_dpad = gp_dpad_raw & ~GP_CONNECTED; // ignore connected bit
                if (gp_dpad || gamepad[0].sticks || gamepad[0].btn0 || gamepad[0].btn1) {
                    any_input = true;
                }
            }
        }

        if (any_input) {
            // Player hit input — exit demo immediately and return to title.
            break;
        }

        // Update music
        update_music();
        
        // Update cooldown timers
        decrement_bullet_cooldown();
        decrement_ebullet_cooldown();
        
        // Enemy bullet system
        fire_ebullet();

        fire_bullet();
        fire_sbullet((uint8_t)get_player_rotation());

        // Update game logic (demo-specific player update)
        update_player(true);

        update_fighters();
        update_bullets();
        update_sbullets();
        update_ebullets();
        
        // Render frame
        render_game();
        draw_hud();
    }

    // Stop demo music, clear screen and return to title
    stop_music();

    // Clear screen area
    RIA.addr0 = 0;
    RIA.step0 = 1;
    for (unsigned i = vlen; i--;) {
        RIA.rw0 = 0;
    }

    // Move all active fighters offscreen
    move_fighters_offscreen();
    
    // Move all bullets offscreen
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].status >= 0) {
            unsigned ptr = BULLET_CONFIG + i * sizeof(vga_mode4_sprite_t);
            xram0_struct_set(ptr, vga_mode4_sprite_t, x_pos_px, -100);
            xram0_struct_set(ptr, vga_mode4_sprite_t, y_pos_px, -100);
            bullets[i].status = -1;
        }
    }
    
    // Move all enemy bullets offscreen
    move_ebullets_offscreen();
    
    // Reset player position to center
    reset_player_position();

    // Restart title music
    start_title_music();

    // Reset scores
    player_score = 0;
    enemy_score = 0;
    game_score = 0;

    printf("Exiting demo mode\n");
}
