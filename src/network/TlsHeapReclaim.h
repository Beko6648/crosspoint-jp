#pragma once

class GfxRenderer;

// Release caches which are rebuilt lazily before an HTTPS handshake. This
// improves the largest contiguous heap block without changing font selection
// or EPUB cache contents.
void reclaimHeapForTls(GfxRenderer& renderer, const char* tag);
