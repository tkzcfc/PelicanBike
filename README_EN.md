# PelicanBike (Axmol C++ Port)

[中文](README.md) | English

## Introduction
This is an **Axmol 2.x C++ port / recreation** of **pelican-bike**, the Three.js r186 real-time 3D demo in [riba2534/claude-opus-5-5-demo](https://github.com/riba2534/claude-opus-5-5-demo). The original is the visual and interaction reference; the goal is a 1:1 look with high performance on desktop and WASM. A white pelican in a helmet and red scarf rides a vintage bike along the coast catching fish, through dusk, starry skies, waves and streetlights, all generated in real time.

Geometry comes from the original JS: `tools/bake.mjs` / `tools/bake_rider.mjs` run the original `bicycle.js`, `pelican.js` and `world.js` in Node and export the meshes in `Content/data/*`. Nothing was re-modelled by hand.

## Features
- Procedural coast: 7 terrain chunks (same Perlin heightfield as the original), road/rail/curb/lamps, instanced palms, trees, pines, rocks and umbrellas
- 5-wave Gerstner ocean (foam, fresnel, sun and moon glint), sky (sun, moon, stars, Milky Way, clouds), fog
- Day/night cycle: time slider, time flow and flow rate; lamp light pools, headlight, fireflies and a lighthouse beam at night
- Pelican rider: crank pedalling and trick poses, steering, head look, jaw/gulp, blinking, wing spread; helmet/glasses/scarf toggles; Verlet cloth scarf
- Gameplay: autopilot that chases fish, lane changes to catch fish, jump, trick, bell, honk, automatic or manual 5-speed gears, 14 achievements, score pops
- Effects: fish halos, fish jumping out of the sea, dust/sparkle/splash particles, confetti, speed lines, gull and pelican flocks, sailboats
- 5 cameras: free orbit, chase, side, cinematic (with letterbox bars and cut flashes), pelican POV
- HUD: frosted-glass panels (backdrop blur), speed gauge, cadence/gear/distance, fish count and achievements, time bar, hint bar, toasts, settings panel with collapsible folders, help, intro card
- Software-synthesized sound effects and generative music that follows cadence
- Quality presets low/medium/high/ultra plus adaptive resolution
- URL params: `?q=low|medium|high|ultra`, `?time=17.35`, `?cam=orbit|chase|side|cine|pov`, `?autostart=1` (the share button writes these)

## Controls
| Key | Action |
|---|---|
| W / ↑ | Accelerate |
| S / ↓ | Brake |
| A / D, ← / → | Change lane |
| Space | Jump |
| T | Trick (wings out + wheelie) |
| B | Bell |
| H | Pelican honk |
| C | Cycle camera; 1–5 pick a camera directly |
| Q / E | Manual gear down / up (auto shifting resumes afterwards) |
| N | Skip ahead 3 hours |
| M | Music on/off |
| P | Pause / resume |
| U | Hide UI |
| K | Screenshot |
| F | Fullscreen |
| F3 | Perf stats |

- **Mouse**: dragging more than 10px switches to the free orbit camera and rotates it; the wheel zooms. Clicking the bell or bike rings the bell, clicking the pelican makes it honk, and clicking the distant sea makes a fish jump out.
- **Touch** (enabled automatically on coarse-pointer devices): ◀ ▶ change lane, ▼ brakes, ▲ accelerates (several pads can be held at once), 🔔 bell, 🪽 trick, ⤒ jump. A finger that starts on a slider track can drag that slider.
- Top toolbar: screenshot, music, share link, fullscreen, settings, help, GitHub.

## Engine
Requires the official **Axmol `release/2.x` branch** (tested on 2.11.6 / de73de2):
```bash
git clone -b release/2.x https://github.com/axmolengine/axmol axmol
cd axmol && pwsh setup.ps1      # fetches cmake / axslcc / emsdk into tools/external
export AX_ROOT=$PWD             # Windows: setx AX_ROOT <path>
```

## Build
**Linux desktop** (the script paths match the author's machine; edit `tools/env_toolchain.sh` first):
```bash
bash tools/build_linux.sh       # → build/bin/PelicanBike/PelicanBike
```
The plain CMake flow also works: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`.

**Windows desktop**: use Axmol's standard CMake flow (VS 2022): `cmake -S . -B build_win -G "Visual Studio 17 2022" -A x64`, then `cmake --build build_win --config Release`. Note: there is no `proj.win32` folder in this repo, and the Windows desktop build has not been tested.

**WASM** (needs emsdk and pwsh on PATH; single-threaded, `AX_WASM_THREADS=0`):
```bash
env -i HOME=$HOME PATH=/usr/bin:/bin bash tools/build_wasm.sh   # → build_wasm/bin/PelicanBike/PelicanBike.html
python3 tools/wasm_serve.py --root build_wasm/bin/PelicanBike --port 8089
# open http://127.0.0.1:8089/PelicanBike.html
```
`wasm_serve.py` sends COOP/COEP/CORP headers, so you can't just double-click the html. `bash tools/package_zip.sh` packs the build into `PelicanBike_web.zip` (web/, wasm_serve.py, 启动.bat, 说明.txt).

## CI / Releases / Play online
- `.github/workflows/build.yml` clones the official Axmol `release/2.x` in CI and runs `setup.ps1` (engine and toolchains are cached), then calls `tools/ci/build.ps1` (i.e. `axmol -p <platform> -O3`).
- **Pushing a `v*` tag** (e.g. `git tag v1.0.0 && git push origin v1.0.0`) builds Windows x64 (MSVC), macOS arm64 (.app zipped), an Android arm64 APK and Web, and uploads `PelicanBike-windows-x64.zip`, `PelicanBike-macos.zip`, `PelicanBike-android.apk` and `PelicanBike-web.zip` to that tag's GitHub Release. You can also run it by hand from the Actions tab (workflow_dispatch); manual runs only produce artifacts.
- **Pushing to `main`** builds only Web and deploys it to GitHub Pages: **https://tkzcfc.github.io/PelicanBike/**. The web build is single-threaded (`AX_WASM_THREADS=0`), so no COOP/COEP headers are needed.
- Enable Pages first: repo Settings → Pages → Build and deployment → **Source = GitHub Actions**.
- Android signing (optional): add the repo secrets `ANDROID_KEYSTORE_BASE64` (base64 of the keystore file), `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS` and `ANDROID_KEY_PASSWORD`. Without them the release APK is signed with the debug key (installable, but not for store upload).
- The macOS build is not signed or notarized: right-click → Open the first time, or run `xattr -dr com.apple.quarantine PelicanBike.app`.
- Render backend: the custom 3D layer uses OpenGL, so `CMakeLists.txt` forces native GL 3.3 on Windows (not ANGLE), OpenGL on macOS (not Metal) and GLES 3.0 on Android.

## What Axmol provides
- Window and GL context (`RenderViewImpl`; GL 3.3 core on desktop / WebGL2), the Director main loop
- Input: `EventListenerKeyboard`, `EventListenerMouse`, `EventListenerTouchAllAtOnce`
- `FileUtils` asset loading (Content/: meshes, textures, fonts, emoji sprites)
- 2D: `Label` (TTF), `DrawNode` and `Sprite` for the HUD, intro card and settings panel
- Audio: OpenAL (Axmol's 3rdparty); the synth streams PCM into it
- Render hook: the whole 3D frame runs inside one `CallbackCommand` (globalZ −9000), which ends with `backend::OpenGLState::reset()` to keep Axmol's state cache in sync
- The Emscripten WASM target (Axmol's wasm platform layer)

## Written from scratch
- **Custom GL render layer** `Source/gfx`: Shader, Mesh (VAO/VBO/IBO, packed 24-byte vertex with per-vertex material), instance buffers, textures, RenderTarget (with MSAA), stats
- **std140 "Frame" UBO** shared by every program: camera, sun/moon, palette, fog, wave phases, ambient cube
- **Shaders** (`Source/game/Shaders.h`): Gerstner ocean, sky, terrain, road (bump mapped), PBR lighting (GGX + Lambert, lamp/head point and spot lights, clearcoat beak)
- **Shadows**: 2048² depth map, texel-snapped, 5-tap PCF
- **HDR + 4× MSAA** (RGBA16F), **UnrealBloomPass port** (5-mip separable gaussian), colour grade (vignette/warm/grain), **ACES Filmic + sRGB**, NaN/Inf sanitizing
- **Environment lighting**: a CPU-integrated analytic sky "ambient cube" plus sky reflection, refreshed when the hour moves more than 0.18h (matching JS PMREM refresh), with ambient tint that depends on normal direction
- **Rider**: uniform bone palette of 48 rigid bones baked from the original `pelican.js` / `bicycle.js` (16 crank × 2 trick poses blended on the CPU), plus runtime steering, wheels, head look, jaw and blinks; the neck is a CPU-built tube
- **Particles and effects**: point-sprite particles (dust/sparkle/splash), confetti, instanced fish, Verlet cloth scarf, instanced flocks, lighthouse/sailboats/fireflies
- **Audio**: software-synth port of `audio.js` (oscillators/noise, envelopes, RBJ biquads, wind/tyre/surf beds, Schroeder reverb, compressor, generative music)
- **UI**: HUD, intro card, settings panel, help, toasts, backdrop-blur glass panels, touch pads, narrow-screen layout
- **Quality**: 4 presets (render scale, shadow size, MSAA, bloom, prop density) plus adaptive resolution ported from `main.js` adaptQuality

## Performance
- Real GPU (WASM in Chrome on Windows): **locked 60 FPS**, render scale 1.00, update 0.3 ms, **65 draw calls**, about 557k triangles
- Old port (one Axmol node per object) on the same software-rendered box: **708 draw calls, 3.0 FPS**; this port does 56–62 draw calls there
- CPU update 0.05–0.08 ms (desktop)

## Project layout
```
CMakeLists.txt  cmake/          Axmol project config
proj.linux/ proj.wasm/          platform main.cpp
Source/AppDelegate.*            startup and window
Source/MainScene.*              input glue and test hooks
Source/Hud.* Intro.*            HUD / settings panel / intro card
Source/Platform.*               fullscreen, share link, open URL, screenshot, URL params
Source/gfx/                     custom GL layer
Source/game/                    Shaders.h, Game (sim + frame graph), Rider, Fx, Extras, Common (JS util/noise/palette ports)
Source/audio/                   software synth
Content/data tex fonts emoji    baked meshes, textures, font subsets, emoji sprites
tools/                          bake, build, package, server, capture/audit scripts, font subsetting
ARCHITECTURE.md PROGRESS.md     architecture notes and dev log
```

## Font subsetting
`bash tools/font_subset.sh` scans every character used in `Source/` (plus ASCII and UI symbols) and builds the subset fonts in `Content/fonts` from `tools/NotoSansSC-full.ttf`. Run it again after changing UI text.

## License & credits
- Original work and visual reference: [riba2534/claude-opus-5-5-demo](https://github.com/riba2534/claude-opus-5-5-demo) (pelican-bike, Three.js). The original shaders, geometry and gameplay belong to its author; follow that repository's license.
- Engine: [Axmol](https://github.com/axmolengine/axmol) (MIT).
- Fonts: Noto Sans SC / Noto Sans (SIL OFL); emoji sprites rendered from Noto Color Emoji (OFL).
- This port's repository: https://github.com/tkzcfc/PelicanBike
