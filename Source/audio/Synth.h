// Software port of audio.js (pure WebAudio synthesis, no audio files): oscillator/noise voices with
// WebAudio-style exponential envelopes + RBJ biquads, wind/tyre/surf beds, Schroeder reverb bus,
// compressor, and the cadence-following generative music sequencer. Output is streamed through
// OpenAL (Axmol's backend on desktop + Emscripten), or rendered offline to WAV for verification.
#pragma once
#include <vector>
#include <cstdint>
#include <random>
namespace pb {
struct Biquad {
  enum T { LP, HP, BP } type = LP; float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void set(T t, float f, float q, float sr);
  float run(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};
struct Ramp {  // WebAudio AudioParam subset: value at t, then linear/exponential ramps
  struct Seg { double t; float v; bool expo; };
  std::vector<Seg> segs; float at(double t) const;
};
struct Voice {
  enum Src { SINE, TRI, SAW, NOISE } src = SINE;
  double t0 = 0, t1 = 0; Ramp freq, gain, ffreq; float detune = 0, vibF = 0, vibA = 0, pan = 0, send = 0; int bus = 0;  // 0 sfx 1 music
  int nf = 0; Biquad f[2]; Biquad::T ft[2]; float fq[2] = {1, 1}, ff[2] = {0, 0}; bool fRamp = false; float phase = 0, vphase = 0; uint32_t noisePos = 0;
};
class Synth {
 public:
  bool init(bool openAL = true);  // opens OpenAL streaming; returns false (silent) when no device
  void shutdown();
  void pump(float dt);  // refill the stream; call every frame
  void renderOffline(float seconds, const char* wavPath);
  static void selfTest(const char* wavPath);  // scripted 8 s scene -> WAV (bell, honk, gulp, splash, whoosh, thump, music)
  struct Params { float speed = 0, night = 0, cadence = 0; bool pedaling = true, airborne = false; } p;
  float volume = 0.8f, musicVolume = 0.55f; bool musicOn = true, muted = false, enabled = false;
  void bell(); void honk(); void gulp(bool golden); void splash(); void thump(); void whoosh(); void click(float vol);
 private:
  static constexpr int SR = 44100;
  double now() const { return double(rendered) / SR; }
  Voice& tone(float freq, int type, double t, float dur, float vol, float attack = 0.005f, int bus = 0, float detune = 0, float glide = 0, float send = 0);
  Voice& noiseV(double t, float dur, double offset);
  void gull(double t); void playStep(int step, double t, float dur); void schedule(double until);
  void mix(float* out, int frames);
  std::vector<Voice> voices; std::vector<float> noise, brown; uint64_t rendered = 0; std::mt19937 rng{7};
  float rnd() { return std::uniform_real_distribution<float>(0, 1)(rng); }
  // beds
  Biquad windF, rollF, surfF; float windG = 0, rollG = 0, windFreq = 250, rollFreq = 160, mvol = 0.55f, master = 0.8f; uint32_t windPos = 0, rollPos = 0, surfPos = 0;
  double nextTick = 0, nextGull = 4, nextNote = 0.3; int step = 0;
  // reverb (Schroeder/Freeverb-style approximation of the 2.8 s convolver)
  struct Comb { std::vector<float> b; int i = 0; float fb, damp, s = 0; float run(float x) { float o = b[i]; s = o * (1 - damp) + s * damp; b[i] = x + s * fb; if (++i >= int(b.size())) i = 0; return o; } };
  struct AP { std::vector<float> b; int i = 0; float run(float x) { float o = b[i]; float y = -x + o; b[i] = x + o * 0.5f; if (++i >= int(b.size())) i = 0; return y; } };
  Comb comb[2][4]; AP ap[2][2]; float compEnv = 0;
  // OpenAL
  unsigned src = 0, bufs[8] = {}; bool al = false; void* dev = nullptr; void* ctx = nullptr; std::vector<int16_t> pcm; std::vector<float> bL, bR, bM, bRV, bOut;
};
}  // namespace pb
