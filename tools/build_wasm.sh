#!/bin/bash
# WASM build (AX_WASM_THREADS=0), clean env (do NOT source env_toolchain.sh)
set -e
cd /workspace/PelicanBike_rename/PelicanBike
source /workspace/axmol-release-2.x/tools/external/emsdk/emsdk_env.sh >/dev/null 2>&1
export AX_ROOT=/workspace/axmol-release-2.x PATH=/workspace/axmol-release-2.x/tools/external/cmake/bin:$HOME/.local/bin:$PATH
OFFS=""; for e in 3D 3D_PHYSICS PHYSICS NAVMESH HTTP WEBSOCKET OPUS EXT_ASSETMANAGER EXT_COCOSTUDIO EXT_DRAGONBONES EXT_FAIRYGUI EXT_GUI EXT_IMGUI EXT_INSPECTOR EXT_JSONDEFAULT EXT_LUA EXT_PARTICLE3D EXT_PHYSICS_NODE EXT_SDFGEN EXT_SPINE EXT_VIDEO EXT_EFFEKSEER EXT_LIVE2D EXT_WEBM VLC_MEDIA; do OFFS="$OFFS -DAX_ENABLE_$e=OFF"; done
[ -f build_wasm/CMakeCache.txt ] || emcmake cmake -S . -B build_wasm -DCMAKE_MAKE_PROGRAM=/home/box/.tc/usr/bin/gmake -DCMAKE_BUILD_TYPE=Release -DAX_WASM_THREADS=0 -DAX_WASM_INITIAL_MEMORY=256MB -DAX_WASM_ENABLE_DEVTOOLS=OFF -DAX_WITH_VPX=OFF $OFFS
[ -f build_wasm/CMakeCache.txt ] && cmake build_wasm >/dev/null  # re-glob new sources
cmake --build build_wasm -j6
