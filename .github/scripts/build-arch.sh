#!/usr/bin/env bash
set -euo pipefail

mkdir -p /home/builder/package /output
cp /workspace/packaging/PKGBUILD /workspace/dist/source.tar.gz /home/builder/package/
chown -R builder:builder /home/builder/package
chown -R builder:builder /ccache

runuser -u builder -- env GAOMON_PKGVER="$GAOMON_PKGVER" GAOMON_BUILD_TYPE="$GAOMON_BUILD_TYPE" \
    CMAKE_CXX_COMPILER_LAUNCHER=ccache CCACHE_DIR=/ccache CCACHE_MAXSIZE=500M \
    bash -c 'cd /home/builder/package && makepkg --noconfirm'

cp /home/builder/package/*.pkg.tar.zst /output/
mkdir -p /tmp/binary
bsdtar -xf /output/*.pkg.tar.zst -C /tmp/binary usr
tar -C /tmp/binary -czf "/output/gaomon-manage-panel-${GAOMON_PKGVER}-linux-x86_64.tar.gz" usr
chown -R "$OUTPUT_UID:$OUTPUT_GID" /output
chown -R "$OUTPUT_UID:$OUTPUT_GID" /ccache
