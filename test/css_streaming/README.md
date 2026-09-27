# Streaming CSS cache

Run `py -3 test/run_css_streaming_test.py --compiler <zig.exe> --zig`.
Production CssParser is compiled with deterministic storage/heap stubs. Tests cover rule registration order across CSS files, duplicate selector property overrides, 2200 unused selectors with no retained rule map during parsing, usage-filtered restore, empty stylesheets, low heap, failed initial/record writes, count seek failure, rename failure, and abandonment cleanup. Existing text_emphasis tests cover codec roundtrip, every truncated byte prefix, and style/render regression.

The zero-map assertion checks retained rule storage, not all allocations or ESP32 peak heap. Input strings, parser buffers, normalized selectors and SD buffers still exist. Real EPUB, device fragmentation and SD timing require device validation.
