<?php

declare(strict_types=1);

it('defines the fill rules, the span size, the sample count and the coordinate limit', function (): void {
    expect(RASTER_NON_ZERO)->toBe(0)
        ->and(RASTER_EVEN_ODD)->toBe(1)
        ->and(RASTER_SPAN_BYTES)->toBe(7)
        ->and(RASTER_SAMPLES)->toBe(16)
        ->and(RASTER_LIMIT)->toBe(2 ** 30);
});

it('numbers the batch ops one to seven', function (): void {
    expect([RASTER_OP_PATH, RASTER_OP_ELLIPSE, RASTER_OP_RING, RASTER_OP_ROUND_RECT, RASTER_OP_ROUND_RING, RASTER_OP_SHADOW, RASTER_OP_POLYLINE])->toBe(range(1, 7));
});
