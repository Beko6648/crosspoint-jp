#!/usr/bin/env python3
"""Create the license bundle distributed with generated SD-card fonts."""

import argparse
import re
import zipfile
from pathlib import Path

import yaml
from fontTools.ttLib import TTFont


ROOT = Path(__file__).resolve().parent.parent
EPDFONTS_DIR = ROOT / "lib" / "EpdFont"
SCRIPT_DIR = EPDFONTS_DIR / "scripts"
DOWNLOAD_DIR = SCRIPT_DIR / "downloaded_fonts"
LICENSES_DIR = ROOT / "licenses"


def source_path(family_name: str, style: dict) -> Path:
    if "path" in style:
        return EPDFONTS_DIR / style["path"]
    if "url" in style:
        filename = style["url"].rsplit("/", 1)[-1]
        return DOWNLOAD_DIR / family_name / filename
    raise ValueError(f"{family_name}: style has neither path nor url")


def font_notice(path: Path) -> str:
    if not path.exists():
        raise FileNotFoundError(f"font source not found: {path}")
    font = TTFont(str(path), lazy=True)
    try:
        records = font["name"].names
        notices = []
        for record in records:
            if record.nameID != 0:
                continue
            try:
                value = record.toUnicode().strip()
            except UnicodeDecodeError:
                continue
            if value and value not in notices:
                notices.append(value)
        return "\n".join(notices) if notices else "No copyright notice embedded in source font."
    finally:
        font.close()


def safe_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", name)


def build_bundle(config_path: Path, output_path: Path, only: set[str] | None) -> None:
    config = yaml.safe_load(config_path.read_text(encoding="utf-8"))
    default_license = config.get("default_license")
    if default_license != "OFL-1.1":
        raise ValueError("only default_license: OFL-1.1 is currently supported")

    license_path = LICENSES_DIR / "OFL-1.1.txt"
    license_text = license_path.read_text(encoding="utf-8")
    families = config.get("families", [])
    if only is not None:
        families = [family for family in families if family["name"] in only]

    if not families:
        raise ValueError("no font families selected")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output_path, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("OFL-1.1.txt", license_text)
        archive.writestr(
            "README.txt",
            "This archive contains licensing and attribution information for the\n"
            "SD-card .cpfont files distributed with CrossPoint Reader.\n"
            "Each .cpfont is a converted, size-specific font asset.\n",
        )
        for family in families:
            name = family["name"]
            lines = [
                f"Font package: {name}",
                "License: SIL Open Font License 1.1 (OFL-1.1.txt)",
                "",
                "Source files and embedded copyright notices:",
            ]
            seen = set()
            for style_name, style in family.get("styles", {}).items():
                path = source_path(name, style)
                key = str(path)
                if key in seen:
                    continue
                seen.add(key)
                source = style.get("url", str(style.get("path", "")))
                lines.extend(["", f"{style_name}: {source}", font_notice(path)])
            lines.append("")
            archive.writestr(f"ATTRIBUTIONS/{safe_name(name)}.txt", "\n".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser(description="Package SD-font license metadata")
    parser.add_argument("--config", default=SCRIPT_DIR / "sd-fonts.yaml", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--only", help="Comma-separated family names (for local checks)")
    args = parser.parse_args()
    only = set(args.only.split(",")) if args.only else None
    build_bundle(args.config, args.output, only)
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
