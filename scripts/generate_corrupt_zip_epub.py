"""Generate a safe, synthetic EPUB whose chapter DEFLATE stream is invalid.

The ZIP directory and EPUB metadata remain readable.  Opening the book reaches
the normal chapter extraction path, where decompression must fail gracefully.
The fixture deliberately contains no copyrighted text.
"""

from __future__ import annotations

import struct
import zipfile
from pathlib import Path


OUTPUT = Path(__file__).resolve().parents[1] / "test" / "epubs" / "yomuka_corrupt_chapter_zip.epub"
CHAPTER_NAME = "OEBPS/chapter.xhtml"


def make_epub() -> bytearray:
    """Return a minimal EPUB whose chapter is initially a normal DEFLATE entry."""
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(OUTPUT, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        archive.writestr(
            "META-INF/container.xml",
            """<?xml version='1.0'?>
<container version='1.0' xmlns='urn:oasis:names:tc:opendocument:xmlns:container'>
  <rootfiles><rootfile full-path='OEBPS/content.opf' media-type='application/oebps-package+xml'/></rootfiles>
</container>""",
            compress_type=zipfile.ZIP_DEFLATED,
        )
        archive.writestr(
            "OEBPS/content.opf",
            """<?xml version='1.0' encoding='UTF-8'?>
<package xmlns='http://www.idpf.org/2007/opf' version='3.0' unique-identifier='bookid'>
  <metadata xmlns:dc='http://purl.org/dc/elements/1.1/'><dc:identifier id='bookid'>yomuka-corrupt-zip</dc:identifier><dc:title>Corrupt ZIP test</dc:title><dc:language>en</dc:language></metadata>
  <manifest><item id='chapter' href='chapter.xhtml' media-type='application/xhtml+xml'/></manifest>
  <spine><itemref idref='chapter'/></spine>
</package>""",
            compress_type=zipfile.ZIP_DEFLATED,
        )
        archive.writestr(
            CHAPTER_NAME,
            "<html xmlns='http://www.w3.org/1999/xhtml'><body><p>ZIP failure fixture.</p></body></html>",
            compress_type=zipfile.ZIP_DEFLATED,
        )
    return bytearray(OUTPUT.read_bytes())


def corrupt_chapter_deflate_stream(data: bytearray) -> None:
    """Overwrite the first DEFLATE bytes without changing lengths or offsets."""
    with zipfile.ZipFile(OUTPUT) as archive:
        entry = archive.getinfo(CHAPTER_NAME)
    local_header = entry.header_offset
    signature, *_ignored, filename_len, extra_len = struct.unpack_from("<IHHHHHIIIHH", data, local_header)
    if signature != 0x04034B50 or entry.compress_size < 4:
        raise RuntimeError("Could not locate the chapter's local ZIP data")
    payload_offset = local_header + 30 + filename_len + extra_len
    data[payload_offset : payload_offset + 4] = b"\x00\x00\x00\x00"


def main() -> None:
    data = make_epub()
    corrupt_chapter_deflate_stream(data)
    OUTPUT.write_bytes(data)
    print(f"Wrote {OUTPUT} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
