#ifndef POWERUP_H
#define POWERUP_H

#define POWERUP_DATA      0xEE80  // 8x8 Sprite Data (128 bytes) Ends at 0xEF00
#define POWERUP_DURATION_FRAMES  (60 * 10) // Power-up lasts for 10 seconds
#define POWERUP_DROP_CHANCE_PERCENT 5 // 20% chance to drop a power-up on fighter destruction

// Single storage for the POWERUP_CONFIG symbol is provided in `rpmegafighter.c`.
extern unsigned POWERUP_CONFIG;        //Power-up sprite config address

// Power-up structure definition
typedef struct {
	bool active;
	int x, y;
	int vy;
    int timer;
} powerup_t;

extern powerup_t powerup;

// Function declarations
void render_powerup(void);

#endif // POWERUP_H