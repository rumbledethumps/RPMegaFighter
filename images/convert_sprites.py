#!/usr/bin/env python3

from PIL import Image


def rp6502_rgb_tile_bpp4(r1,g1,b1,r2,g2,b2):
    return (((b1>>7)<<6)|((g1>>7)<<5)|((r1>>7)<<4)|((b2>>7)<<2)|((g2>>7)<<1)|(r2>>7))

def conv_tile(name_in, size, name_out):
    with Image.open(name_in) as im:
        with open("./" + name_out, "wb") as o:
            rgb_im = im.convert("RGB")
            im2 = rgb_im.resize(size=[size,size])
            for y in range(0, im2.height):
                for x in range(0, im2.width, 2):
                    r1, g1, b1 = im2.getpixel((x, y))
                    r2, g2, b2 = im2.getpixel((x+1, y))
                    o.write(
                        rp6502_rgb_tile_bpp4(r1,g1,b1,r2,g2,b2).to_bytes(
                            1, byteorder="little", signed=False
                        )
                    )

def rp6502_rgb_sprite_bpp16(r,g,b):
    if r==0 and g==0 and b==0:
        return 0
    else:
        return ((((b>>3)<<11)|((g>>3)<<6)|(r>>3))|1<<5)

def conv_spr(name_in, size, name_out):
    with Image.open(name_in) as im:
        with open("./" + name_out, "wb") as o:
            rgb_im = im.convert("RGB")
            im2 = rgb_im.resize(size=[size,size])
            for y in range(0, im2.height):
                for x in range(0, im2.width):
                    r, g, b = im2.getpixel((x, y))
                    o.write(
                        rp6502_rgb_sprite_bpp16(r,g,b).to_bytes(
                            2, byteorder="little", signed=False
                        )
                    )

# Convert all sprites
if __name__ == "__main__":
    conv_spr("spaceship.png", 16, "spaceship.bin")
    conv_spr("enemy1.png", 16, "enemy1.bin")
    conv_spr("Earth.png", 32, "Earth.bin")
    conv_spr("npbattle.png", 8, "npbattle.bin")
    conv_spr("fighter.png", 4, "fighter.bin")
    conv_spr("spaceship2.png", 8, "spaceship2.bin")
    conv_spr("sbullet.png", 4, "sbullet.bin")
    print("All sprites converted successfully!")
