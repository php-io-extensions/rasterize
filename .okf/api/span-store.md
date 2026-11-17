---
type: API
title: Span store
description: RasterSpanStore: span lists scanned once and kept in C by number, painted many times at whole-pixel offsets into an FbBuffer through ext-fb's span sink; what it refuses.
resource: stubs/RasterSpanStore.stub.php
tags: [rasterize, spans, cache, glyphs, fb, c]
status: draft
generated: { by: claude-opus/5.5, at: 2026-11-17T02:10:00Z }
sources:
  - id: stub
    resource: stubs/RasterSpanStore.stub.php
    title: RasterSpanStore.stub.php
  - id: class
    resource: src/RasterSpanStore.c
    title: The class
---

# Overview

What a glyph cache keeps: a shape scanned once, its coverage kept in C, painted wherever it is placed. The spans never come back to PHP: `add()` answers a number, `fill()` paints by number. `src/RasterSpanStore.c` is the class over the scanner's `raster_path()`; it paints through the span sink ext-fb exports, as `RasterScanner::fill()` does, and needs ext-fb only for `fill()`.[^class]

# Methods

| Method | Does |
|---|---|
| `__construct(bool $antialias)` | The edges every list is scanned with: `RasterScanner`'s. A second construction is an `Error`. |
| `add(array $contours, int $width, int $height, int $rule = RASTER_NON_ZERO): int` | Scans the contours as `RasterScanner::path()` does inside a clip `(0, 0, width, height)` and keeps the spans at their own length. Answers the list's number (a removed number is handed out again first), or `-1` when they cover no pixel and nothing is kept. Sides 1..65535. |
| `remove(int $id)`, `clear()` | Drop one list, or every list. |
| `count()`, `size()` | Lists kept; the bytes their spans take (7 a span). |
| `fill(array $placements, int $rgba, object $target, int $clipX, int $clipY, int $clipWidth, int $clipHeight): ?array` | `$placements` is a flat list `number, x, y, …` of whole numbers. Each list is moved by `(x, y)`, each span cut to the clip (rows outside it dropped, ends cut), and every list's spans painted in one call to the sink: as `FbBuffer::paintSpans()` paints them, in placement order, so overlapping placements blend one over the other. Answers `[x, y, width, height]` of what it painted, or `null`. |

Everything is checked before anything is painted.[^stub]

# Refusals

* `add()`: a side outside 1..65535, a rule that is not `RASTER_NON_ZERO` or `RASTER_EVEN_ODD`, contours that `path()` refuses (`ValueError`).
* `remove()`, `fill()`: a number no kept list has (`ValueError`).
* `fill()`: placements not a list of threes or not whole numbers, an offset past ±2^30, a colour past 32 bits, a clip empty or not inside the buffer (`ValueError`); a target that is not a constructed `FbBuffer` (`TypeError`); ext-fb not loaded (`Error`).

[^stub]: RasterSpanStore.stub.php
[^class]: src/RasterSpanStore.c
