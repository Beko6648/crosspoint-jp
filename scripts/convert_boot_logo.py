#!/usr/bin/env python3
"""Convert a 120x120 raster image into CrossPoint's boot-logo assets."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


LOGO_SIZE = (120, 120)
THRESHOLD = 128


def normalize_image(source: Path, allow_resize: bool = False) -> Image.Image:
    with Image.open(source) as opened:
        image = opened.convert("RGBA")

    if image.size != LOGO_SIZE and not allow_resize:
        raise ValueError(
            f"input must be exactly {LOGO_SIZE[0]}x{LOGO_SIZE[1]} pixels "
            f"(got {image.width}x{image.height}); pass --resize to scale it"
        )

    # Transparent pixels are treated as white. The display asset is strictly
    # 1-bit, so threshold after flattening instead of retaining alpha/greyscale.
    background = Image.new("RGBA", image.size, (255, 255, 255, 255))
    background.alpha_composite(image)
    grayscale = background.convert("L")
    if grayscale.size != LOGO_SIZE:
        grayscale = grayscale.resize(LOGO_SIZE, Image.Resampling.LANCZOS)
    return grayscale.point(lambda value: 255 if value >= THRESHOLD else 0, mode="1")


def pack_for_display(image: Image.Image) -> bytes:
    # GfxRenderer/EInkDisplay copies MSB-first, row-packed bytes directly to
    # the portrait framebuffer. The source artwork therefore needs this CCW
    # rotation to retain its intended upright orientation on the panel.
    rotated = image.rotate(90, expand=True)
    pixels = rotated.load()
    packed = bytearray()

    for y in range(rotated.height):
        for x in range(0, rotated.width, 8):
            value = 0
            for bit in range(8):
                if x + bit < rotated.width and pixels[x + bit, y] != 0:
                    value |= 1 << (7 - bit)  # white=1, black=0, MSB first
            packed.append(value)

    expected_size = (LOGO_SIZE[0] // 8) * LOGO_SIZE[1]
    if len(packed) != expected_size:
        raise RuntimeError(f"generated {len(packed)} bytes; expected {expected_size}")
    return bytes(packed)


def make_header(data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 16):
        values = ", ".join(f"0x{value:02x}" for value in data[offset : offset + 16])
        lines.append(f"    {values},")

    return (
        "#pragma once\n"
        "#include <cstdint>\n\n"
        "// Image dimensions: 120x120 (1-bit, white=1, MSB-first, source rotated 90 degrees CCW)\n"
        "static const uint8_t Logo120[] = {\n"
        + "\n".join(lines)
        + "\n};\n"
    )


def main() -> None:
    project_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="120x120 PNG or other Pillow-supported raster image")
    parser.add_argument(
        "--resize",
        action="store_true",
        help="allow high-quality resizing to 120x120 (the input aspect ratio should be square)",
    )
    parser.add_argument(
        "--png-output",
        type=Path,
        default=project_root / "src" / "images" / "Logo120.png",
        help="normalized 1-bit source PNG (default: src/images/Logo120.png)",
    )
    parser.add_argument(
        "--header-output",
        type=Path,
        default=project_root / "src" / "images" / "Logo120.h",
        help="firmware header (default: src/images/Logo120.h)",
    )
    args = parser.parse_args()

    image = normalize_image(args.input, allow_resize=args.resize)
    data = pack_for_display(image)

    args.png_output.parent.mkdir(parents=True, exist_ok=True)
    args.header_output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.png_output, format="PNG", optimize=True)
    args.header_output.write_text(make_header(data), encoding="utf-8", newline="\n")

    print(f"Wrote {args.png_output} ({image.width}x{image.height}, 1-bit)")
    print(f"Wrote {args.header_output} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
