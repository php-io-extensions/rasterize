# Security Policy

## Supported versions

ext-rasterize is pre-1.0. No 0.x release receives security fixes or advisories; fixes land in
the next release line. Security support starts with 1.0.

| Version | Security fixes |
|---------|----------------|
| < 1.0   | No             |

## Reporting a vulnerability

Please don't open a public issue for a security problem.

Report it privately through GitHub: the **Report a vulnerability** button on the
[php-io-extensions/rasterize](https://github.com/php-io-extensions/rasterize) repository's
**Security** tab. If that isn't available, email **info@projectsaturnstudios.com**.

Include what you found, the affected version, your platform and PHP build (NTS or ZTS), the
scanner's clip and edge mode and the shape that triggers it, and steps to reproduce. Reports are
read and weighed for the release line in development; before 1.0 there is no response-time
commitment.

## Security model

ext-rasterize is geometry in plain C: it opens no files, makes no network requests, links no
library and holds no state between calls beyond each scanner's clip and edge mode. It reads the
numbers it is given and writes only the strings it returns.

- **Input.** Every coordinate is checked before scanning: it must be a finite number with
  magnitude at most `RASTER_LIMIT` (2³⁰), and every list must be a flat list of numbers in
  pairs. Anything else is a `ValueError`, so no NaN, infinity or huge value reaches the
  arithmetic.
- **Bounds.** A scanner's clip lies inside 0..65535, so every span's `y`, `x` and `length` fits
  its uint16 field. Scanning walks only the rows inside the clip, and a one-pixel line walks
  only its stretch inside it, so an endpoint far off the canvas costs nothing.
- **Memory.** The spans, crossings and per-row buffers come from PHP's allocator and grow by
  doubling, so `memory_limit` bounds the cost of a large shape. Everything a call allocates is
  freed before it returns, the answer string aside.

A report is in scope when ext-rasterize itself reads or writes memory it should not, leaks, or
crashes on any argument: a buffer sized from a count that can disagree with the data, a span
written past the clip, an allocation whose size can wrap.
