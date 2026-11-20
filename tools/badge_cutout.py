# The launcher's badges cut out (assets/launcher/README.md; docs/launcher.md, The cards):
#   python3 tools/badge_cutout.py <generated badge on black>.png <out>.png   (Pillow)
# Make a centred round badge's black surround transparent: flood the near-black background in from the edges, feather the edge.
import sys
from PIL import Image, ImageFilter, ImageDraw
src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGB'); w, h = im.size
g = im.convert('L')
mask = Image.new('L', (w, h), 0)
# background = pixels reachable from the border through near-black (<= 28)
bg = g.point(lambda v: 255 if v <= 28 else 0)
seed = Image.new('L', (w, h), 0)
filled = bg.copy()
for xy in [(0,0),(w-1,0),(0,h-1),(w-1,h-1),(w//2,0),(w//2,h-1),(0,h//2),(w-1,h//2)]:
    if filled.getpixel(xy) == 255: ImageDraw.floodfill(filled, xy, 128)
alpha = filled.point(lambda v: 0 if v == 128 else 255)
alpha = alpha.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(1.2))
out = im.convert('RGBA'); out.putalpha(alpha)
out.save(dst)
# crop to the badge and pad square, so every badge fills its box the same
out = Image.open(dst); bb = out.getchannel('A').point(lambda v: 255 if v > 8 else 0).getbbox()
out = out.crop(bb); s = max(out.size); sq = Image.new('RGBA', (s, s), (0, 0, 0, 0))
sq.paste(out, ((s - out.width) // 2, (s - out.height) // 2)); sq.resize((320, 320), Image.LANCZOS).save(dst)
