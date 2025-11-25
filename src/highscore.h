/*
 * highscore.h - High score system for RPMegaFighter
 */

#ifndef HIGHSCORE_H
#define HIGHSCORE_H

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

// High score structure
typedef struct {
    char name[HIGH_SCORE_NAME_LEN + 1];  // 3 chars + null terminator
    int16_t score;
} HighScore;

// High score functions
void init_high_scores(void);
bool load_high_scores(void);
void save_high_scores(void);
int8_t check_high_score(int16_t score);
void insert_high_score(int8_t position, const char* name, int16_t score);
void draw_high_scores(void);
void get_player_initials(char* name);

#endif // HIGHSCORE_H
