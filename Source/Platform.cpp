#include "Platform.h"
#include "axmol.h"
#include "platform/RenderViewImpl.h"
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <cstdlib>
#include <ctime>
USING_NS_AX;
namespace plat {
void toggleFullscreen() {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  EmscriptenFullscreenChangeEvent st; emscripten_get_fullscreen_status(&st);
  if (st.isFullscreen) emscripten_exit_fullscreen(); else emscripten_request_fullscreen("#canvas", 1);
#else
  auto v = dynamic_cast<RenderViewImpl*>(Director::getInstance()->getRenderView()); if (!v) return;
  if (v->isFullscreen()) v->setWindowed(1280, 720); else v->setFullscreen();
#endif
}
bool copyText(const std::string& s) {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  return EM_ASM_INT({ try { navigator.clipboard.writeText(UTF8ToString($0)); return 1; } catch (e) { return 0; } }, s.c_str()) != 0;
#else
  auto v = dynamic_cast<RenderViewImpl*>(Director::getInstance()->getRenderView()); if (!v) return false;
  glfwSetClipboardString(v->getWindow(), s.c_str()); return true;
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
  return "http://127.0.0.1:8089/PelicanBike.html";
#endif
}
void screenshot(std::function<void(bool, const std::string&)> done) {
  std::string name = "pelican-ride-" + std::to_string(long(std::time(nullptr))) + ".png";
  utils::captureScreen([done](bool ok, std::string_view f) {
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
    if (ok) { auto d = FileUtils::getInstance()->getDataFromFile(f);  // MEMFS -> browser download (toBlob equivalent)
      EM_ASM({ var b = new Blob([HEAPU8.subarray($0, $0 + $1)], {type: 'image/png'}); var a = document.createElement('a'); a.href = URL.createObjectURL(b);
        a.download = UTF8ToString($2); a.click(); setTimeout(function() { URL.revokeObjectURL(a.href); }, 2000); }, d.getBytes(), int(d.getSize()), std::string(f.substr(f.rfind('/') + 1)).c_str()); }
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
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
  return EM_ASM_INT({ return (window.matchMedia && matchMedia('(pointer: coarse)').matches) ? 1 : 0; });
#else
  return false;
#endif
}
