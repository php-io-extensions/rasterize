<?php

declare(strict_types=1);

it('holds its clip and edge mode', function (): void {
    $scanner = new RasterScanner(1, 2, 3, 4, true);

    expect($scanner->clip())->toBe([1, 2, 3, 4])
        ->and($scanner->antialias())->toBeTrue()
        ->and((new RasterScanner(0, 0, 1, 1, false))->antialias())->toBeFalse();
});

it('refuses a clip that is empty or reaches past 65535', function (array $clip): void {
    expect(fn () => new RasterScanner(...[...$clip, false]))->toThrow(ValueError::class, 'is empty or not inside 0..65535');
})->with([[[-1, 0, 1, 1]], [[0, -1, 1, 1]], [[0, 0, 0, 1]], [[0, 0, 1, 0]], [[65535, 0, 1, 1]], [[0, 1, 1, 65535]], [[0, 0, PHP_INT_MAX, 1]]]);

it('takes a clip that ends exactly at 65535', function (): void {
    expect(spans((new RasterScanner(0, 65534, 65535, 1, false))->path([[0, 65534, 65535, 65534, 65535, 65535, 0, 65535]])))->toBe([[65534, 0, 65535, 255]]);
});

it('fills a path, taking the pixels whose centres are inside', function (): void {
    $scanner = new RasterScanner(0, 0, 5, 5, false);

    expect(grid($scanner->path([[0, 0, 4, 0, 0, 4]]), 0, 0, 5, 5))->toBe(['###..', '##...', '#....', '.....', '.....']);
});

it('unites overlaps under non-zero and cuts them under even-odd', function (): void {
    $scanner = new RasterScanner(0, 0, 6, 1, false);
    $squares = [[0, 0, 4, 0, 4, 1, 0, 1], [2, 0, 6, 0, 6, 1, 2, 1]];

    expect(grid($scanner->path($squares), 0, 0, 6, 1))->toBe(['######'])
        ->and(grid($scanner->path($squares, RASTER_EVEN_ODD), 0, 0, 6, 1))->toBe(['##..##']);
});

it('answers coverage with anti-aliasing: halves at both ends, sixteen sample lines a row', function (): void {
    $scanner = new RasterScanner(0, 0, 4, 2, true);

    expect(spans($scanner->path([[0.5, 0, 2.5, 0, 2.5, 1, 0.5, 1]])))->toBe([[0, 0, 1, 128], [0, 1, 1, 255], [0, 2, 1, 128]])
        ->and(spans((new RasterScanner(0, 0, 1, 2, true))->path([[0, 0.2, 1, 0.2, 1, 1.2, 0, 1.2]])))->toBe([[0, 0, 1, 0xCF], [1, 0, 1, 0x30]]);
});

it('fills an ellipse and the ring of a stroked one', function (): void {
    $scanner = new RasterScanner(0, 0, 8, 8, false);

    expect(grid($scanner->ellipse(4, 4, 3, 3), 0, 0, 8, 8))->toBe(['........', '..####..', '.######.', '.######.', '.######.', '.######.', '..####..', '........'])
        ->and(grid($scanner->ring(4, 4, 3, 3, 2), 0, 0, 8, 8))->toBe(['..####..', '.######.', '###..###', '##....##', '##....##', '###..###', '.######.', '..####..']);
});

it('draws one-pixel polylines between the floored points, every pixel once, in either mode', function (bool $antialias): void {
    $scanner = new RasterScanner(0, 0, 6, 5, $antialias);

    expect(grid($scanner->polyline([1, 1, 4.9, 1.2, 4, 3, 1, 3], true), 0, 0, 6, 5))->toBe(['......', '.####.', '.#..#.', '.####.', '......'])
        ->and(spans($scanner->polyline([1, 1, 4, 1, 4, 3, 1, 3], true)))->toHaveCount(4)
        ->and(spans($scanner->polyline([2, 2])))->toBe([[2, 2, 1, 255]]);
})->with([false, true]);

it('walks only the part of a far-off line inside the clip', function (): void {
    $spans = spans((new RasterScanner(0, 0, 4, 4, false))->polyline([-1e9, -1e9, 1e9, 1e9]));

    expect($spans)->toBe([[0, 0, 1, 255], [1, 1, 1, 255], [2, 2, 1, 255], [3, 3, 1, 255]]);
});

it('answers nothing for shapes with no inside or outside the clip', function (): void {
    $scanner = new RasterScanner(0, 0, 4, 4, true);

    expect($scanner->path([]))->toBe('')
        ->and($scanner->path([[0, 0, 4, 4]]))->toBe('')
        ->and($scanner->path([[10, 10, 20, 10, 20, 20]]))->toBe('')
        ->and($scanner->ellipse(2, 2, 0, 1))->toBe('')
        ->and($scanner->ring(2, 2, 1, 1, 0))->toBe('')
        ->and($scanner->polyline([]))->toBe('')
        ->and($scanner->polyline([10, 10, 20, 20]))->toBe('');
});

it('refuses malformed contours and points, naming them', function (Closure $call, string $message): void {
    expect($call)->toThrow(ValueError::class, $message);
})->with([
    'an odd count' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, 4, 0, 4]]), 'Contour 0 is not a flat list of x, y pairs.'],
    'not a list' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, 4, 0, 4, 4], 'x']), 'Contour 1 is not a flat list of x, y pairs.'],
    'keyed points' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([['a' => 0, 'b' => 0]]), 'Contour 0 is not a flat list of x, y pairs.'],
    'a string' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, 4, '0', 4, 4]]), 'Contour 0 holds something that is not a number.'],
    'keyed contours' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path(['a' => [0, 0, 4, 0, 4, 4]]), 'must be a list of contours'],
    'a rule' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([], 2), 'must be RASTER_NON_ZERO or RASTER_EVEN_ODD'],
    'NAN' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, NAN, 0, 4, 4]]), 'A coordinate is not a finite number.'],
    'past the limit' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->polyline([0, 0, 2 ** 30 + 1, 0]), 'A coordinate 1073741825 is past ±1073741824.'],
    'an odd point list' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->polyline([0, 0, 1]), 'Points is not a flat list of x, y pairs.'],
    'an infinite radius' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->ellipse(2, 2, INF, 1), 'rx is not a finite number.'],
    'a stroke past the limit' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->ring(2, 2, 1, 1, -2e9), 'stroke -2000000000 is past ±1073741824.'],
]);

it('takes the limit itself', function (): void {
    expect((new RasterScanner(0, 0, 4, 4, false))->polyline([-RASTER_LIMIT, -RASTER_LIMIT, RASTER_LIMIT, RASTER_LIMIT]))->not->toBe('');
});
