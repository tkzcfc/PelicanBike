#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "AppDelegate.h"
#include "MainScene.h"

#define USE_AUDIO_ENGINE 1

#if USE_AUDIO_ENGINE
#    include "audio/AudioEngine.h"
#endif

#if _AX_TESTS
#   include "doctest/doctest.h"
#endif

#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
#    include <emscripten.h>
#    include <emscripten/html5.h>
#endif

using namespace ax;

// Device pixels per CSS/design pixel. WASM: window.devicePixelRatio (override ?dpr=1|2|3), desktop: PB2_DPR
// (simulates a HiDPI screen). The canvas/window backing store is design*dpr device pixels so nothing is
// upscaled by the browser/compositor; UI keeps its design-unit layout via SHOW_ALL.
static float deviceScale() {
    float d = 1.f;
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
    d = float(emscripten_get_device_pixel_ratio());
    d = float(EM_ASM_DOUBLE({ var v = parseFloat(new URLSearchParams(location.search).get('dpr')); return v > 0 ? v : $0; }, d));
#else
    if (const char* e = std::getenv("PB2_DPR")) d = float(std::atof(e));
#endif
    return std::max(1.f, std::min(3.f, d > 0 ? d : 1.f));
}

static ax::Size designResolutionSize = ax::Size(1280, 720);

AppDelegate::AppDelegate() {}

AppDelegate::~AppDelegate() {}

// if you want a different context, modify the value of gfxContextAttrs
// it will affect all platforms
void AppDelegate::initGfxContextAttrs()
{
    // set graphics context attributes: red,green,blue,alpha,depth,stencil,multisamplesCount
    GfxContextAttrs gfxContextAttrs = {8, 8, 8, 8, 24, 8, 0};
    // since axmol-2.2 vsync was enabled in engine by default
    if (std::getenv("PB2_NOVSYNC")) gfxContextAttrs.vsync = false;

    RenderView::setGfxContextAttrs(gfxContextAttrs);
}

bool AppDelegate::applicationDidFinishLaunching()
{
    // initialize director
    auto director = Director::getInstance();
    auto renderView   = director->getRenderView();
    if (const char* w = std::getenv("PB2_WIN")) { int a = 0, b = 0; if (sscanf(w, "%dx%d", &a, &b) == 2 && a > 0 && b > 0) designResolutionSize = ax::Size(a, b); }
    if (!renderView)
    {
#if (AX_TARGET_PLATFORM != AX_PLATFORM_ANDROID) && (AX_TARGET_PLATFORM != AX_PLATFORM_IOS)
        const float dpr = deviceScale();
        renderView = RenderViewImpl::createWithRect(
            "PelicanBike", ax::Rect(0, 0, std::round(designResolutionSize.width * dpr), std::round(designResolutionSize.height * dpr)));
#    if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
        // backing store = CSS size * DPR; keep the on-page size in CSS px
        emscripten_set_element_css_size("#canvas", designResolutionSize.width, designResolutionSize.height);
#    endif
#else
        renderView = RenderViewImpl::create("PelicanBike");
#endif
        director->setRenderView(renderView);
    }

    // turn on display FPS
    director->setStatsDisplay(false);

    // set FPS. the default value is 1.0/60 if you don't call this
    director->setAnimationInterval(std::getenv("PB2_NOVSYNC") ? 1.0f / 1000 : 1.0f / 60);

    // Set the design resolution
    renderView->setDesignResolutionSize(designResolutionSize.width, designResolutionSize.height,
                                    ResolutionPolicy::SHOW_ALL);
    // rasterize TTF glyphs (and size sprite textures) at framebuffer pixels instead of design pixels,
    // otherwise SHOW_ALL magnifies 1x glyph atlases on HiDPI / large windows
    director->setContentScaleFactor(std::max(renderView->getScaleX(), renderView->getScaleY()));
    AXLOGI("[pb2] frame {}x{} design {}x{} contentScale {}", renderView->getFrameSize().width, renderView->getFrameSize().height,
           designResolutionSize.width, designResolutionSize.height, director->getContentScaleFactor());

#if !_AX_TESTS
    // create a scene. it's an autorelease object
    auto scene = utils::createInstance<MainScene>();

    // run
    director->runWithScene(scene);
#endif

    return true;
}

// This function will be called when the app is inactive. Note, when receiving a phone call it is invoked.
void AppDelegate::applicationDidEnterBackground()
{
    Director::getInstance()->stopAnimation();

#if USE_AUDIO_ENGINE
    AudioEngine::pauseAll();
#endif
}

// this function will be called when the app is active again
void AppDelegate::applicationWillEnterForeground()
{
    Director::getInstance()->startAnimation();

#if USE_AUDIO_ENGINE
    AudioEngine::resumeAll();
#endif
}

void AppDelegate::applicationWillQuit() {}

#if _AX_TESTS
int AppDelegate::run(int argc, char** argv) {
    AXLOGI("Running unit tests...\n");
    fflush(stdout);
    AXLOGI("Default resource path: {}\n", FileUtils::getInstance()->getDefaultResourceRootPath());
    AXLOGI("Writable path: {}\n", FileUtils::getInstance()->getWritablePath());
    {
        for (auto& path : FileUtils::getInstance()->getSearchPaths())
            AXLOGI("Search path: {}\n", path);
    }
    fflush(stdout);

    ax::Director::getInstance()->init();

    doctest::Context context;

    //context.addFilter("test-case-exclude", "*math*"); // exclude test cases with "math" in their name
    //context.setOption("abort-after", 5);              // stop test execution after 5 failed assertions

    //context.setOption("order-by", "name");            // sort the test cases by their name

    context.applyCommandLine(argc, argv);

    // overrides
    context.setOption("no-breaks", true);             // don't break in the debugger when assertions fail

    int res = context.run(); // run
    return res;
}
#endif
