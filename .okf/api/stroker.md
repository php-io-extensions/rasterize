---
type: API
title: Stroker
description: RasterStroker: a polyline into the contours of its stroke, and into the pieces a dash pattern leaves; the joins, the caps, the arithmetic, what it refuses.
resource: stubs/RasterStroker.stub.php
tags: [rasterize, stroke, dash, geometry, c]
status: draft
generated: { by: claude-fable/5.2, at: 2026-11-13T12:00:00Z }
sources:
  - id: stub
    resource: stubs/RasterStroker.stub.php
    title: RasterStroker.stub.php
  - id: core
    resource: src/stroke.h
    title: The stroker
---

# Overview

Geometry only: points in, contours or pieces out, no pixel. `src/stroke.{h,c}` is plain C (memory through the two function pointers the class passes); `src/RasterStroker.c` is the class over it, `RasterStroker` with two static methods and a private constructor.[^stub] Hand the contours to `RasterScanner::path()` with `RASTER_NON_ZERO`.[^core]

Points are lists of `[x, y]`, ints or floats, each finite with magnitude at most `RASTER_LIMIT`. Answers are lists of floats.

# Constants

`RASTER_CAP_BUTT` 0, `RASTER_CAP_ROUND` 1, `RASTER_CAP_SQUARE` 2. `RASTER_JOIN_MITER` 0, `RASTER_JOIN_ROUND` 1, `RASTER_JOIN_BEVEL` 2. `RASTER_MITER_LIMIT` 4.0.

# outline()

`outline(points, stroke, closed, cap = BUTT, join = MITER, miterLimit = 4.0, direction = null)`: `half = stroke / 2`. A stroke that is not above 0, a number or not, answers `[]`: before anything else is read, so a bad point is not refused either.

1. **Points.** Each point equal to the one before it is dropped (`==` on both numbers). A closed polyline whose last point equals its first loses the last, when more than two remain. `closed` holds only with more than two points left.
2. **Fewer than two points left.** One point, with at least two given, under a round cap: a *dot*, a polygon of `s = ceil(steps(2π, half) / 4) · 4` points `(x + half·cos θ, y + half·sin θ)`, `θ = 2π·i / s`. Under a square cap: the four points `(x − ux + nx, y − uy + ny)`, `(x + ux + nx, y + uy + ny)`, `(x + ux − nx, y + uy − ny)`, `(x − ux − nx, y − uy − ny)`, `u = direction · half` (along x when none), `n = (−uy, ux)`. A dot is answered as it is made, not turned to positive area. Under a butt cap, or with one point given: `[]`.
3. **Segments.** For each segment a to b, `length = sqrt(dx² + dy²)` (a length of 0 is a `DivisionByZeroError`), `u = (dx, dy) / length`, `n = (−uy·half, ux·half)`; the quad `a + n, b + n, b − n, a − n`. A closed polyline has one more segment, from the last point to the first.
4. **Joins**, in the order of the corners: a quad per corner, in the cases below, where `in` and `out` are the unit directions of the segments that meet at `v`. Nothing when `in × out == 0` (straight on or straight back). `side = −1` when the cross product is above 0, else 1; the outer normals `a = side · (−in_y, in_x) · half` and `b = side · (−out_y, out_x) · half`.
   * **Bevel**: `v, v + a, v + b`.
   * **Round**: `v, v + a`, then `from = atan2(a_y, a_x)`, `sweep = atan2(b_y, b_x) − from` brought into `−π..π`, `steps = steps(|sweep|, half)`, and for `i = 1 .. steps − 1` the point `v + half·(cos θ, sin θ)`, `θ = from + sweep·i / steps`; then `v + b`.
   * **Miter**: `m = (a + b) / half`, `q = m·m`. When `q · limit² < 4` it is the bevel; otherwise `reach = 2·half / q` and `v, v + a, v + m·reach, v + b`.
5. **Caps**, for an open polyline with a cap other than butt: at the first point with `outward` the first direction reversed, then at the last with the last direction. `n = (−outward_y·half, outward_x·half)`. Square: `p + n, p + n + o, p − n + o, p − n` with `o = outward · half`. Round: `steps = steps(π, half)` made even, and for `i = 0 .. steps` the point `p + half·(cos θ, sin θ)`, `θ = atan2(n_y, n_x) − π·i / steps`.
6. **Orientation.** Each contour's twice signed area `Σ (x_i·y_j − x_j·y_i)` (`j = i + 1`, wrapping). Above 0: kept. Below 0: its points reversed. 0 or not a number: dropped.

`steps(sweep, radius)`: 2 when `radius ≤ 0.1`, else `max(2, ceil(sweep / (2 · acos(1 − 0.1 / radius))))`.

# dashed()

`dashed(points, closed, dash, offset)`: the points are not deduplicated. The pattern is `dash`, repeated once when its count is odd. `total` is the sum of the pattern; `[]` when it is not above 0, or when no element is above `1e-12`, or when no two consecutive points differ (the walk's first direction is the first segment with `hypot(dx, dy) > 0`).

A closed polyline with more than two points is walked once round (the first point is appended). The state at the start: `phase = fmod(offset, total)`, plus `total` when below 0; elements are taken from the pattern while `phase > element`, or `phase == element` and `element > 0`, each subtracted; `left` is what remains of the element.

*Settling*: while `left ≤ 1e-12` the walk is on an element of no length: an on element (even index) leaves a piece of two equal points at the walk's position; the next element begins. Then, on an on element, a piece begins at the position.

For each segment of length `hypot(dx, dy) > 0`, direction `(dx, dy) / length`: while `position < length`, `take = min(left, length − position)`, `position += take`, `left −= take`, the position `a + (b − a)·position / length`; a piece being made takes the point and the direction. When `left ≤ 1e-12`: a piece being made ends (kept when it has two points or more); the next element begins; settle.

At the end: a piece still being made with two points or more ends. When the polyline is closed, the walk started on an element that is on and ends on one, and some piece ended: the last piece takes the first piece's points after its first point and its direction, and replaces the first. When none ended, the answer is the whole polyline, `closed`, as given, with the last direction.

# Refusals

`ValueError`: a point that is not `[x, y]` of two numbers, a coordinate or a stroke above 0 or an offset that is not finite or is past `RASTER_LIMIT`, a cap or join that is not a constant, a direction that is not `[x, y]`, a dash length that is not a number, or is not finite, or is below 0. `DivisionByZeroError`: a segment whose length is 0, a stroke so thin that half of it is 0, a miter of a corner whose normals sum to 0.

# Arithmetic

IEEE double with `+ − × ÷ sqrt floor ceil`, fixed order, `-ffp-contract=off`, and the C library's `hypot`, `fmod`, `atan2`, `acos`, `sin` and `cos`, with `-fno-builtin-sin -fno-builtin-cos` so the compiler does not join a `sin` and `cos` of one angle into a `sincos` call. A PHP implementation in the same order answers the same bits on one machine: held by 16,000 random polylines and patterns. Another C library may differ in the last bit of `sin`, `cos`, `atan2` and `acos`.

[^stub]: `stubs/RasterStroker.stub.php` is the declaration; arginfo is generated from it.
[^core]: `stroke.h` documents the contours and the pieces.
