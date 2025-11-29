#!/usr/bin/env python3

import sys
import os
import argparse
from PIL import Image

def rp6502_rgb_tile_bpp4(r1, g1, b1, r2, g2, b2):
    """ Converts 2 pixels into one byte (4bpp). """
    return (((b1 >> 7) << 6) | ((g1 >> 7) << 5) | ((r1 >> 7) << 4) | 
            ((b2 >> 7) << 2) | ((g2 >> 7) << 1) | (r2 >> 7))

def rp6502_rgb_sprite_bpp16(r, g, b):
    """ Converts 1 pixel into two bytes (16bpp). """
    if r == 0 and g == 0 and b == 0:
        return 0
    else:
        # 0x20 (bit 5) is transparency; must be 1 for opaque
        return ((((b >> 3) << 11) | ((g >> 3) << 6) | (r >> 3)) | 1 << 5)

def convert_image(image_path, output_path, mode):
    try:
        with Image.open(image_path) as im:
            rgb_im = im.convert("RGB")
            width, height = rgb_im.size
            
            # Logic: Sprite size is determined by image height (Square Sprites)
            sprite_size = height
            
            # Validation
            if width % height != 0:
                print(f"Error: Image width ({width}) must be a multiple of height ({height}).")
                print("       Input must be a horizontal strip of square sprites.")
                sys.exit(1)
                
            num_frames = width // height

            print(f"Processing: {image_path}")
            print(f"Dimensions: {width}x{height} -> {num_frames} frames of {sprite_size}x{sprite_size}")
            print(f"Output:     {output_path} [{mode}]")

            with open(output_path, "wb") as o:
                # Loop through each frame horizontally
                for i in range(num_frames):
                    base_x = i * sprite_size
                    
                    if mode == 'tile':
                        # Tile Mode (4bpp)
                        if sprite_size % 2 != 0:
                            print("Error: Sprite size must be even for Tile mode.")
                            sys.exit(1)
                            
                        for y in range(sprite_size):
                            # Step by 2 pixels for 4bpp packing
                            for x in range(base_x, base_x + sprite_size, 2):
                                r1, g1, b1 = rgb_im.getpixel((x, y))
                                r2, g2, b2 = rgb_im.getpixel((x+1, y))
                                o.write(
                                    rp6502_rgb_tile_bpp4(r1, g1, b1, r2, g2, b2).to_bytes(1, "little")
                                )
                    else:
                        # Sprite Mode (16bpp)
                        for y in range(sprite_size):
                            for x in range(base_x, base_x + sprite_size):
                                r, g, b = rgb_im.getpixel((x, y))
                                val = rp6502_rgb_sprite_bpp16(r, g, b)
                                o.write(val.to_bytes(2, "little"))
            
            print("Done.")

    except FileNotFoundError:
        print(f"Error: File '{image_path}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"An error occurred: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Convert horizontal sprite strips to RP6502 binary.")
    
    parser.add_argument("input_file", help="Input PNG (horizontal strip).")
    parser.add_argument("-o", "--output", help="Output BIN file.")
    parser.add_argument("--mode", choices=['sprite', 'tile'], default='sprite', 
                        help="Mode: 'sprite' (16-bit) or 'tile' (4-bit).")

    args = parser.parse_args()

    if not args.output:
        args.output = os.path.splitext(args.input_file)[0] + ".bin"

    convert_image(args.input_file, args.output, args.mode)

if __name__ == "__main__":
    main()
