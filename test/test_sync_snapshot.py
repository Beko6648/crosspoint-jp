"""Host reference checks for the v1 contract; not a firmware importer.

Install requirements-sync-snapshot.txt, then run this file directly.
EPUB spine bounds and local font availability require the future importer.
"""

import copy
import json
import struct
from pathlib import Path
import unittest

from jsonschema import Draft202012Validator, ValidationError

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "docs/development/sync"
SCHEMA = json.loads((DATA / "snapshot-v1.schema.json").read_text(encoding="utf-8"))
EXAMPLE = json.loads((DATA / "all-units.example.json").read_text(encoding="utf-8"))
Draft202012Validator.check_schema(SCHEMA)
VALIDATOR = Draft202012Validator(SCHEMA)


def decode_legacy_progress(raw):
    """Reference conversion only. Never opens or changes device storage."""
    if len(raw) not in (4, 6, 7, 8):
        raise ValueError("unsupported progress size")
    spine, page = struct.unpack_from("<HH", raw)
    count = struct.unpack_from("<H", raw, 4)[0] if len(raw) >= 6 else None
    finished = None
    if len(raw) >= 7:
        if raw[6] not in (0, 1):
            raise ValueError("invalid finished flag")
        finished = bool(raw[6])
    percent = None
    if len(raw) == 8 and raw[7] != 255:
        if raw[7] > 100:
            raise ValueError("invalid percent")
        percent = raw[7]
    return dict(spineIndex=spine, chapterPage=page, chapterPageCount=count,
                finished=finished, percent=percent)


def validate_target(document, book_id, spine_count):
    """Read-only reference for identity and chapter bounds, not font resolution."""
    validate(document)
    if document["bookId"] != book_id:
        raise ValueError("different EPUB revision")
    if not 1 <= spine_count <= 65535:
        raise ValueError("unsupported spine count")
    units = document["units"]
    if "progress" in units:
        p = units["progress"]["data"]
        if p["spineIndex"] >= spine_count:
            if not (p["spineIndex"] == spine_count and p["chapterPage"] == 0
                    and p["chapterPageCount"] == 0 and p["finished"] is True
                    and p["percent"] == 100):
                raise ValueError("progress outside target EPUB")
    for bookmark in units.get("bookmarks", {}).get("data", []):
        if bookmark["spineIndex"] >= spine_count:
            raise ValueError("bookmark outside target EPUB")


def validate(document):
    VALIDATOR.validate(document)
    units = document["units"]
    positions = []
    if "progress" in units:
        positions.append(units["progress"]["data"])
    if "bookmarks" in units:
        bookmarks = units["bookmarks"]["data"]
        keys = [(b["spineIndex"], b["chapterPage"]) for b in bookmarks]
        if len(set(keys)) != len(keys):
            raise ValueError("duplicate bookmark location")
        positions.extend(bookmarks)
    for position in positions:
        count = position["chapterPageCount"]
        page = position["chapterPage"]
        if count is not None and (page >= count if count else page != 0):
            raise ValueError("page outside chapter")
    settings = units.get("readerSettings", {}).get("data", {})
    for direction in ("horizontal", "vertical"):
        font = settings.get(direction, {}).get("font")
        if font and len(font["sdFamilyName"].encode("utf-8")) > 31:
            raise ValueError("font name exceeds local UTF-8 storage")


def parse(raw):
    if len(raw) > 65536 or raw.startswith(b"\xef\xbb\xbf"):
        raise ValueError("size or BOM")

    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result

    def constant(value):
        raise ValueError(f"non-JSON constant {value}")

    document = json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)
    validate(document)
    return document


class SnapshotContract(unittest.TestCase):
    def test_legacy_progress_lengths(self):
        raw = struct.pack("<HHHBB", 12, 34, 120, 1, 95)
        for size, count, finished, percent in ((4, None, None, None),
                                              (6, 120, None, None),
                                              (7, 120, True, None),
                                              (8, 120, True, 95)):
            with self.subTest(size=size):
                self.assertEqual(decode_legacy_progress(raw[:size]), dict(
                    spineIndex=12, chapterPage=34, chapterPageCount=count,
                    finished=finished, percent=percent))
        self.assertIsNone(decode_legacy_progress(raw[:7] + b"\xff")["percent"])
        for bad in (raw[:3], raw[:5], raw + b"\0", raw[:6] + b"\x02",
                    raw[:7] + b"\x65"):
            with self.assertRaises(ValueError):
                decode_legacy_progress(bad)

    def test_updated_book_requires_matching_new_id(self):
        updated = copy.deepcopy(EXAMPLE)
        updated["bookId"] = "fedcba9876543210"
        validate_target(updated, "fedcba9876543210", 20)
        with self.assertRaises(ValueError):
            validate_target(EXAMPLE, "fedcba9876543210", 20)
        with self.assertRaises(ValueError):
            validate_target(updated, "fedcba9876543210", 10)

    def test_target_end_marker(self):
        doc = copy.deepcopy(EXAMPLE)
        doc["units"] = {"progress": doc["units"]["progress"]}
        p = doc["units"]["progress"]["data"]
        p.update(spineIndex=20, chapterPage=0, chapterPageCount=0, finished=True, percent=100)
        validate_target(doc, doc["bookId"], 20)
        p["finished"] = False
        with self.assertRaises(ValueError):
            validate_target(doc, doc["bookId"], 20)

    def test_all_units_roundtrip(self):
        self.assertEqual(parse(json.dumps(EXAMPLE).encode()), EXAMPLE)

    def test_each_unit_independent(self):
        for name, value in EXAMPLE["units"].items():
            with self.subTest(name=name):
                doc = copy.deepcopy(EXAMPLE)
                doc["units"] = {name: value}
                validate(doc)

    def test_explicit_clear_and_unknown(self):
        doc = copy.deepcopy(EXAMPLE)
        doc["units"]["bookmarks"]["data"] = []
        doc["units"]["readerSettings"]["data"] = {}
        p = doc["units"]["progress"]["data"]
        p.update(chapterPageCount=None, finished=None, percent=None)
        validate(doc)

    def test_finished_is_not_end_of_book(self):
        doc = copy.deepcopy(EXAMPLE)
        doc["units"]["progress"]["data"].update(finished=True, percent=95)
        validate(doc)

    def test_boundaries(self):
        doc = copy.deepcopy(EXAMPLE)
        doc["exportedAt"] = 4294967295
        p = doc["units"]["progress"]["data"]
        p.update(spineIndex=65535, chapterPage=65534, chapterPageCount=65535, percent=100)
        doc["units"]["history"]["data"].update(seconds=4294967295, sessionCount=4294967295)
        doc["units"]["bookmarks"]["data"] = [
            {**EXAMPLE["units"]["bookmarks"]["data"][0], "chapterPage": i} for i in range(24)
        ]
        validate(doc)
        p.update(chapterPage=0, chapterPageCount=0, finished=True)
        validate(doc)

    def test_reject_invalid_fields(self):
        cases = [
            (("formatVersion",), 2), (("bookId",), 123),
            (("bookId",), "0000000000000000"), (("bookId",), "ABCDEF0123456789"),
            (("exportedAt",), -1), (("units",), {}),
            (("units", "progress", "updatedAt"), 4294967296),
            (("units", "progress", "data", "chapterPage"), 120),
            (("units", "progress", "data", "chapterPageCount"), 0),
            (("units", "progress", "data", "percent"), 101),
            (("units", "progress", "data", "finished"), 1),
            (("units", "readerSettings", "data", "writingMode"), 2),
            (("units", "readerSettings", "data", "vertical", "rubyOffsetX"), 65),
            (("units", "readerSettings", "data", "vertical", "font", "sdFamilyName"), "字" * 11),
            (("units", "readerSettings", "data", "vertical", "font", "sdFamilyName"), "bad\0name"),
            (("units", "history", "data", "seconds"), -1),
            (("units", "history", "data", "sessionCount"), 1.5),
        ]
        for path, value in cases:
            with self.subTest(path=path, value=value):
                doc = copy.deepcopy(EXAMPLE)
                target = doc
                for key in path[:-1]:
                    target = target[key]
                target[path[-1]] = value
                with self.assertRaises((ValueError, ValidationError)):
                    validate(doc)

    def test_reject_missing_or_extra(self):
        for path in [("units", "progress", "updatedAt"),
                     ("units", "readerSettings", "data", "vertical", "font", "family")]:
            doc = copy.deepcopy(EXAMPLE)
            target = doc
            for key in path[:-1]:
                target = target[key]
            del target[path[-1]]
            with self.assertRaises(ValidationError):
                validate(doc)
        for extra in ("wifi", "unknown"):
            doc = copy.deepcopy(EXAMPLE)
            doc[extra] = {}
            with self.assertRaises(ValidationError):
                validate(doc)

    def test_bookmark_limits(self):
        for items in ([EXAMPLE["units"]["bookmarks"]["data"][0]] * 2,
                      [{**EXAMPLE["units"]["bookmarks"]["data"][0], "chapterPage": i} for i in range(25)]):
            doc = copy.deepcopy(EXAMPLE)
            doc["units"]["bookmarks"]["data"] = items
            with self.assertRaises((ValueError, ValidationError)):
                validate(doc)

    def test_invalid_wire_encoding(self):
        good = json.dumps(EXAMPLE).encode()
        for raw in (b" " * 65537, b"\xef\xbb\xbf" + good, b"\xff", b'{"a":1,"a":2}',
                    b'{"x":NaN}', b'{"x":Infinity}'):
            with self.subTest(raw=raw[:20]):
                with self.assertRaises(ValueError):
                    parse(raw)


if __name__ == "__main__":
    unittest.main(verbosity=2)
