#!/usr/bin/env python3

import sys
import os
import argparse
import colorsys
from PIL import Image

def rp6502_rgb_tile_bpp4(r1, g1, b1, r2, g2, b2):
    return (((b1 >> 7) << 6) | ((g1 >> 7) << 5) | ((r1 >> 7) << 4) | 
            ((b2 >> 7) << 2) | ((g2 >> 7) << 1) | (r2 >> 7))

def rp6502_rgb_sprite_bpp16(r, g, b):
    if r == 0 and g == 0 and b == 0:
        return 0
    else:
        # 16-bit RGB555 (1 bit alpha, 5 red, 5 green, 5 blue)
        # Format: A BBBBB GGGGG RRRRR
        return ((((b >> 3) << 11) | ((g >> 3) << 6) | (r >> 3)) | 1 << 5)

def convert_image(image_path, output_path, mode):
    try:
        with Image.open(image_path) as im:
            rgb_im = im.convert("RGB")
            width, height = rgb_im.size

            print(f"Processing: {image_path} ({width}x{height})")
            
            # --- BITMAP MODE (Indexed 8-bit + Palette) ---
            if mode == 'bitmap':
                print(f"Mode:       8-bit Indexed Color (Palette)")
                
                # 1. Quantize to 256 colors (Adaptive Palette)
                #    This calculates the BEST 256 colors for your specific image.
                p_im = im.convert("P", palette=Image.ADAPTIVE, colors=256)
                
                # 2. Extract and Convert Palette
                #    Pillow returns [r,g,b, r,g,b...] flat list
                raw_pal = p_im.getpalette() 

                # Get range of indices actually used by pixels
                min_idx, max_idx = p_im.getextrema()
                print(f"Indices used by image: {min_idx} to {max_idx}")

                # Calculate how many safe slots you have at the end
                print(f"Safe UI indices: {max_idx + 1} to 255")

                # We need exactly 256 entries. Pillow might return fewer if image is simple.
                # Pad it just in case.
                while len(raw_pal) < 768:
                    raw_pal.extend([0, 0, 0])

                # --- PALETTE INJECTION ---
                
                # Reserved: 0-15 are used by the Image.
                
                # Section A: GRAYSCALE (Indices 16-31)
                # Useful for Stars
                print("Injecting Grayscale ramp at indices 16-31")
                for i in range(16):
                    # Value from 50 (dim) to 255 (bright)
                    val = 50 + int((i / 15) * 205)
                    idx = 16 + i
                    raw_pal[idx*3]   = val # R
                    raw_pal[idx*3+1] = val # G
                    raw_pal[idx*3+2] = val # B

                # Section B: RAINBOW (Indices 32-255)
                # Useful for High Scores / UI
                print("Injecting Rainbow spectrum at indices 32-255")
                start_idx = 32
                count = 256 - start_idx
                for i in range(count):
                    hue = i / count
                    # HSV to RGB (Full Saturation, Full Value)
                    r, g, b = colorsys.hsv_to_rgb(hue, 1.0, 1.0)
                    
                    idx = start_idx + i
                    raw_pal[idx*3]   = int(r * 255)
                    raw_pal[idx*3+1] = int(g * 255)
                    raw_pal[idx*3+2] = int(b * 255)
                
                # -------------------------

                # --- PRINT PALETTE DEBUG INFO ---
                print("-" * 30)
                print("Palette Colors:")
                black_indices = []
                
                for i in range(256):
                    r = raw_pal[i*3]
                    g = raw_pal[i*3+1]
                    b = raw_pal[i*3+2]
                    
                    # Store Black Indices
                    if r == 0 and g == 0 and b == 0:
                        black_indices.append(i)
                    
                    # Print the first 16 colors so you can check your mapping
                    if i <= 15:
                        print(f"Index {i:3}: ({r:3}, {g:3}, {b:3})")
                
                print("-" * 30)
                if black_indices:
                    print(f"BLACK (0,0,0) found at indices: {black_indices}")
                    print(f"Recommended Black Index: {black_indices[0]}")
                else:
                    print("WARNING: No pure black found in palette.")
                print("-" * 30)
                # -------------------------------------
                
                pal_filename = os.path.splitext(output_path)[0] + "_pal.bin"
                print(f"Output Img: {output_path}")
                print(f"Output Pal: {pal_filename}")

                # Save Palette File (512 bytes)
                with open(pal_filename, "wb") as pal_out:
                    for i in range(256):
                        r = raw_pal[i*3]
                        g = raw_pal[i*3+1]
                        b = raw_pal[i*3+2]
                        # Pack to RP6502 16-bit format
                        val = rp6502_rgb_sprite_bpp16(r, g, b)
                        pal_out.write(val.to_bytes(2, "little"))

                # Save Image Indices (57600 bytes)
                with open(output_path, "wb") as o:
                    # Get pixel indices directly
                    pixels = list(p_im.getdata())
                    for p in pixels:
                        o.write(p.to_bytes(1, "little"))
                
                print("Done.")
                return

            # --- SPRITE / TILE MODES ---
            sprite_size = height
            
            # Validation
            if width % height != 0:
                print(f"Error: Image width ({width}) must be a multiple of height ({height}).")
                sys.exit(1)
                
            num_frames = width // height
            print(f"Layout:     {num_frames} frames of {sprite_size}x{sprite_size}")
            print(f"Output:     {output_path} [{mode}]")

            with open(output_path, "wb") as o:
                for i in range(num_frames):
                    base_x = i * sprite_size
                    if mode == 'tile':
                        if sprite_size % 2 != 0:
                            print("Error: Sprite size must be even for Tile mode.")
                            sys.exit(1)
                        for y in range(sprite_size):
                            for x in range(base_x, base_x + sprite_size, 2):
                                r1, g1, b1 = rgb_im.getpixel((x, y))
                                r2, g2, b2 = rgb_im.getpixel((x+1, y))
                                o.write(rp6502_rgb_tile_bpp4(r1, g1, b1, r2, g2, b2).to_bytes(1, "little"))
                    else:
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
    parser = argparse.ArgumentParser(description="Convert images to RP6502 binary format.")
    parser.add_argument("input_file", help="Input PNG image.")
    parser.add_argument("-o", "--output", help="Output BIN file.")
    parser.add_argument("--mode", choices=['sprite', 'tile', 'bitmap'], default='sprite', 
                        help="Mode: 'sprite' (16-bit), 'tile' (4-bit), or 'bitmap' (8-bit indexed).")

    args = parser.parse_args()

    if not args.output:
        args.output = os.path.splitext(args.input_file)[0] + ".bin"

    convert_image(args.input_file, args.output, args.mode)

if __name__ == "__main__":
    main()