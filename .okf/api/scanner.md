---
type: API
title: Scanner
description: Constants, RasterScanner: every method, the span bytes, the edge rules, what it refuses.
resource: stubs/
tags: [rasterize, spans, c]
status: draft
generated: { by: claude-opus/5.5, at: 2026-10-03T22:56:49Z }
sources:
  - id: stubs
    resource: stubs/
    title: rasterize.stub.php, RasterScanner.stub.php
  - id: core
    resource: src/core.h
    title: The scanning core
---

# Overview

Geometry only. No pixels, no stores, no framework names, no dependency.[^stubs]

Layers: `src/core.{h,c}` plain C (paths, ellipses, rings, one-pixel lines, the row scan; no Zend; memory through two function pointers the class passes). `src/RasterScanner.c` the Zend class over it. `src/rasterize.c` module + constants.[^core]

# Constants

`RASTER_NON_ZERO` 0, `RASTER_EVEN_ODD` 1, `RASTER_SPAN_BYTES` 7, `RASTER_SAMPLES` 16, `RASTER_LIMIT` 2³⁰.

# RasterScanner

`__construct(x, y, width, height, antialias)`: clip inside 0..65535, sides at least 1.

| Call | Does |
|---|---|
| `clip()`, `antialias()` | what it was made with |
| `path(array $contours, int $rule)` | contours: a list of flat `x, y…` lists, each closed implicitly; non-zero or even-odd |
| `ellipse(cx, cy, rx, ry)` | axis-aligned; nothing when a radius is not positive |
| `ring(cx, cy, rx, ry, stroke)` | band between `r + stroke / 2` and `r − stroke / 2`; inner part dropped when an inner radius is not positive |
| `polyline(array $points, bool $closed)` | one-pixel lines between floored points, every pixel once, coverage 255 in either mode |

Answer: span bytes, 7 each, little-endian `y`, `x`, `length` (uint16), `coverage` (uint8); rows ascending, `x` ascending, no overlap, coverage 1..255; empty string for nothing.

Refused (`ValueError`): clip outside 0..65535 or empty; a contour or point list that is not a list or holds an odd count; a non-number; a non-finite value; a value past `RASTER_LIMIT`; a rule other than the two. Final, no clone, no serialize, second `__construct` is `Error`.

# Rules

Pixel `(x, y)` covers `[x, x+1) × [y, y+1)`.

* Hard: one sample line per row at `y + 0.5`; an interval `[a, b)` covers pixels `ceil(a − 0.5) .. ceil(b − 0.5) − 1`.
* Anti-aliased: 16 sample lines at `y + (k + 0.5) / 16`; each interval adds its horizontal overlap with each pixel, weighted 1/16; a difference array carries full runs; coverage `floor(min(sum, 1) × 255 + 0.5)`; equal neighbours join.
* Path edge crosses line `y` when `min(y0, y1) ≤ y < max(y0, y1)`, at `x0 + (y − y0) × slope`, `slope = (x1 − x0) / (y1 − y0)`; crossings sort by x then edge order.
* Ellipse at line `y`: `t = (y − cy) / ry`, `h = rx × sqrt(1 − t²)` when `1 − t² > 0`, interval `[cx − h, cx + h)`.
* One-pixel line: along the major axis from the lower end, minor offset `floor((2·|d_minor|·i + |d_major|) / (2·|d_major|))`; only the clipped stretch is walked, so far-off endpoints cost nothing.

Arithmetic: IEEE double, `+ − × ÷ sqrt floor ceil`, fixed order, `-ffp-contract=off` (without it clang fuses seven multiply-adds in `core.c`, `1 − t²` among them). A PHP implementation in the same order answers the same bytes; Surface's native scanner is one.

[^stubs]: The stubs are the declaration of record; arginfo is generated from them.
[^core]: `core.h` documents the span bytes and the memory hooks.
