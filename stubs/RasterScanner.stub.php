<?php

/** @generate-class-entries */

/**
 * Shapes into coverage spans inside one clip. Every method answers span
 * bytes: 7-byte records, little-endian, y (uint16), x (uint16), length
 * (uint16), coverage (uint8); rows ascending, x ascending, never overlapping,
 * coverage 1..255 (255 only with hard edges). Coordinates are finite with
 * magnitude at most RASTER_LIMIT; anything else, or a malformed list, is
 * ValueError.
 *
 * @not-serializable
 */
final class RasterScanner
{
    /** A clip inside 0..65535 with both sides at least 1. Antialias: 16 sample lines a row and exact horizontal coverage; otherwise one sample at each pixel centre. */
    public function __construct(int $x, int $y, int $width, int $height, bool $antialias) {}

    /** @return array [x, y, width, height] */
    public function clip(): array {}

    public function antialias(): bool {}

    /** @param array $contours A list of contours, each a flat list x, y, x, y… of numbers, closed implicitly. */
    public function path(array $contours, int $rule = RASTER_NON_ZERO): string {}

    public function ellipse(float $cx, float $cy, float $rx, float $ry): string {}

    /** The band between radii r + stroke / 2 and r - stroke / 2. */
    public function ring(float $cx, float $cy, float $rx, float $ry, float $stroke): string {}

    /**
     * One-pixel lines between the floored points, every pixel once, coverage 255 whatever the edge mode.
     *
     * @param array $points A flat list x, y, x, y… of numbers.
     */
    public function polyline(array $points, bool $closed = false): string {}
}
