---
okf_version: "0.2"
---

# ext-rasterize

* [Scanner](api/scanner.md) - Constants, RasterScanner: every method, the span bytes, the edge rules, what it refuses.
* [Span store](api/span-store.md) - RasterSpanStore: span lists kept in C by number and painted, moved, into an FbBuffer through ext-fb's sink.
* [Stroker](api/stroker.md) - RasterStroker: a polyline into the contours of its stroke, and into the pieces a dash pattern leaves; the joins, the caps, the arithmetic, what it refuses.

# Runbooks

* [Build, install, test](runbooks/build.md) - Scratch build with gen_stub, the two installers, Pest, the Pi loop.
