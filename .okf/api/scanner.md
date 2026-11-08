---
type: API
title: Scanner
description: Constants, RasterScanner: every method, the span bytes, the edge rules, what it refuses.
resource: stubs/
tags: [rasterize, spans, c]
status: draft
generated: { by: claude-opus/5.5, at: 2026-11-05T00:00:00Z }
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

Layers: `src/core.{h,c}` plain C (paths, ellipses, rings, rounded rects, rounded rings, shadows, one-pixel lines, the row scan; no Zend; memory through two function pointers the class passes). `src/RasterScanner.c` the Zend class over it. `src/rasterize.c` module + constants.[^core]

# Constants

`RASTER_NON_ZERO` 0, `RASTER_EVEN_ODD` 1, `RASTER_SPAN_BYTES` 7, `RASTER_SAMPLES` 16, `RASTER_LIMIT` 2³⁰. Batch ops: `RASTER_OP_PATH` 1, `RASTER_OP_ELLIPSE` 2, `RASTER_OP_RING` 3, `RASTER_OP_ROUND_RECT` 4, `RASTER_OP_ROUND_RING` 5, `RASTER_OP_SHADOW` 6, `RASTER_OP_POLYLINE` 7.

# RasterScanner

`__construct(x, y, width, height, antialias)`: clip inside 0..65535, sides at least 1.

| Call | Does |
|---|---|
| `clip()`, `antialias()` | what it was made with |
| `path(array $contours, int $rule)` | contours: a list of flat `x, y…` lists, each closed implicitly; non-zero or even-odd |
| `ellipse(cx, cy, rx, ry)` | axis-aligned; nothing when a radius is not positive |
| `ring(cx, cy, rx, ry, stroke)` | band between `r + stroke / 2` and `r − stroke / 2`; inner part dropped when an inner radius is not positive |
| `roundRect(x0, y0, x1, y1, radii)` | box `[x0, x1) × [y0, y1)`, radii `[tl, tr, br, bl]` already held to `0..min(w, h)/2`; one interval a line, a corner's end at `corner x ∓ r·sqrt(1 − t²)`, written as the ellipse is (a circle-shaped box answers the ellipse's bytes) |
| `roundRing(x0, y0, x1, y1, radii, inset)` | band between the box and the box inset by `inset` (radii `max(0, r − inset)`); the whole box when the inset box is empty |
| `shadow(x0, y0, x1, y1, radius, blur)` | coverage `1 − smoothstep(−blur, blur, d)` once at each pixel centre, `d` the signed distance to the box rounded by `radius`; blur ≤ 0 = the hard box (in where `d < 0`); hard edges: ≥ 128 → 255. Columns more than `radius + blur + 2` inside both sides: one sample at the box centre x, one run |
| `polyline(array $points, bool $closed)` | one-pixel lines between floored points, every pixel once, coverage 255 in either mode |
| `fills()` (static) | true when ext-fb is loaded and exports its span sink (found at startup through ext-fb's module handle; ext-fb is an optional dependency, so it starts first) |
| `fill(array $ops, array $colours, object $target)` | `batch()`'s ops painted straight into an `FbBuffer` at their colours, as its `paintSpans()` paints them; the clip must lie inside the buffer, every op and colour checked first; one `[x, y, w, h]` or null an op. `Error` without ext-fb |
| `batch(array $ops)` | each op `[RASTER_OP_*, ...that method's arguments, every one given]`; every op read and checked before any is scanned; a list of span strings, one an op, the bytes the single calls answer. Refused: `Op N is not a list that starts with a RASTER_OP_* shape.`, `Op N takes K entries…`, `Op N: <name> is not a number.`, a bad rule or `closed`, and the single calls' own messages |

Answer: span bytes, 7 each, little-endian `y`, `x`, `length` (uint16), `coverage` (uint8); rows ascending, `x` ascending, no overlap, coverage 1..255; empty string for nothing.

Refused (`ValueError`): clip outside 0..65535 or empty; a contour or point list that is not a list or holds an odd count; a non-number; a non-finite value; a value past `RASTER_LIMIT`; a rule other than the two. Final, no clone, no serialize, second `__construct` is `Error`.

# Rules

Pixel `(x, y)` covers `[x, x+1) × [y, y+1)`.

* Hard: one sample line per row at `y + 0.5`; an interval `[a, b)` covers pixels `ceil(a − 0.5) .. ceil(b − 0.5) − 1`.
* Anti-aliased: 16 sample lines at `y + (k + 0.5) / 16`; each interval adds its horizontal overlap with each pixel, weighted 1/16; a difference array carries full runs; coverage `floor(min(sum, 1) × 255 + 0.5)`; equal neighbours join.
* Path edge crosses line `y` when `min(y0, y1) ≤ y < max(y0, y1)`, at `x0 + (y − y0) × slope`, `slope = (x1 − x0) / (y1 − y0)`; crossings sort by x then edge order.
* Ellipse at line `y`: `t = (y − cy) / ry`, `h = rx × sqrt(1 − t²)` when `1 − t² > 0`, interval `[cx − h, cx + h)`.
* One-pixel line: along the major axis from the lower end, minor offset `floor((2·|d_minor|·i + |d_major|) / (2·|d_major|))`; only the clipped stretch is walked, so far-off endpoints cost nothing.

Arithmetic: IEEE double, `+ − × ÷ sqrt floor ceil`, fixed order, `-ffp-contract=off` (without it clang fuses seven multiply-adds in `core.c`, `1 − t²` among them). A PHP implementation in the same order answers the same bytes.

[^stubs]: The stubs are the declaration of record; arginfo is generated from them.
[^core]: `core.h` documents the span bytes and the memory hooks.
