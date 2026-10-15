// Small platform shims for the HUD tool buttons (fullscreen, share link, open URL, screenshot, URL params)
#pragma once
#include <string>
#include <functional>
namespace plat {
void setEnv(const char* k, const std::string& v);  // portable setenv (MSVC: _putenv_s)
void toggleFullscreen();
bool copyText(const std::string& s);
void openUrl(const std::string& u);
void resumeAudio();
bool coarsePointer();
std::string urlParam(const char* name);  // WASM: ?name= from location.search; desktop: env PB2_<NAME>
std::string pageUrl();                    // base URL for share links
void screenshot(std::function<void(bool, const std::string&)> done);
}
