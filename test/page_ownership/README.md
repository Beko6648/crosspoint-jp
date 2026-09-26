# Page ownership host tests

Run `py -3 test/run_page_ownership_test.py --compiler <zig.exe> --zig`.

Compiles production Page.cpp and TableRowBlock.cpp against host storage and text/image payload-codec stubs. Checks ownership destruction, rejected missing payloads, object allocation failure, short page headers/coordinates/footnotes, and byte-identical page-envelope roundtrip. Also checks that a later table row can render after the earlier page is destroyed, and that the shared column layout is released after its last row. This does not test production image/text payload codecs, parser table pagination, or physical rendering.

The existing layout_memory suite compiles production ParsedText and tests horizontal/vertical acceptance failure, preserved remaining source metadata, and retained output after ParsedText destruction. Its renderer stub now includes current measurement APIs; failure indices count layout probes separately from input construction.
