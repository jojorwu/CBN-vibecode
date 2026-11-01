#!/usr/bin/env python3
"""Generate gfx/ui_icons/sidebar_icons.png, the sprite sheet used by the "icons" sidebar layout.

Each icon is 16x16 pixel art drawn below as character rows. The order of ICONS must match
`enum class ui_icon` in src/ui_icons.h. Requires Pillow: `pip install pillow`.

Usage: tools/gen_sidebar_icons.py [--preview out.png]
"""

import argparse
import pathlib
import sys

from PIL import Image

SPRITE = 16
COLUMNS = 8

PALETTE = {
    ".": (0, 0, 0, 0),
    "K": (24, 20, 28, 255),  # outline
    "R": (214, 48, 49, 255),  # red
    "r": (255, 140, 140, 255),  # red highlight
    "D": (140, 20, 25, 255),  # dark red
    "Y": (250, 204, 40, 255),  # yellow
    "y": (255, 240, 150, 255),  # pale yellow
    "O": (225, 135, 25, 255),  # orange
    "G": (70, 190, 80, 255),  # green
    "L": (160, 235, 150, 255),  # light green
    "B": (60, 120, 220, 255),  # blue
    "b": (140, 190, 255, 255),  # light blue
    "N": (30, 55, 130, 255),  # navy
    "W": (240, 240, 240, 255),  # white
    "w": (175, 175, 185, 255),  # light gray
    "s": (110, 110, 122, 255),  # gray
    "T": (150, 90, 40, 255),  # brown
    "t": (205, 145, 85, 255),  # tan
}

ICONS = {
    "health": [
        ".KKK....KKK.",
        "KRrRK..KRRRK",
        "KrrRRKKRRRRK",
        "KrRRRRRRRRDK",
        "KRRRRRRRRRDK",
        "KRRRRRRRRRDK",
        ".KRRRRRRRDK.",
        "..KRRRRRDK..",
        "...KRRRDK...",
        "....KRDK....",
        ".....KK.....",
    ],
    "stamina": [
        "......KKKKK",
        ".....KyYYYK",
        "....KyYYYK.",
        "...KyYYYK..",
        "..KyYYYKKKK",
        ".KYYYYYYYYK",
        "KKKKKYYYOK.",
        "...KYYYOK..",
        "..KYYOK....",
        ".KYOK......",
        "KOK........",
        "KK.........",
    ],
    "speed": [
        "KK...KK....",
        "KGK..KGK...",
        "KLGK.KLGK..",
        ".KLGK.KLGK.",
        "..KLGK.KLGK",
        "..KGGK.KGGK",
        ".KGGK.KGGK.",
        "KGGK.KGGK..",
        "KGK..KGK...",
        "KK...KK....",
    ],
    "mood": [
        "....KKKKK....",
        "..KKyyyyYKK..",
        ".KyyYYYYYYYK.",
        ".KyYYYYYYYYK.",
        "KyYYKYYYKYYOK",
        "KYYYKYYYKYYOK",
        "KYYYYYYYYYYOK",
        "KYKYYYYYYYKOK",
        "KYYKYYYYYKYOK",
        ".KYYKKKKKYOK.",
        ".KYYYYYYYOOK.",
        "..KKOOOOOKK..",
        "....KKKKK....",
    ],
    "focus": [
        "....KKKKKKK....",
        "..KKWWWWWWWKK..",
        ".KWWWWKKKWWWWK.",
        "KWWWWKBBBKWWWWK",
        "KWWWKBbKBBKWWwK",
        "KWWWKBKKKBKWWwK",
        "KWWWKBBKBBKWWwK",
        "KWWWWKBBBKWWWwK",
        ".KWWWWKKKWWWwK.",
        "..KKWWWWWWwKK..",
        "....KKKKKKK....",
    ],
    "power": [
        "...KKKK...",
        "...KwwK...",
        ".KKKKKKKK.",
        ".KwwwwwsK.",
        ".KwGGGGsK.",
        ".KwGLGGsK.",
        ".KwGGGGsK.",
        ".KwsssssK.",
        ".KwGGGGsK.",
        ".KwGLGGsK.",
        ".KwGGGGsK.",
        ".KwsssssK.",
        ".KwGGGGsK.",
        ".KwGGGGsK.",
        ".KKKKKKKK.",
    ],
    "mana": [
        ".....KK.....",
        "....KbbK....",
        "...KbbBBK...",
        "..KbbBBBBK..",
        ".KbbBBBBBNK.",
        "KbbBBBBBBNNK",
        "KbBBBBBBBNNK",
        ".KBBBBBBNNK.",
        "..KBBBBBNK..",
        "...KBBBNK...",
        "....KBNK....",
        ".....KK.....",
    ],
    "safe": [
        ".KKKKKKKKKK.",
        "KwwwwwwwwwsK",
        "KwBBBBBBBBsK",
        "KwBBBBBBWBsK",
        "KwBBBBBWWBsK",
        "KwBWBBWWBBsK",
        "KwBWWWWBBBsK",
        "KwBBWWBBBBsK",
        ".KwBBBBBBsK.",
        ".KwBBBBBBsK.",
        "..KwBBBBsK..",
        "...KwBBsK...",
        "....KssK....",
        ".....KK.....",
    ],
    "hunger": [
        ".....KKKK...",
        "...KKtTTTK..",
        "..KtttTTTTK.",
        ".KtttTTTTTTK",
        ".KttTTTTTTTK",
        "KttTTTTTTTTK",
        "KtTTTTTTTTK.",
        "KTTTTTTTTK..",
        ".KTTTTTKK...",
        "..KKKKWK....",
        ".....KWK....",
        "....KWWWK...",
        "...KWKKWK...",
        "...KK..KK...",
    ],
    "thirst": [
        ".....KK.....",
        "....KbbK....",
        "....KbBK....",
        "...KbBBBK...",
        "..KbBBBBBK..",
        ".KbBBBBBBBK.",
        ".KbBBBBBBBK.",
        "KbWBBBBBBBNK",
        "KbWBBBBBBBNK",
        "KbBWBBBBBNNK",
        ".KBBBBBBBNK.",
        ".KNBBBBBNNK.",
        "..KKNNNNKK..",
        "....KKKK....",
    ],
    "rest": [
        "...KKKKK.....",
        ".KKyyyyK..KKK",
        ".KyyyKK...KyK",
        "KyyyK....KyK.",
        "KyyK.....KKK.",
        "KyyK.........",
        "KyyK.........",
        "KyyyK........",
        "KyyyyKK...KK.",
        ".KyyyyyKKKyK.",
        ".KKyyyyyyyK..",
        "...KKKKKKK...",
    ],
    "pain": [
        ".....KK.....",
        ".K..KRRK..K.",
        ".KK.KRRK.KK.",
        ".KRKKRRKKRK.",
        "..KRRrrRRK..",
        "KKKRrrrrRKKK",
        "KRRRrWWrRRRK",
        "KKKRrrrrRKKK",
        "..KRRrrRRK..",
        ".KRKKRRKKRK.",
        ".KK.KRRK.KK.",
        ".K..KRRK..K.",
        ".....KK.....",
    ],
    "heat": [
        "...KKK...",
        "..KwwwK..",
        "..KwRwK..",
        "..KwRwK..",
        "..KwRwK..",
        "..KwRwK..",
        "..KwRwK..",
        "..KwRwK..",
        ".KwRRRwK.",
        "KwRRrRRwK",
        "KwRrRRRwK",
        "KwRRRRRwK",
        ".KwRRRwK.",
        "..KKKKK..",
    ],
    "sound": [
        ".....KK.....",
        "....KwK..W..",
        "...KwwK...W.",
        "KKKwwwK.W..W",
        "KsswwwK..W.W",
        "KsswwwK..W.W",
        "KKKwwwK.W..W",
        "...KwwK...W.",
        "....KwK..W..",
        ".....KK.....",
    ],
    "place": [
        "......KK......",
        ".....KRRK.....",
        "....KRRRRK....",
        "...KRRRRRRK...",
        "..KRRRRRRRRK..",
        ".KRRRRRRRRRRK.",
        "KKKKKKKKKKKKKK",
        ".KttttttttttK.",
        ".KtKKKtKKKttK.",
        ".KtKbKtKTKttK.",
        ".KtKKKtKTKttK.",
        ".KttttttKTKtK.",
        ".KKKKKKKKKKKK.",
    ],
    "sky": [
        ".....YY.....",
        ".Y...YY...Y.",
        "..Y......Y..",
        "....KKKK....",
        "...KOYYOK...",
        "YY.KYyyYK.YY",
        "YY.KYyyYK.YY",
        "...KOYYOK...",
        "....KKKK....",
        "..Y......Y..",
        ".Y...YY...Y.",
        ".....YY.....",
    ],
    "underground": [
        "...KKKKKKK...",
        ".KKwwwwwwwKK.",
        "KwwsKKTKKswwK",
        "KsK..KTK..KsK",
        ".K...KTK...K.",
        ".....KTK.....",
        ".....KTK.....",
        ".....KTK.....",
        ".....KTK.....",
        ".....KtK.....",
        ".....KTK.....",
        ".....KKK.....",
    ],
    "light": [
        "...KKKKKK...",
        "..KyyyyyyK..",
        ".KyyYYYYyyK.",
        ".KyYYYYYYyK.",
        ".KyYYYYYYOK.",
        ".KyYYYYYYOK.",
        "..KyYYYYOK..",
        "...KyYYOK...",
        "...KKKKKK...",
        "...KwwwwK...",
        "...KssssK...",
        "...KwwwwK...",
        "....KKKK....",
    ],
    "time": [
        "....KKKKK....",
        "..KKWWWWWKK..",
        ".KWWWWKWWWWK.",
        ".KWWWWKWWWWK.",
        "KWWWWWKWWWWwK",
        "KWWWWWKWWWWwK",
        "KWWWWWKKKKWwK",
        "KWWWWWWWWWWwK",
        "KWWWWWWWWWWwK",
        ".KWWWWWWWWwK.",
        ".KWWWWWWWwwK.",
        "..KKwwwwwKK..",
        "....KKKKK....",
    ],
    "date": [
        "..KK....KK..",
        "KKKKKKKKKKKK",
        "KRRRRRRRRRRK",
        "KRRRRRRRRRRK",
        "KKKKKKKKKKKK",
        "KWWWWWWWWWWK",
        "KWsWsWsWsWWK",
        "KWWWWWWWWWWK",
        "KWsWsWsWsWWK",
        "KWWWWWWWWWWK",
        "KWsWsWRRWWWK",
        "KWWWWWRRWWWK",
        "KKKKKKKKKKKK",
    ],
    "weapon": [
        "............KK",
        "...........KWK",
        "..........KWwK",
        ".........KWwK.",
        "........KWwK..",
        ".......KWwK...",
        "..KK..KWwK....",
        "..KTKKWwK.....",
        "...KTWwK......",
        "....KTK.......",
        "...KTKTK......",
        "..KTK.KTK.....",
        ".KTK...KK.....",
        "KYK...........",
        "KK............",
    ],
    "style": [
        "..KKKKKKK...",
        ".KtKtKtKtK..",
        ".KtKtKtKtKK.",
        "KKtKtKtKtKtK",
        "KtttttttttTK",
        "KttKKKKKttTK",
        "KtttttttttTK",
        ".KtttttttTK.",
        "..KttttttK..",
        "..KTTTTTTK..",
        "..KKKKKKKK..",
    ],
    "weight": [
        "...KKKKKK...",
        "..KwKKKKwK..",
        "..KwK..KwK..",
        "..KwK..KwK..",
        "..KKwKKwKK..",
        ".KwwwwwwwwK.",
        "KwWWwwwwwwsK",
        "KwWwwwwwwwsK",
        "KwwwwwwwwwsK",
        "KwwwwwwwwssK",
        ".KswwwwwssK.",
        "..KKKKKKKK..",
    ],
    "volume": [
        "...KKKKKK...",
        "..KK....KK..",
        ".KTTTTTTTTK.",
        "KTttttttttTK",
        "KTttttttttTK",
        "KTKKKKKKKKTK",
        "KTtKKKKKKtTK",
        "KTtKttttKtTK",
        "KTtKtYYtKtTK",
        "KTtKKKKKKtTK",
        "KTttttttttTK",
        ".KKKKKKKKKK.",
    ],
}


def render_icon(name: str, rows: list[str]) -> Image.Image:
    width = len(rows[0])
    bad = [i for i, row in enumerate(rows) if len(row) != width]
    if bad or width > SPRITE or len(rows) > SPRITE:
        sys.exit(f"icon '{name}': inconsistent or oversized rows {bad}")
    img = Image.new("RGBA", (SPRITE, SPRITE))
    ox = (SPRITE - width) // 2
    oy = (SPRITE - len(rows)) // 2
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            img.putpixel((ox + x, oy + y), PALETTE[ch])
    return img


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--preview", help="also write a 4x upscaled preview on a dark background")
    args = parser.parse_args()

    root = pathlib.Path(__file__).resolve().parent.parent
    count = len(ICONS)
    sheet_rows = (count + COLUMNS - 1) // COLUMNS
    sheet = Image.new("RGBA", (COLUMNS * SPRITE, sheet_rows * SPRITE))
    for index, (name, rows) in enumerate(ICONS.items()):
        pos = ((index % COLUMNS) * SPRITE, (index // COLUMNS) * SPRITE)
        sheet.paste(render_icon(name, rows), pos)

    out = root / "gfx" / "ui_icons" / "sidebar_icons.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    print(f"wrote {out} ({count} icons)")

    if args.preview:
        preview = Image.new("RGBA", sheet.size, (12, 12, 16, 255))
        preview.alpha_composite(sheet)
        preview.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST).save(args.preview)


if __name__ == "__main__":
    main()
