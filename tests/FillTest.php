<?php

declare(strict_types=1);

/*
 * fill() paints batch()'s ops into an ext-fb FbBuffer through ext-fb's span sink, as FbBuffer::paintSpans() paints
 * batch()'s answers, with the same bounds. Without ext-fb, ext-rasterize still loads: fills() is false, fill() an Error.
 */

/** @return list<list<mixed>> overlapping ops, one outside the clip */
function fillOps(): array
{
    return [
        [RASTER_OP_PATH, [[1.5, 2, 30.2, 4.1, 9, 25.7], [5, 5, 12, 5, 12, 12]], RASTER_EVEN_ODD],
        [RASTER_OP_ELLIPSE, 20.25, 15, 9.5, 6],
        [RASTER_OP_RING, 20, 15.5, 9, 7, 2.5],
        [RASTER_OP_ROUND_RECT, 2.4, 3, 33.1, 26, [4, 0, 6.5, 2]],
        [RASTER_OP_ROUND_RING, 2.4, 3, 33.1, 26, [4, 0, 6.5, 2], 1.75],
        [RASTER_OP_SHADOW, 6, 7, 30, 22, 3, 2.5],
        [RASTER_OP_POLYLINE, [1, 1, 38, 4, 20, 28], true],
        [RASTER_OP_ELLIPSE, 500, 500, 3, 3],
    ];
}

/** A buffer of noise, the same for a seed. */
function fillBuffer(int $seed): FbBuffer
{
    $buffer = new FbBuffer(new FbFormat(FB_LAYOUT_RGBA8888, channelOrder: FB_CHANNELS_RGBA), 44, 32);
    mt_srand($seed);
    for ($y = 0; $y < 32; $y++) {
        for ($x = 0; $x < 44; $x++) {
            $buffer->set($x, $y, mt_rand(0, 0x7FFFFFFF) * 2 + mt_rand(0, 1));
        }
    }

    return $buffer;
}

it('paints into an FbBuffer what paintSpans() paints of the batch, with the same bounds', function (bool $antialias): void {
    $scanner = new RasterScanner(1, 2, 40, 28, $antialias);
    $colours = [0x3366CCB0, 0xFFFFFF60, 0x0000FFFF, 0x80808040, 0x10EE10D0, 0x000000A0, 0xFF0000FF, 0xFFFFFFFF];
    [$direct, $spanned] = [fillBuffer(3), fillBuffer(3)];

    $bounds = $scanner->fill(fillOps(), $colours, $direct);
    $expected = [];
    foreach ($scanner->batch(fillOps()) as $i => $spans) {
        $expected[] = $spans === '' ? null : $spanned->paintSpans($spans, $colours[$i]);
    }

    expect(RasterScanner::fills())->toBeTrue()
        ->and($bounds)->toBe($expected)
        ->and(bin2hex($direct->bytes()))->toBe(bin2hex($spanned->bytes()))
        ->and($scanner->fill([], [], $direct))->toBe([]);
})->with([true, false])->skip(! class_exists(FbBuffer::class), 'ext-fb is not loaded in this PHP.');

it('refuses a target, clip, colour or op before painting any', function (Closure $call, string $error, string $message): void {
    $buffer = fillBuffer(4);
    $before = $buffer->bytes();

    expect(fn () => $call($buffer))->toThrow($error, $message)
        ->and($buffer->bytes() === $before)->toBeTrue();
})->with([
    'not an FbBuffer' => [fn ($b) => (new RasterScanner(0, 0, 8, 8, true))->fill([], [], new stdClass), TypeError::class, 'must be a constructed FbBuffer, stdClass given'],
    'a clip past the buffer' => [fn ($b) => (new RasterScanner(0, 0, 45, 8, true))->fill([], [], $b), ValueError::class, 'The clip reaches past the 44x32 framebuffer'],
    'fewer colours than ops' => [fn ($b) => (new RasterScanner(0, 0, 8, 8, true))->fill([[RASTER_OP_ELLIPSE, 4, 4, 2, 2]], [], $b), ValueError::class, 'a list of ops and a list of as many colours'],
    'a colour past 32 bits' => [fn ($b) => (new RasterScanner(0, 0, 8, 8, true))->fill([[RASTER_OP_ELLIPSE, 4, 4, 2, 2]], [1 << 32], $b), ValueError::class, 'Colour 0 is not 0xRRGGBBAA'],
    'a bad op after a good one' => [fn ($b) => (new RasterScanner(0, 0, 8, 8, true))->fill([[RASTER_OP_ELLIPSE, 4, 4, 2, 2], [RASTER_OP_RING, 4, 4, 2]], [0xFF0000FF, 0xFF0000FF], $b), ValueError::class, 'Op 1 takes 6 entries'],
])->skip(! class_exists(FbBuffer::class), 'ext-fb is not loaded in this PHP.');

it('loads without ext-fb: fills() is false and fill() an Error', function (): void {
    expect(RasterScanner::fills())->toBeFalse()
        ->and(fn () => (new RasterScanner(0, 0, 8, 8, true))->fill([], [], new stdClass))->toThrow(Error::class, 'ext-fb is not loaded');
})->skip(class_exists(FbBuffer::class), 'ext-fb is loaded in this PHP.');
