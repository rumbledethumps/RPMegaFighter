#ifndef PAUSE_H
#define PAUSE_H

#include <stdint.h>
#include <stdbool.h>

// Display or clear the pause message on screen
void display_pause_message(bool show_paused);

// Handle pause button input and update pause state
void handle_pause_input(void);

// Check if game is currently paused
bool is_game_paused(void);

// Set game pause state
void set_game_paused(bool paused);

// Reset pause state (call when starting new game)
void reset_pause_state(void);

// Check if exit combination is pressed while paused
bool check_pause_exit(void);

#endif // PAUSE_H
