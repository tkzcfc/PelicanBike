#include "Platform.h"
#include "axmol.h"
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <cstdlib>
#include <ctime>
#if AX_TARGET_PLATFORM == AX_PLATFORM_WIN32 || AX_TARGET_PLATFORM == AX_PLATFORM_LINUX || AX_TARGET_PLATFORM == AX_PLATFORM_MAC
#  define PB_GLFW 1  // desktop: RenderViewImpl is GLFW-backed
#  include "platform/RenderViewImpl.h"
#else
#  define PB_GLFW 0
#endif
USING_NS_AX;
namespace plat {
void toggleFullscreen() {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  EmscriptenFullscreenChangeEvent st; emscripten_get_fullscreen_status(&st);
  if (st.isFullscreen) emscripten_exit_fullscreen(); else emscripten_request_fullscreen("#canvas", 1);
#elif PB_GLFW
  auto v = dynamic_cast<RenderViewImpl*>(Director::getInstance()->getRenderView()); if (!v) return;
  if (v->isFullscreen()) v->setWindowed(1280, 720); else v->setFullscreen();
#endif  // mobile: always fullscreen
}
bool copyText(const std::string& s) {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  return EM_ASM_INT({ try { navigator.clipboard.writeText(UTF8ToString($0)); return 1; } catch (e) { return 0; } }, s.c_str()) != 0;
#elif PB_GLFW
  auto v = dynamic_cast<RenderViewImpl*>(Director::getInstance()->getRenderView()); if (!v) return false;
  glfwSetClipboardString(v->getWindow(), s.c_str()); return true;
#else
  (void)s; return false;
#endif
}
void openUrl(const std::string& u) { Application::getInstance()->openURL(u); }
std::string urlParam(const char* name) {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  char* r = (char*)EM_ASM_PTR({ var v = new URLSearchParams(location.search).get(UTF8ToString($0)); return v ? stringToNewUTF8(v) : 0; }, name);
  if (!r) return ""; std::string s(r); free(r); return s;
#else
  std::string k = "PB2_"; for (const char* c = name; *c; c++) k += char(toupper(*c)); const char* v = std::getenv(k.c_str()); return v ? v : "";
#endif
}
std::string pageUrl() {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  char* r = (char*)EM_ASM_PTR({ return stringToNewUTF8(location.origin + location.pathname); }); std::string s(r); free(r); return s;
#else
  return "https://tkzcfc.github.io/PelicanBike/";
#endif
}
void screenshot(std::function<void(bool, const std::string&)> done) {
  std::string name = "pelican-ride-" + std::to_string(long(std::time(nullptr))) + ".png";
  utils::captureScreen([done](bool ok, std::string_view f) {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
    if (ok) { auto d = FileUtils::getInstance()->getDataFromFile(f);  // MEMFS -> browser download (toBlob equivalent)
      EM_ASM({ try {  // HEAPU8 may be a SharedArrayBuffer view (pthread-enabled runtime): Blob rejects shared views, so copy first
        var u = new Uint8Array($1); u.set(HEAPU8.subarray($0, $0 + $1)); var b = new Blob([u], {type: 'image/png'}); var a = document.createElement('a'); a.href = URL.createObjectURL(b);
        a.download = UTF8ToString($2); document.body.appendChild(a); a.click(); a.remove(); setTimeout(function() { URL.revokeObjectURL(a.href); }, 2000); } catch (e) { console.error('[pb2] screenshot download failed', e); } }, d.getBytes(), int(d.getSize()), std::string(f.substr(f.rfind('/') + 1)).c_str()); }
#endif
    done(ok, std::string(f));
  }, name);
}
}  // namespace plat
void plat::resumeAudio() {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  EM_ASM({ try { if (typeof AL !== 'undefined' && AL.contexts) for (var k in AL.contexts) { var c = AL.contexts[k]; if (c && c.audioCtx && c.audioCtx.state !== 'running') c.audioCtx.resume(); } } catch (e) {} });
#endif
}
bool plat::coarsePointer() {
#if AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID || AX_TARGET_PLATFORM == AX_PLATFORM_IOS
  return true;  // touch pads on phones/tablets
#elif AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  return EM_ASM_INT({ return (window.matchMedia && matchMedia('(pointer: coarse)').matches) ? 1 : 0; });
#else
  return false;
#endif
}
void plat::setEnv(const char* k, const std::string& v) {
#ifdef _WIN32
  _putenv_s(k, v.c_str());
#else
  setenv(k, v.c_str(), 1);
#endif
}
