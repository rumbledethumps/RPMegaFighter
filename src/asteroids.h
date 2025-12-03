#ifndef ASTEROIDS_H
#define ASTEROIDS_H

#include <stdint.h>
#include <stdbool.h>

// Types
typedef enum {
    AST_LARGE = 0,
    AST_MEDIUM,
    AST_SMALL
} AsteroidType;

// Object Structure
typedef struct {
    bool active;
    int16_t x, y;       // World Position
    int16_t rx, ry;     // Sub-pixel remainders (for smooth movement)
    int16_t vx, vy;     // Velocity (Speed)
    uint8_t anim_frame; // For rotation/animation
    int8_t health;      // Hit points
    AsteroidType type;
} asteroid_t;

// Pools
#define MAX_AST_L 2
#define MAX_AST_M 4
#define MAX_AST_S 8

extern asteroid_t ast_l[MAX_AST_L];
extern asteroid_t ast_m[MAX_AST_M];
extern asteroid_t ast_s[MAX_AST_S];

// Functions
void init_asteroids(void);
void spawn_asteroid_wave(int level); // Call every frame
void update_asteroids(void);         // Call every frame
void move_asteroids_offscreen(void); // Move all asteroids offscreen (for screen transitions)

// Returns true if the bullet hit an asteroid (so the bullet should die)
bool check_asteroid_hit(int16_t x, int16_t y);

// Returns true if the fighter at (fx, fy) crashed into a rock
bool check_asteroid_hit_fighter(int16_t fx, int16_t fy);

// Checks collision between Player and all Asteroids
// Modifies scores and destroys asteroids if hit
void check_player_asteroid_collision(int16_t px, int16_t py);

#endif