# PelicanBike 鹈鹕骑单车（Axmol C++ 移植版）

🎮 **在线试玩：** [https://tkzcfc.github.io/PelicanBike/](https://tkzcfc.github.io/PelicanBike/)

中文 | [English](README_EN.md)

## 简介
本项目是 [riba2534/claude-opus-5-5-demo](https://github.com/riba2534/claude-opus-5-5-demo) 中 **pelican-bike**（Three.js r186 实时 3D 演示）的 **Axmol 2.x C++ 移植 / 复刻**，以原版为视觉与交互参考，目标是 1:1 观感并在桌面与 WASM 上保持高性能。一只戴头盔、围红围巾的白鹈鹕骑着复古单车沿海岸线追鱼：黄昏、星空、海浪与路灯，全部实时生成。

几何来自原版 JS：`tools/bake.mjs` / `tools/bake_rider.mjs` 在 Node 中直接运行原版 `bicycle.js`、`pelican.js`、`world.js`，导出 `Content/data/*` 网格，没有手工重新建模。

## 功能
- 程序化海岸世界：7 段地形块（Perlin 高度场与原版一致）、道路/护栏/路沿/路灯，棕榈、树、松、岩石、遮阳伞实例化绘制
- 5 组 Gerstner 海浪（泡沫/菲涅尔/太阳与月亮高光）、天空（日月、星空、银河、云）、雾
- 昼夜循环：时刻滑块、时间流动、流速；晚上路灯光池、车灯、萤火虫、灯塔光束
- 鹈鹕骑手：曲柄踩踏与特技姿态、转向、头部注视、张嘴吞鱼、眨眼、展翅；头盔/墨镜/围巾开关；Verlet 布料围巾
- 玩法：自动驾驶追鱼、变道抓鱼、跳跃、特技、车铃、鹈鹕大叫、自动/手动 5 档变速、14 个成就、得分弹字
- 特效：鱼光晕、跃出水面的鱼、尘土/闪光/水花粒子、彩带、速度线、鸟群（海鸥/鹈鹕）、帆船
- 5 个镜头：自由环绕、追随、侧面跟拍、电影运镜（含黑边/切镜闪黑）、鹈鹕视角
- HUD：玻璃面板（背景模糊）、速度表、踏频/档位/里程、鱼数与成就、时间条、提示条、Toast、参数面板（可折叠分组）、帮助、开场卡片
- 软件合成音效 + 生成式音乐（跟随踏频）
- 画质预设 流畅/均衡/精美/极致 + 自适应分辨率
- URL 参数：`?q=low|medium|high|ultra`、`?time=17.35`、`?cam=orbit|chase|side|cine|pov`、`?autostart=1`（分享按钮会生成这些参数）

## 操作
| 按键 | 作用 |
|---|---|
| W / ↑ | 加速 |
| S / ↓ | 刹车 |
| A / D、← / → | 左右变道 |
| 空格 | 跳跃 |
| T | 特技（放手展翅 + 抬前轮） |
| B | 车铃 |
| H | 鹈鹕大叫 |
| C | 切换镜头；1–5 直接选镜头 |
| Q / E | 手动降档 / 升档（之后会恢复自动变速） |
| N | 时间快进 3 小时 |
| M | 音乐开关 |
| P | 暂停 / 继续 |
| U | 隐藏界面 |
| K | 截图 |
| F | 全屏 |
| F3 | 性能信息 |

- **鼠标**：拖动超过 10px 进入自由环绕视角并旋转；滚轮缩放；单击车铃/单车会响铃，单击鹈鹕会让它大叫，单击远处海面会让一条鱼跃出水面。
- **触屏**（粗指针设备自动启用）：◀ ▶ 变道，▼ 刹车，▲ 加速（支持多点按住），🔔 车铃，🪽 特技，⤒ 跳跃；手指从滑块轨道上按下后可以拖动滑块。
- 顶部工具栏：截图、音乐、分享链接、全屏、参数、帮助、GitHub。

## 引擎
需要官方 **Axmol `release/2.x` 分支**（已在 2.11.6 / de73de2 上测试）：
```bash
git clone -b release/2.x https://github.com/axmolengine/axmol axmol
cd axmol && pwsh setup.ps1      # 下载 cmake / axslcc / emsdk 等到 tools/external
export AX_ROOT=$PWD             # Windows: setx AX_ROOT <路径>
```

## 构建
**Linux 桌面**（脚本里的路径按本机环境写，使用前请修改 `tools/env_toolchain.sh`）：
```bash
bash tools/build_linux.sh       # → build/bin/PelicanBike/PelicanBike
```
也可以用通用 CMake 流程：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`。

**Windows 桌面**：使用 Axmol 的标准 CMake 流程（VS 2022）：`cmake -S . -B build_win -G "Visual Studio 17 2022" -A x64`，然后 `cmake --build build_win --config Release`。注意：本仓库没有 `proj.win32` 目录，Windows 桌面构建未经测试。

**WASM**（需要 emsdk 和 PATH 里的 pwsh；单线程 `AX_WASM_THREADS=0`）：
```bash
env -i HOME=$HOME PATH=/usr/bin:/bin bash tools/build_wasm.sh   # → build_wasm/bin/PelicanBike/PelicanBike.html
python3 tools/wasm_serve.py --root build_wasm/bin/PelicanBike --port 8089
# 浏览器打开 http://127.0.0.1:8089/PelicanBike.html
```
`wasm_serve.py` 会附带 COOP/COEP/CORP 响应头，所以不能直接双击 html 打开。`bash tools/package_zip.sh` 会把构建结果打包成 `PelicanBike_web.zip`（包含 web/、wasm_serve.py、启动.bat、说明.txt）。

## 持续集成 / 发布 / 在线试玩
- 工作流 `.github/workflows/build.yml` 会在 CI 里克隆官方 Axmol `release/2.x` 并运行 `setup.ps1`（引擎和工具链会缓存），然后调用 `tools/ci/build.ps1`（即 `axmol -p <平台> -O3`）。
- **推送 `v*` 标签**（例如 `git tag v1.0.0 && git push origin v1.0.0`）会构建 Windows x64（MSVC）、macOS arm64（.app 打成 zip）、Android arm64 APK 和 Web，并把 `PelicanBike-windows-x64.zip`、`PelicanBike-macos.zip`、`PelicanBike-android.apk`、`PelicanBike-web.zip` 上传到该标签的 GitHub Release。也可以在 Actions 页面手动运行（workflow_dispatch），手动运行只产出 artifacts。
- **推送到 `main`** 只构建 Web 并部署到 GitHub Pages：[https://tkzcfc.github.io/PelicanBike/](https://tkzcfc.github.io/PelicanBike/)。Web 版是单线程的（`AX_WASM_THREADS=0`），不需要 COOP/COEP 响应头。
- 需要先启用 Pages：仓库 Settings → Pages → Build and deployment → **Source = GitHub Actions**。
- Android 签名（可选）：在仓库 Secrets 里配置 `ANDROID_KEYSTORE_BASE64`（keystore 文件的 base64）、`ANDROID_KEYSTORE_PASSWORD`、`ANDROID_KEY_ALIAS`、`ANDROID_KEY_PASSWORD`。不配置时 release APK 用 debug key 签名（可以安装，但不能上架）。
- macOS 包没有签名和公证：首次打开时请右键 → 打开，或者运行 `xattr -dr com.apple.quarantine PelicanBike.app`。
- 渲染后端：自研 3D 层使用 OpenGL，所以 `CMakeLists.txt` 强制 Windows 使用原生 GL 3.3（不用 ANGLE），macOS 使用 OpenGL（不用 Metal），Android 使用 GLES 3.0。

## 用到的 Axmol 能力
- 窗口与 GL 上下文（`RenderViewImpl`，桌面 GL 3.3 core / WebGL2）、Director 主循环
- 输入：`EventListenerKeyboard`、`EventListenerMouse`、`EventListenerTouchAllAtOnce`
- `FileUtils` 资源读取（Content/：网格数据、纹理、字体、emoji 贴图）
- 2D：`Label`（TTF）、`DrawNode`、`Sprite` 用于 HUD、开场卡片和参数面板
- 音频：OpenAL（Axmol 自带的 3rdparty），合成器把 PCM 流式推入
- 渲染接入：整个 3D 帧都在一个 `CallbackCommand`（globalZ −9000）中完成，结束时调用 `backend::OpenGLState::reset()`，保持 Axmol 的状态缓存一致
- Emscripten WASM 目标（Axmol 的 wasm 平台层）

## 从零重写的部分
- **自研 GL 渲染层** `Source/gfx`：Shader、Mesh（VAO/VBO/IBO，24 字节压缩顶点，材质写在顶点里）、实例缓冲、纹理、RenderTarget（含 MSAA）、统计
- **std140 "Frame" UBO**：相机、日月、调色板、雾、波相位、环境光立方体，所有程序共用
- **着色器**（`Source/game/Shaders.h`）：Gerstner 海洋、天空、地形、道路（凹凸贴图）、PBR 光照（GGX + Lambert、路灯/车灯点光与聚光、鸟喙清漆层）
- **阴影**：2048² 深度图，纹素对齐，PCF 5 采样
- **HDR + MSAA 4×**（RGBA16F）、**UnrealBloomPass 移植**（5 级 mip 可分离高斯）、色彩分级（暗角/暖色/颗粒）、**ACES Filmic + sRGB**，以及 NaN/Inf 清理
- **环境光照**：CPU 积分的解析天空"环境光立方体" + 天空反射（时间变化超过 0.18h 时刷新，与 JS 的 PMREM 刷新一致），按法线方向的环境光色调
- **骑手**：48 块刚性骨骼的 uniform 骨骼调色板，数据由原版 `pelican.js` / `bicycle.js` 烘焙（16 个曲柄姿态 × 2 个特技姿态在 CPU 混合），运行时再叠加转向、车轮、头部注视、嘴部、眨眼，脖子是 CPU 生成的管状网格
- **粒子与特效**：点精灵粒子（尘土/闪光/水花）、彩带、实例化鱼、Verlet 布料围巾、实例化鸟群、灯塔/帆船/萤火虫
- **音频**：`audio.js` 的软件合成器移植（振荡器/噪声、包络、RBJ 双二阶滤波、风/轮胎/海浪底噪、Schroeder 混响、压缩器、生成式音乐）
- **UI**：HUD、开场卡片、参数面板、帮助、Toast、背景模糊玻璃面板、触屏按钮、窄屏布局
- **画质**：4 档预设（渲染缩放/阴影尺寸/MSAA/泛光/道具密度）+ 移植自 `main.js` adaptQuality 的自适应分辨率

## 性能
- 真实 GPU（Windows Chrome WASM）：**稳定 60 FPS**，渲染缩放 1.00，update 0.3 ms，**65 次绘制**，约 557k 三角形
- 旧移植版（每个物体一个 Axmol 节点）：同一台软件渲染机上为 **708 次绘制、3.0 FPS**；新版为 56–62 次绘制
- CPU update 0.05–0.08 ms（桌面）

## 目录结构
```
CMakeLists.txt  cmake/          Axmol 工程配置
proj.linux/ proj.wasm/          平台入口 main.cpp
Source/AppDelegate.*            启动与窗口
Source/MainScene.*              输入与胶水层、测试钩子
Source/Hud.* Intro.*            HUD / 参数面板 / 开场卡片
Source/Platform.*               全屏、分享链接、打开 URL、截图、URL 参数
Source/gfx/                     自研 GL 层
Source/game/                    Shaders.h、Game（模拟+帧图）、Rider、Fx、Extras、Common（JS 工具/噪声/调色板移植）
Source/audio/                   软件合成器
Content/data tex fonts emoji    烘焙网格、纹理、字体子集、emoji 贴图
tools/                          烘焙、构建、打包、服务器、截图对比/审计脚本、字体子集
ARCHITECTURE.md PROGRESS.md     架构说明与开发日志
```

## 字体子集
`bash tools/font_subset.sh` 扫描 `Source/` 里用到的所有字符（再加上 ASCII 和 UI 符号），从 `tools/NotoSansSC-full.ttf` 生成 `Content/fonts` 下的子集字体。修改界面文字后需要重新运行。

## 许可与致谢
- 原作与视觉参考：[riba2534/claude-opus-5-5-demo](https://github.com/riba2534/claude-opus-5-5-demo)（pelican-bike，Three.js）。原作的着色器、几何与玩法都归原作者，使用前请遵循原仓库的许可证。
- 引擎：[Axmol](https://github.com/axmolengine/axmol)（MIT）。
- 字体：Noto Sans SC / Noto Sans（SIL OFL）；emoji 贴图由 Noto Color Emoji 渲染（OFL）。
- 本移植的仓库：https://github.com/tkzcfc/PelicanBike

