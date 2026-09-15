# Layout memory regression

Run `py -3 test/run_layout_memory_test.py --compiler <C++ compiler>` (add `--zig` for Zig).
The test compiles production `ParsedText`, `Hyphenator` and UTF-8 code with `-fno-exceptions`.
Only font metrics, storage declarations and ESP heap observations are stubbed.

It fails each admission point in both writing directions, rejects page callbacks,
fails the nothrow TextBlock allocation, simulates fragmentation and prewarm exhausting
heap, and compares unconsumed words/ruby/style/emphasis/continuation/image metadata.
It also exercises partial flush, soft hyphens and long Latin runs.

This is an admission/ownership test, not proof that all STL allocations are fallible.
It does not emulate the ESP allocator, SD fonts, Expat or on-device page-cache writes.
See [device checks and limitations](../../docs/development/v0.7.5-stability-ja.md).

Generate the device fixtures with `py -3 scripts/generate_v075_layout_test_epubs.py`.
The normal chapter covers layout regression and pending JP #144 comparisons. The
stress chapter intentionally exceeds ordinary paragraph/ruby sizes: a graceful
cache-generation failure is acceptable there; reboot, hang or a successful but
truncated cache is not. Fixture XML/ZIP validation is not EPUBCheck certification.
