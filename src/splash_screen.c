#include <rp6502.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

// Assuming this is defined globally in your project.
// If not, you may need to extern it or pass it in.
// extern unsigned BITMAP_CONFIG;

extern void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);

void load_file_to_xram(const char *filename, unsigned address) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Error: Could not open %s\n", filename);
        return;
    }

    uint8_t buffer[256];
    int bytes_read;
    
    RIA.addr0 = address;
    RIA.step0 = 1; 

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < bytes_read; i++) {
            RIA.rw0 = buffer[i];
        }
    }
    close(fd);
}

void show_splash_screen(void) {
    const uint8_t red_color = 0x03;
    const uint16_t center_x = 110;

    // 1. Load the Palette to Free Space (0xF000)
    // The palette file is 512 bytes (256 colors * 2 bytes)
    load_file_to_xram("title_screen_pal.bin", 0xF000);

    // 2. Load the Bitmap Indices to Background (0x0000)
    load_file_to_xram("title_screen.bin", 0x0000);

    // 3. Configure the Video Plane to use the Palette
    // We update the BITMAP_CONFIG struct in XRAM.
    // In Mode 3 Config, the palette pointer is at a specific offset.
    // struct vga_mode3_config_t:
    //   ...
    //   uint16_t data_ptr;    (offset 6)
    //   uint16_t palette_ptr; (offset 8)
    
    // Set Palette Pointer to 0xF000
    // xram0_struct_set(BITMAP_CONFIG, vga_mode3_config_t, palette_ptr, 0xF000);


    // Overlay Text
    // draw_text(center_x, 110, "PRESS START", red_color);
}