<div align="center" id="header">

<h1>ext-rasterize</h1>

<h3>Shapes into coverage spans in C, for PHP 8.4+</h3>

[![CI](https://img.shields.io/github/actions/workflow/status/php-io-extensions/rasterize/ci.yml?style=flat-square&label=CI)](https://github.com/php-io-extensions/rasterize/actions/workflows/ci.yml)
[![Packagist](https://img.shields.io/packagist/v/php-io-extensions/rasterize?style=flat-square)](https://packagist.org/packages/php-io-extensions/rasterize)
[![PHP](https://img.shields.io/packagist/dependency-v/php-io-extensions/rasterize/php?style=flat-square&label=PHP)](https://packagist.org/packages/php-io-extensions/rasterize)
[![License](https://img.shields.io/github/license/php-io-extensions/rasterize?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-linux_%7C_macos-blue?style=flat-square&logo=linux&logoColor=white)](#requirements)
[![OKF](https://img.shields.io/badge/OKF-0.2-6f42c1?style=flat-square&label=knowledge%20bundle)](.okf/index.md)
[![PIE](https://img.shields.io/badge/PIE-installable-4f5b93?style=flat-square&logo=php&logoColor=white)](https://github.com/php/pie)

</div>

To draw a filled shape you first have to know which pixels it covers, and how much of each. A circle, a star, a rounded label or a gauge's arc each come down to the same questions asked row by row: where does the outline cross this row? Which pixels lie inside? and how much of each pixel on the edge does the shape cover? Answer it with a yes or no per-pixel and the edges stair-step; answer it with a fraction, and they come out smooth.

In PHP that question is a scanline loop. Every row walks every edge to find its crossings, sorts them, applies a fill rule, and for smooth edges does it all again at several sample heights per row. A disc a couple of hundred pixels across is tens of thousands of float operations through the interpreter, and a self-intersecting path has to be wound correctly, or else it fills its holes.

<b>The only problem?</b> PHP has no rasteriser that stops at geometry. gd and Imagick draw straight into their own images and never tell you what they covered. 

<b>ext-rasterize</b> answers resolves that scanline loop in C. It has no dependency, no pixel store and no opinions: same input, same bytes, on every platform, because the arithmetic is IEEE double in a fixed order with fused multiply-add turned off. With the performance you expect from C, bound and exposed to PHP.




## Basic usage

Most ways you will interface with ext-rasterize are in two parts -

Make a scanner with the clip you draw into and the edge mode, then ask it for shapes. Coordinates are pixel edges: pixel `(x, y)` covers `[x, x + 1) × [y, y + 1)`.

```php
// Scan inside a 320 × 240 clip, anti-aliased.
$scanner = new RasterScanner(0, 0, 320, 240, true);

$disc = $scanner->ellipse(160, 120, 80, 50);
```

Read the spans. Each one is 7 bytes, little-endian: `y`, `x` and `length` as uint16, `coverage` as uint8, rows ascending and `x` ascending within a row. Unpack them, or hand the whole string to a pixel store that paints them.

```php
echo strlen($disc) / RASTER_SPAN_BYTES, " spans\n";

foreach (array_slice(str_split($disc, RASTER_SPAN_BYTES), 0, 3) as $span) {
    ['y' => $y, 'x' => $x, 'length' => $length, 'coverage' => $coverage] = unpack('vy/vx/vlength/Ccoverage', $span);
    echo "row $y: $length px from x = $x at coverage $coverage\n";
}
```

```console
$ php disc.php
568 spans
row 70: 1 px from x = 144 at coverage 13
row 70: 1 px from x = 145 at coverage 43
row 70: 1 px from x = 146 at coverage 72
```

Row 70 is the top of the disc, where the outline only grazes each pixel; further down the rows fill out into long runs at coverage 255.

### Examples

#### Pick a fill rule

A path is a list of contours, each a flat list of `x, y` pairs closed implicitly. Under `RASTER_NON_ZERO` a hole has to wind the other way to stay empty; under `RASTER_EVEN_ODD` any contour inside another cuts it out.

```php
$hard = new RasterScanner(0, 0, 16, 16, false);

// A 12 × 12 square with a 6 × 6 square inside it, both wound the same way.
$frame = [[2, 2, 14, 2, 14, 14, 2, 14], [5, 5, 11, 5, 11, 11, 5, 11]];
$pixels = fn (string $spans): int => array_sum(array_map(fn ($s) => unpack('x4/vlength', $s)['length'], str_split($spans, RASTER_SPAN_BYTES)));

echo "non-zero: ", $pixels($hard->path($frame, RASTER_NON_ZERO)), " px, even-odd: ", $pixels($hard->path($frame, RASTER_EVEN_ODD)), " px\n";
```

```console
non-zero: 144 px, even-odd: 108 px
```

#### Hard or anti-aliased edges

A hard scanner samples each row once, at the pixel centres, and answers coverage 255 only. An anti-aliased one samples each row on `RASTER_SAMPLES` lines and adds the exact horizontal overlap of every crossing, so the edge pixels get their own coverage and split into more spans.

```php
$hard = new RasterScanner(0, 0, 16, 16, false);
$soft = new RasterScanner(0, 0, 16, 16, true);

echo "ring spans, hard: ", strlen($hard->ring(8, 8, 5, 5, 2)) / RASTER_SPAN_BYTES, ", anti-aliased: ", strlen($soft->ring(8, 8, 5, 5, 2)) / RASTER_SPAN_BYTES, "\n";
```

```console
ring spans, hard: 20, anti-aliased: 86
```

#### Draw one-pixel lines

`polyline()` draws one-pixel lines between the floored points, every pixel once, at coverage 255 in either edge mode. Only the stretch inside the clip is walked, so an endpoint far off the canvas costs nothing.

```php
$hard = new RasterScanner(0, 0, 16, 16, false);
$pixels = fn (string $spans): int => array_sum(array_map(fn ($s) => unpack('x4/vlength', $s)['length'], str_split($spans, RASTER_SPAN_BYTES)));

$line = $hard->polyline([0, 0, 15, 5]);
echo "line: ", strlen($line) / RASTER_SPAN_BYTES, " spans, ", $pixels($line), " px\n";

echo "far off: ", strlen($hard->polyline([-1e9, 3, 1e9, 3])) / RASTER_SPAN_BYTES, " span\n";
```

```console
line: 6 spans, 16 px
far off: 1 span
```

#### Paint the spans

The span bytes are exactly what [ext-fb](https://github.com/php-io-extensions/fb)'s `FbBuffer::paintSpans()` takes, in any of its pixel layouts:

```php
$buffer = new FbBuffer(new FbFormat(FB_LAYOUT_RGB565), 320, 240);
$buffer->paintSpans($disc, 0xFF8000FF);
```

## Requirements

| | |
|---|---|
| OS | Linux or macOS. `composer.json` lists those two families. |
| PHP | 8.4 or newer, NTS or ZTS. |
| Libraries | None. The extension is plain C on the Zend API. |

To build it you need a C compiler and the development files of the PHP you intend to load it into:

```bash
# Debian, Ubuntu, Raspberry Pi OS
sudo apt install build-essential php8.4-dev

# macOS: Xcode Command Line Tools, and Homebrew's PHP ships phpize and php-config
xcode-select --install
brew install php@8.4
```

## Installation

### With PHP PIE (recommended)

[PIE](https://github.com/php/pie) is the PHP Foundation's installer for extensions. It downloads the tagged source from Packagist, builds it against the PHP on your `PATH`, installs the module and enables it. `sudo` where your PHP's extension directory is root-owned, as on Debian; not on a Homebrew Mac:

```bash
pie install php-io-extensions/rasterize
```

### Manually (install script)

The repository ships two installers, one per platform. Each one builds for each PHP it targets, installs `rasterize.so` into that PHP's `extension_dir`, writes `30-rasterize.ini` into its `conf.d` scan directory, and checks the module loads. The macOS one builds in a temporary copy of the checkout and ad-hoc signs the module so the system loads it on first use; the Debian one builds in the checkout, writes the ini into the CLI, FPM and Apache `conf.d` of that PHP, and cleans the build products out afterwards.

```bash
git clone https://github.com/php-io-extensions/rasterize.git
cd rasterize

./install-debian-trixie.sh             # Debian Trixie, Ubuntu 24.04+, Raspberry Pi OS: the php on PATH
./install-macos.sh                     # Homebrew php@8.4 and php@8.4-zts
./install-macos.sh /path/to/bin/php    # the macOS script takes specific PHP binaries
```

### Manually (phpize)

```bash
git clone https://github.com/php-io-extensions/rasterize.git
cd rasterize
phpize
./configure --enable-rasterize
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
sudo make install
```

Then enable it: add `extension=rasterize.so` to your `php.ini`, or drop that line into a file in the scan directory that `php --ini` reports.

### Verify

```console
$ php --ri rasterize

rasterize

rasterize support => enabled
Version => 0.11.x
```

## API

One final class and 5 constants, declared in [`stubs/`](stubs). Every shape method answers span bytes, or an empty string when the shape covers nothing inside the clip. A coordinate that is not finite or lies past `±RASTER_LIMIT`, a list that is not a flat list of numbers in pairs, or an unknown fill rule throws `ValueError`. The class cannot be cloned or serialized, and calling `__construct()` a second time throws `Error`.

### Constants

| Constant | Value | Meaning |
|---|---|---|
| `RASTER_NON_ZERO` | `0` | Fill rule: a point is inside when the contours wind around it a non-zero number of times. |
| `RASTER_EVEN_ODD` | `1` | Fill rule: a point is inside when a ray from it crosses the contours an odd number of times. |
| `RASTER_SPAN_BYTES` | `7` | The bytes in one span. |
| `RASTER_SAMPLES` | `16` | The sample lines per row an anti-aliased scanner uses. |
| `RASTER_LIMIT` | `1073741824` | 2³⁰: the largest coordinate magnitude accepted. |

### Spans

Every answer is a string of 7-byte spans, little-endian: `y` (uint16), `x` (uint16), `length` (uint16), `coverage` (uint8). Rows ascend, `x` ascends within a row, no two spans of one answer overlap, and coverage runs 1..255. Unpack one with `unpack('vy/vx/vlength/Ccoverage', $span)`.

### Edges

* **Hard**: each row is sampled once at its pixel centres, `y + 0.5`. A pixel is in when its centre is inside; a centre exactly on a left or top edge is in, on a right or bottom edge out. Coverage is always 255.
* **Anti-aliased**: each row is sampled on 16 lines at `y + (k + 0.5) / 16`. Each crossing adds its exact horizontal overlap with every pixel it spans, weighted 1/16, and the sum becomes coverage `floor(min(sum, 1) × 255 + 0.5)`. Neighbouring pixels of equal coverage join one span.

### RasterScanner

#### `__construct(int $x, int $y, int $width, int $height, bool $antialias)`

A scanner that answers only inside the clip rect `($x, $y, $width, $height)`, which must lie in 0..65535 with both sides at least 1. `$antialias` picks the edge mode.

---

#### `clip(): array`, `antialias(): bool`

What the scanner was made with: `[$x, $y, $width, $height]` and the edge mode.

---

#### `path(array $contours, int $rule = RASTER_NON_ZERO): string`

Fills one or more contours. `$contours` is a list of contours, each a flat list `x, y, x, y…` of numbers, closed implicitly; self-intersecting contours are allowed. `$rule` is `RASTER_NON_ZERO` or `RASTER_EVEN_ODD`.

**Example — a five-pointed star**

```php
$star = $scanner->path([[160, 20, 190, 110, 280, 110, 205, 160, 235, 230, 160, 185, 85, 230, 115, 160, 40, 110, 130, 110]]);
```

---

#### `ellipse(float $cx, float $cy, float $rx, float $ry): string`

Fills an axis-aligned ellipse centred on `($cx, $cy)`. A radius that is not positive covers nothing.

---

#### `ring(float $cx, float $cy, float $rx, float $ry, float $stroke): string`

Fills the band between the ellipses of radii `r + $stroke / 2` and `r − $stroke / 2`. When the inner radius is not positive, the band is the whole outer ellipse.

**Example — a gauge's outline**

```php
$outline = $scanner->ring(160, 120, 80, 80, 6);
```

---

#### `polyline(array $points, bool $closed = false): string`

Draws one-pixel lines between consecutive points of the flat list `x, y, x, y…`, each point floored to its pixel, every pixel once, coverage 255 in either edge mode. `$closed` adds the line from the last point back to the first.

## Extensions you might use with this one

* [ext-fb](https://github.com/php-io-extensions/fb): the pixel store the spans paint into. `FbBuffer::paintSpans()` takes these bytes as they are, blending by coverage on RGB and grey layouts and thresholding on mono, palette and planar ones.

## AI-assisted development

This codebase was built with LLM assistance under human direction. Prompts, reviews, and architectural decisions were owned by a professional software engineer — the model accelerates drafting and exploration; it does not replace judgment on APIs, dependency direction, or what ships.

## Agentic Documentation

This repository carries an [Open Knowledge Format](https://github.com/GoogleCloudPlatform/knowledge-catalog/blob/main/okf/SPEC.md) bundle at [`.okf/`](.okf/): Markdown with YAML frontmatter, readable by you and loadable by a coding agent. It records what the README doesn't: how the module is layered, the exact arithmetic behind each shape and each edge mode, what every method refuses, why the build turns off fused multiply-add and how to check it, the scratch build and both installers, and a log of every change that altered behaviour.

Start at [`.okf/index.md`](.okf/index.md). [`AGENTS.md`](AGENTS.md) is the short rule sheet an agent reads before touching the C, the stubs, or packaging; each rule points into the bundle for the reason behind it.

Point your agent at the repo and it has the same context the maintainer has. Point yourself at it when the README's answer is "it does X" and you want "because".

## Resources

* [`src/core.h`](src/core.h): the span bytes and the scanning core's memory hooks, documented next to the code.
* [PIE](https://github.com/php/pie): the PHP Foundation's extension installer.
* [`gen_stub.php`](https://github.com/php/php-src/blob/master/build/gen_stub.php): the php-src stub format. [`stubs/`](stubs) holds the API declarations; each `*_arginfo.h` is generated from its stub.
* [Open Knowledge Format](https://github.com/GoogleCloudPlatform/knowledge-catalog/blob/main/okf/SPEC.md): the spec the [`.okf/`](.okf/) bundle follows.

## Running the test suite

The suite is Pest v4 and needs no hardware or image files: the span format and its ordering, both edge modes and both fill rules, ellipses and rings, one-pixel lines including far-off ones, clips up to the 65535 edge, empty answers, and every refusal with its message. It runs on Linux and macOS. Build the module in the clone, then point `php` at it:

```bash
git clone https://github.com/php-io-extensions/rasterize.git
cd rasterize
phpize && ./configure --enable-rasterize && make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
composer install
php -n -d extension="$PWD/modules/rasterize.so" vendor/bin/pest
```

```console
Tests:    34 passed (105 assertions)
Duration: 0.10s
```

`-n` skips your `php.ini`, so the suite tests the clone and not an installed `rasterize.so`; without it, a PHP that already loads rasterize warns `Module "rasterize" is already loaded` and keeps the installed one. A module only loads in the PHP whose `phpize` built it: for Homebrew's ZTS build, run `phpize` and pass `--with-php-config` from `/opt/homebrew/opt/php@8.4-zts/bin`.

## Contributing

Open an issue first for anything beyond a typo. Pull requests change a stub in `stubs/` and the C under `src/` together, regenerate the `*_arginfo.h` files with gen_stub, and add a Pest test for the behaviour. A change to the arithmetic changes the bytes every caller gets, so it needs its reason in the pull request and the expected coverage worked by hand in the test. CI must be green on every cell of the matrix. Read [`AGENTS.md`](AGENTS.md) for the rules the code follows.

## Security

ext-rasterize reads only the numbers it is given and writes only the strings it returns. See [SECURITY.md](SECURITY.md) for what it guards against and how to report a vulnerability.

## License

MIT. See [LICENSE](LICENSE).
