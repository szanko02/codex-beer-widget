"""Generate project-owned vector-style icons. Requires Pillow only for asset generation."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets' / 'icons'
OUT.mkdir(parents=True, exist_ok=True)
SIZE = 256

def make(kind, accent):
    im = Image.new('RGBA', (SIZE, SIZE))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((12, 12, 244, 244), radius=54, fill='#132432')
    if kind == 'widget':
        d.rounded_rectangle((153, 76, 213, 171), radius=22, outline='#DBEBF2', width=15)
        d.rounded_rectangle((52, 54, 167, 205), radius=16, fill='#DBEBF2')
        d.rounded_rectangle((64, 85, 155, 191), radius=8, fill=accent)
        d.rounded_rectangle((64, 67, 155, 94), radius=10, fill='#FFF4CF')
        d.line((84, 111, 84, 169), fill='#FFF4CF', width=9)
    elif kind == 'probe':
        d.arc((46, 46, 210, 210), 135, 405, fill='#DBEBF2', width=17)
        d.line((128, 131, 181, 82), fill=accent, width=17)
        d.ellipse((114, 117, 142, 145), fill=accent)
        d.rounded_rectangle((99, 183, 157, 201), radius=8, fill=accent)
    else:
        d.line((77, 63, 47, 63, 47, 193, 77, 193), fill='#DBEBF2', width=14, joint='curve')
        d.line((179, 63, 209, 63, 209, 193, 179, 193), fill='#DBEBF2', width=14, joint='curve')
        d.line((87, 129, 118, 160, 175, 96), fill=accent, width=18, joint='curve')
    im.save(OUT / f'{kind}.png')
    im.save(OUT / f'{kind}.ico', sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
    return im

images = [make('widget', '#EFAE36'), make('probe', '#5BBFD4'), make('tests', '#8BC59B')]
sheet = Image.new('RGB', (840, 360), '#F3F6F8')
for i, im in enumerate(images):
    sheet.paste(im, (24 + i * 276, 28), im)
    ImageDraw.Draw(sheet).text((70 + i * 276, 308), ['Widget', 'Quota probe', 'Test tools'][i], fill='#132432', font_size=24)
sheet.save(OUT / 'preview.png')
