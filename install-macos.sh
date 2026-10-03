#!/bin/bash

# Builds ext-rasterize in a disposable copy and installs it into each PHP given.
#
#   ./install-macos.sh                      # Homebrew php@8.4 (NTS) and php@8.4-zts
#   ./install-macos.sh /path/to/bin/php ... # specific PHP binaries
#
# For each PHP: phpize/configure/make against that PHP's php-config, copy the
# .so into its extension_dir, re-sign it ad hoc so amfid accepts it on first
# load, and enable it with 30-rasterize.ini in its conf.d scan dir.

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCES=(config.m4 php_rasterize.h src stubs)
INI_NAME="30-rasterize.ini"

die() { printf '✖ %s\n' "$*" >&2; exit 1; }
warn() { printf '⚠ %s\n' "$*" >&2; }
step() { printf '▶ %s\n' "$*"; }

[[ "$(uname -s)" == "Darwin" ]] || die "This installer targets macOS; on Debian or Raspberry Pi OS use install-debian-trixie.sh."

if [[ $# -gt 0 ]]; then
    PHP_BINS=("$@")
else
    PHP_BINS=(/opt/homebrew/opt/php@8.4/bin/php /opt/homebrew/opt/php@8.4-zts/bin/php)
fi

BUILD_DIR=""
cleanup() { if [[ -n "$BUILD_DIR" ]]; then rm -rf "$BUILD_DIR"; fi; }
trap cleanup EXIT

for PHP_BIN in "${PHP_BINS[@]}"; do
    BIN_DIR="$(dirname "$PHP_BIN")"
    PHPIZE="${BIN_DIR}/phpize"
    PHP_CONFIG="${BIN_DIR}/php-config"
    for tool in "$PHP_BIN" "$PHPIZE" "$PHP_CONFIG"; do
        [[ -x "$tool" ]] || die "$tool not found or not executable"
    done

    step "Building for $("$PHP_BIN" -r 'echo PHP_VERSION, PHP_ZTS ? " ZTS" : " NTS";') (${PHP_BIN})"
    BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rasterize-build.XXXXXX")"
    for f in "${SOURCES[@]}"; do
        cp -R "${SCRIPT_DIR}/${f}" "${BUILD_DIR}/"
    done
    if ! (cd "$BUILD_DIR" \
        && "$PHPIZE" \
        && ./configure --enable-rasterize --with-php-config="$PHP_CONFIG" \
        && make -j"$(sysctl -n hw.ncpu)") >"${BUILD_DIR}/build.log" 2>&1; then
        tail -40 "${BUILD_DIR}/build.log" >&2
        die "Build failed for ${PHP_BIN}"
    fi

    EXT_DIR="$("$PHP_BIN" -r 'echo ini_get("extension_dir");')"
    SCAN_DIR="$("$PHP_BIN" -r 'echo PHP_CONFIG_FILE_SCAN_DIR;')"
    [[ -d "$EXT_DIR" ]] || die "extension_dir ${EXT_DIR} does not exist"
    [[ -n "$SCAN_DIR" && -d "$SCAN_DIR" ]] || die "conf.d scan dir '${SCAN_DIR}' does not exist"

    install -m 0755 "${BUILD_DIR}/modules/rasterize.so" "${EXT_DIR}/rasterize.so"
    codesign --force --sign - "${EXT_DIR}/rasterize.so" >/dev/null 2>&1
    printf 'extension=rasterize\n' > "${SCAN_DIR}/${INI_NAME}"

    "$PHP_BIN" -r 'exit(extension_loaded("rasterize") ? 0 : 1);' || die "rasterize did not load in ${PHP_BIN}"
    step "Installed ${EXT_DIR}/rasterize.so, enabled by ${SCAN_DIR}/${INI_NAME}"

    rm -rf "$BUILD_DIR"
    BUILD_DIR=""
done
