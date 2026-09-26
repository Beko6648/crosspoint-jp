# Batch cache safety tests

Run `py -3 test/run_batch_cache_test.py --compiler <zig.exe> --zig`.
Compiles production CssParser/CssSelectorUsage and CacheProgressPolicy with host storage/heap stubs.
Checks progress at book transitions and timed chapter boundaries (including uint32 wrap), rejection and clearing of partial CSS in strict mode, unchanged ordinary-reader fallback, retry, empty stylesheet cache, and missing cache.
This does not emulate Epub ZIP extraction, Section publication, SD I/O failure or the physical progress popup. Those require integration/device checks.
