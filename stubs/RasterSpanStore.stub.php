<?php

/** @generate-class-entries */

/**
 * Span lists scanned once and kept in C, each by a number, then painted many times at whole-pixel
 * offsets into an FbBuffer through ext-fb's span sink: what a glyph cache keeps. The spans never
 * come back to PHP.
 *
 * A list is scanned as RasterScanner::path() scans inside a clip (0, 0, width, height), with this
 * store's edges. Contours and their errors are path()'s.
 *
 * @not-serializable
 */
final class RasterSpanStore
{
    /** Antialias: as RasterScanner's. */
    public function __construct(bool $antialias) {}

    public function antialias(): bool {}

    /**
     * The contours scanned inside (0, 0, width, height) and their spans kept. Answers the list's
     * number, 0 or more, or -1 when the contours cover no pixel and nothing is kept. A number
     * removed is handed out again.
     *
     * @param array $contours A list of contours, each a flat list x, y, x, y… of numbers, closed implicitly.
     * @throws ValueError When a side is not 1..65535, the rule unknown, or the contours are not path()'s.
     */
    public function add(array $contours, int $width, int $height, int $rule = RASTER_NON_ZERO): int {}

    /** @throws ValueError When no list kept has that number. */
    public function remove(int $id): void {}

    /** Every list dropped. */
    public function clear(): void {}

    /** How many lists are kept. */
    public function count(): int {}

    /** The bytes the kept spans take. */
    public function size(): int {}

    /**
     * Kept lists painted into an FbBuffer at one 0xRRGGBBAA colour, in order: each moved by (x, y),
     * cut to the clip, then all of them painted as one FbBuffer::paintSpans() paints. The clip lies
     * inside the buffer; everything is checked before anything is painted. Error when ext-fb is not
     * loaded (RasterScanner::fills()).
     *
     * @param array $placements A flat list number, x, y, number, x, y…: whole numbers, x and y within ±2^30.
     * @return array|null [x, y, width, height] of what it painted, or null for nothing.
     * @throws ValueError When a placement names no kept list or is past the reach, the colour is not 0xRRGGBBAA, or the clip is empty or not inside.
     */
    public function fill(array $placements, int $rgba, object $target, int $clipX, int $clipY, int $clipWidth, int $clipHeight): ?array {}
}
