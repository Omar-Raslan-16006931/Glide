#!/usr/bin/env bash
# Downloads MediaMTX (pinned, checksum checked), makes a throw-away TLS certificate and runs the going-live
# tests against it. Usage: run-live-stream-tests.sh <test-binary> <mediamtx-asset-suffix> [exe-name]
set -euo pipefail
binary=$1; suffix=$2; exe=${3:-mediamtx}
version=v1.21.4
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
cd "$work"
asset=mediamtx_${version}_${suffix}
curl -fsSL -o "$asset" "https://github.com/bluenviron/mediamtx/releases/download/${version}/${asset}"
curl -fsSL -o checksums.sha256 "https://github.com/bluenviron/mediamtx/releases/download/${version}/checksums.sha256"
grep -E "[ *]${asset}\$" checksums.sha256 | sed "s#\*# #" | sha256sum -c -
case "$asset" in *.zip) unzip -q "$asset" ;; *) tar -xzf "$asset" ;; esac
# MSYS_NO_PATHCONV: Git Bash on Windows would turn "/CN=..." into a file path.
MSYS_NO_PATHCONV=1 openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out server.crt -days 2 -subj "/CN=127.0.0.1" \
    -addext "subjectAltName=IP:127.0.0.1" -addext "basicConstraints=critical,CA:TRUE" 2>&1 | tail -n 3
test -s server.crt -a -s server.key
# Windows: FFmpeg's TLS there (Schannel) checks the certificate like a browser, as it should for the real
# platforms, so the throw-away certificate is trusted on the (disposable) CI machine for the test only.
if command -v certutil.exe >/dev/null 2>&1; then
    MSYS_NO_PATHCONV=1 certutil.exe -f -addstore Root "$(cygpath -w "$work/server.crt")" >/dev/null
    trap 'MSYS_NO_PATHCONV=1 certutil.exe -delstore Root 127.0.0.1 >/dev/null 2>&1 || true' EXIT
fi
cp "$here/mediamtx.yml" .
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi; }
export PUSHFPS_STREAM_SERVER="$(native "$work/$exe")" PUSHFPS_STREAM_CONFIG="$(native "$work/mediamtx.yml")" PUSHFPS_STREAM_FIXTURE="$(native "$here/../stream-test.flv")"
"$binary"
