from PIL import Image

def rp6502_rgb_sprite_bpp16(r,g,b):
    if r==0 and g==0 and b==0:
        return 0
    else:
        return ((((b>>3)<<11)|((g>>3)<<6)|(r>>3))|1<<5)

with Image.open("sbullet.png") as im:
    with open("sbullet.bin", "wb") as o:
        rgb_im = im.convert("RGB")
        im2 = rgb_im.resize(size=[4,4])
        for y in range(0, 4):
            for x in range(0, 4):
                r, g, b = im2.getpixel((x, y))
                o.write(rp6502_rgb_sprite_bpp16(r,g,b).to_bytes(2, byteorder="little", signed=False))

print("Done - sbullet.bin created")
