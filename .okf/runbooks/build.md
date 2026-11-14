---
type: Runbook
title: Build, install, test
description: Scratch build with gen_stub, the two installers, Pest, the Pi loop.
resource: install-macos.sh
tags: [build, linux, macos, pest]
status: draft
generated: { by: claude-opus/5.5, at: 2026-11-05T00:00:00Z }
sources:
  - id: mac
    resource: install-macos.sh
    title: install-macos.sh
  - id: debian
    resource: install-debian-trixie.sh
    title: install-debian-trixie.sh
---

# Overview

No system library needed: php-dev and a C compiler.

Dev loop: copy `config.m4 php_rasterize.h src stubs` to a scratch dir, `phpize`, `php build/gen_stub.php stubs`, `./configure --enable-rasterize --with-php-config=…`, `make`. Copy `stubs/*_arginfo.h` back after a stub edit. Run Pest with `-d extension=<scratch>/modules/rasterize.so`. Build with `-Wall -Wextra`; no warning from `src/` is acceptable.

macOS (`install-macos.sh`): builds in a temp copy for php@8.4 and php@8.4-zts, ad-hoc signs, writes `30-rasterize.ini`.[^mac]

Linux (`install-debian-trixie.sh`): builds in place, installs `rasterize.so`, writes `30-rasterize.ini`, removes build output.[^debian]

Pi from the Mac: tree on the Mac is authoritative, Pi copy disposable. `COPYFILE_DISABLE=1 tar --no-mac-metadata --exclude .git -czf - -C <ext> . | fnk 'tar -xzf - -C ~/rz'`, install, Pest, `rm -rf ~/rz`. Without `--no-mac-metadata` the copy carries `._*` files that gen_stub trips on.

Gaming PC from the Mac (through the `gamingpc` alias): a stdin pipe into the alias does not work from a script, so build a tar file and redirect it in. Take the files from `git ls-files`: `git archive` drops `tests/` through `export-ignore`. Set `COPYFILE_DISABLE=1` and pass `--no-xattrs`, or the `._*` files break gen_stub. `COPYFILE_DISABLE=1 git ls-files -z | xargs -0 tar --no-xattrs -czf /tmp/rz.tgz`, then `gamingpc '…' < /tmp/rz.tgz` with the Pi loop's unpack, install, Pest and clean-up on the far side.

`config.m4` passes `-ffp-contract=off -fno-builtin-sin -fno-builtin-cos`. Without the first the compiler may fuse multiply-adds and answer different bytes from a PHP implementation of the same arithmetic; without the other two it joins the sine and cosine of one angle into a single `sincos` call (`__sincos_stret` on macOS) whose last bit is not always the one `sin()` and `cos()` give alone, and the stroker's arcs differ. Check with `objdump -d src/.libs/core.o src/.libs/stroke.o | grep -c fmadd`: 0 on arm64, and `nm -u src/.libs/stroke.o | grep -i sincos`: nothing.

To test a consumer without the ext, run with a `PHP_INI_SCAN_DIR` that omits `30-rasterize.ini`; `php -n` drops every other conf.d extension too.

[^mac]: Same script as ext-fb's, names changed.
[^debian]: Same script as ext-fb's, names changed.
