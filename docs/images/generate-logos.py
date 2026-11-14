"""ロゴを、モザイクのマスで描いて SVG と 512x512 の PNG に書き出す。

使い方 (リポジトリのルートで実行する):
    .venv/Scripts/python -X utf8 docs/images/generate-logos.py

マス (四角形) の並びを 1 つ作り、同じ並びを SVG と PNG の両方に描く。
出力は docs/images/logo.svg・logo.png と、配信プラグイン倉庫くんに載せる .soko-kun/logo.png。
依頼者が 3 つの案から選んだ「二つの場面」の案を描く。
"""

from __future__ import annotations

import colorsys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 512
RADIUS = 96
OUT = Path(__file__).resolve().parent

Rect = tuple[int, int, int, int, tuple[int, int, int]]


def hsv(h: float, s: float, v: float) -> tuple[int, int, int]:
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, s, v)
    return round(r * 255), round(g * 255), round(b * 255)


def design_two_scenes() -> tuple[str, list[Rect]]:
    """粗いモザイクの前の場面 (暖色) と次の場面 (寒色) を、黒い帯で切り替える"""
    rects: list[Rect] = []
    cell = 64
    for gx in range(6):
        for gy in range(6):
            x, y = 64 + gx * cell, 64 + gy * cell
            if gx < 3:
                color = hsv(0.03 + 0.03 * ((gx * 7 + gy * 3) % 4), 0.8, 0.95 - 0.08 * gy)
            else:
                color = hsv(0.55 + 0.03 * ((gx * 5 + gy * 2) % 4), 0.7, 0.95 - 0.08 * gy)
            rects.append((x, y, cell, cell, color))
    rects.append((240, 64, 32, 384, (12, 12, 16)))
    return "#f2efe8", rects


def write_svg(path: Path, background: str, rects: list[Rect]) -> None:
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{SIZE}" height="{SIZE}" viewBox="0 0 {SIZE} {SIZE}">',
        f'<rect width="{SIZE}" height="{SIZE}" rx="{RADIUS}" fill="{background}"/>',
    ]
    for x, y, w, h, (r, g, b) in rects:
        parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="#{r:02x}{g:02x}{b:02x}"/>')
    parts.append("</svg>")
    path.write_text("\n".join(parts) + "\n", encoding="utf-8", newline="\n")


def write_png(path: Path, background: str, rects: list[Rect]) -> None:
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([0, 0, SIZE - 1, SIZE - 1], radius=RADIUS, fill=background)
    for x, y, w, h, color in rects:
        draw.rectangle([x, y, x + w - 1, y + h - 1], fill=color)
    image.save(path)


def main() -> None:
    background, rects = design_two_scenes()
    write_svg(OUT / "logo.svg", background, rects)
    write_png(OUT / "logo.png", background, rects)
    soko_kun = OUT.parent.parent / ".soko-kun"
    soko_kun.mkdir(exist_ok=True)
    write_png(soko_kun / "logo.png", background, rects)
    print("wrote docs/images/logo.svg, docs/images/logo.png and .soko-kun/logo.png")


if __name__ == "__main__":
    main()
