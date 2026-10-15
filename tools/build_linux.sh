#!/bin/bash
# desktop Linux build against official axmol release/2.x ($AX_ROOT, see tools/env_toolchain.sh)
set -e; cd "$(dirname "$0")/.."
source tools/env_toolchain.sh
OFFS=""; for e in 3D 3D_PHYSICS PHYSICS NAVMESH HTTP WEBSOCKET OPUS EXT_ASSETMANAGER EXT_COCOSTUDIO EXT_DRAGONBONES EXT_FAIRYGUI EXT_GUI EXT_IMGUI EXT_INSPECTOR EXT_JSONDEFAULT EXT_LUA EXT_PARTICLE3D EXT_PHYSICS_NODE EXT_SDFGEN EXT_SPINE EXT_VIDEO EXT_EFFEKSEER EXT_LIVE2D VLC_MEDIA; do OFFS="$OFFS -DAX_ENABLE_$e=OFF"; done
[ -f build/CMakeCache.txt ] || cmake -S . -B build -DCMAKE_MAKE_PROGRAM=/home/box/.tc/usr/bin/gmake -DCMAKE_BUILD_TYPE=Release $OFFS
cmake --build build -j7
