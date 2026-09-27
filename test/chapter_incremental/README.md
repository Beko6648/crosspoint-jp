# Incremental parse driver test
Compiles the actual lifecycle/step method definitions extracted from ChapterHtmlSlimParser.cpp with the bundled Expat C implementation. Stubs storage, memory admission and layout callbacks; checks identical UTF-8/element event streams between synchronous and sliced operation, one-shot finalization, cancellation, heap and IO failures, malformed XML and tolerated trailing data. This does not validate production page geometry, Section publication or UI thread scheduling; those require firmware/device regression.


Run `py -3 test/run_chapter_incremental_test.py --compiler <zig.exe>` for driver, font budget, diagnostic pause and PNG draw recovery tests.
Run `test/run_image_cancel_test.py`, `test/run_chapter_exit_test.py` and `test/run_chapter_input_lock_test.py` with the same `--compiler` argument for image cancellation and reader lifecycle/input lock regressions.
The image cancellation test uses the production output adapter and callback cleanup fragment with fake storage; it does not prove SD deletion success or actual button latency during decompression.
