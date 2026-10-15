#include "audio/Synth.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
namespace pb {
static float midi(float n) { return 440.f * std::pow(2.f, (n - 69) / 12); }
void Biquad::set(T t, float f, float q, float sr) {
  type = t; f = std::clamp(f, 10.f, sr * 0.45f); float w = 2 * float(M_PI) * f / sr, cw = std::cos(w), sw = std::sin(w);
  float Q = t == BP ? q : std::pow(10.f, q / 20);  // WebAudio: LP/HP Q in dB, BP linear
  float al = sw / (2 * std::max(Q, 1e-3f)), a0 = 1 + al;
  if (t == LP) { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; }
  else if (t == HP) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; }
  else { b0 = al; b1 = 0; b2 = -al; }
  b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2 * cw / a0; a2 = (1 - al) / a0;
}
float Ramp::at(double t) const {
  if (segs.empty()) return 0; if (t <= segs[0].t) return segs[0].v;
  for (size_t i = 1; i < segs.size(); i++) if (t < segs[i].t) { const Seg &a = segs[i - 1], &b = segs[i]; float u = float((t - a.t) / (b.t - a.t));
      if (b.expo && a.v > 0 && b.v > 0) return a.v * std::pow(b.v / a.v, u); return a.v + (b.v - a.v) * u; }
  return segs.back().v;
}

bool Synth::init(bool openAL) {
  voices.reserve(256);
  noise.resize(SR * 2); for (auto& v : noise) v = rnd() * 2 - 1;
  brown.resize(SR * 3); float last = 0; for (auto& v : brown) { last = (last + 0.02f * (rnd() * 2 - 1)) / 1.02f; v = last * 3.5f; }
  windF.set(Biquad::BP, 250, 0.6f, SR); rollF.set(Biquad::LP, 260, 1, SR); surfF.set(Biquad::LP, 700, 1, SR);
  static const int CL[4] = {1557, 1617, 1491, 1422}, AL_[2] = {556, 441};
  for (int c = 0; c < 2; c++) { for (int k = 0; k < 4; k++) { comb[c][k].b.assign(CL[k] * 2 + c * 23, 0); comb[c][k].fb = 0.86f; comb[c][k].damp = 0.3f; }
    for (int k = 0; k < 2; k++) ap[c][k].b.assign(AL_[k] + c * 17, 0); }
  enabled = true;
  if (!openAL) return true;
  if (!alcGetCurrentContext()) { ALCdevice* d = alcOpenDevice(nullptr); if (!d) return false; ALCcontext* c = alcCreateContext(d, nullptr); if (!c) { alcCloseDevice(d); return false; } alcMakeContextCurrent(c); dev = d; ctx = c; }
  alGenSources(1, &src); alGenBuffers(8, bufs); if (alGetError() != AL_NO_ERROR) return false;
  pcm.resize(2048 * 2);
  for (int i = 0; i < 8; i++) { std::vector<float> f(2048 * 2); mix(f.data(), 2048); for (int k = 0; k < 4096; k++) pcm[k] = int16_t(std::clamp(f[k], -1.f, 1.f) * 32767);
    alBufferData(bufs[i], AL_FORMAT_STEREO16, pcm.data(), 4096 * 2, SR); }
  alSourceQueueBuffers(src, 8, bufs); alSourcePlay(src); al = true; return true;
}
void Synth::shutdown() {
  if (al) { alSourceStop(src); alDeleteSources(1, &src); alDeleteBuffers(8, bufs); al = false; }
  if (ctx) { alcMakeContextCurrent(nullptr); alcDestroyContext((ALCcontext*)ctx); alcCloseDevice((ALCdevice*)dev); ctx = dev = nullptr; }
}
void Synth::pump(float) {
  if (!al) return;
  ALint done = 0; alGetSourcei(src, AL_BUFFERS_PROCESSED, &done); std::vector<float>& f = bOut; f.resize(4096);
  while (done-- > 0) { ALuint b; alSourceUnqueueBuffers(src, 1, &b); mix(f.data(), 2048);
    for (int k = 0; k < 4096; k++) pcm[k] = int16_t(std::clamp(f[k], -1.f, 1.f) * 32767);
    alBufferData(b, AL_FORMAT_STEREO16, pcm.data(), 4096 * 2, SR); alSourceQueueBuffers(src, 1, &b); }
  ALint st; alGetSourcei(src, AL_SOURCE_STATE, &st); if (st != AL_PLAYING) alSourcePlay(src);  // recover from underrun
  alSourcef(src, AL_GAIN, muted ? 0.f : 1.f);
}
void Synth::renderOffline(float seconds, const char* path) {
  int n = int(seconds * SR); std::vector<float> f(size_t(n) * 2); for (int i = 0; i < n; i += 1024) mix(&f[size_t(i) * 2], std::min(1024, n - i));
  FILE* o = fopen(path, "wb"); if (!o) return; auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, o); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, o); };
  fwrite("RIFF", 1, 4, o); w32(36 + n * 4); fwrite("WAVEfmt ", 1, 8, o); w32(16); w16(1); w16(2); w32(SR); w32(SR * 4); w16(4); w16(16); fwrite("data", 1, 4, o); w32(n * 4);
  for (auto v : f) { int16_t s = int16_t(std::clamp(v, -1.f, 1.f) * 32767); fwrite(&s, 2, 1, o); } fclose(o);
}

void Synth::selfTest(const char* path) {
  Synth s; s.init(false); s.p.speed = 7; s.p.cadence = 90; int n = 8 * SR; std::vector<float> f(size_t(n) * 2);
  struct Ev { float t; int k; } ev[] = {{0.5f, 0}, {1.5f, 1}, {2.5f, 2}, {3.2f, 3}, {4.0f, 4}, {4.6f, 5}, {5.5f, 6}};
  size_t e = 0;
  for (int i = 0; i < n; i += 512) { float t = float(i) / SR;
    while (e < sizeof(ev) / sizeof(ev[0]) && ev[e].t <= t) { switch (ev[e].k) { case 0: s.bell(); break; case 1: s.honk(); break; case 2: s.gulp(false); break; case 3: s.gulp(true); break; case 4: s.splash(); break; case 5: s.whoosh(); break; default: s.thump(); } e++; }
    if (t > 6.5f) s.p.pedaling = false;
    s.mix(&f[size_t(i) * 2], std::min(512, n - i)); }
  FILE* o = fopen(path, "wb"); if (!o) return; auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, o); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, o); };
  fwrite("RIFF", 1, 4, o); w32(36 + n * 4); fwrite("WAVEfmt ", 1, 8, o); w32(16); w16(1); w16(2); w32(SR); w32(SR * 4); w16(4); w16(16); fwrite("data", 1, 4, o); w32(n * 4);
  for (auto v : f) { int16_t q = int16_t(std::clamp(v, -1.f, 1.f) * 32767); fwrite(&q, 2, 1, o); } fclose(o);
}
Voice& Synth::tone(float freq, int type, double t, float dur, float vol, float attack, int bus, float detune, float glide, float send) {
  voices.emplace_back(); Voice& v = voices.back(); v.src = Voice::Src(type); v.t0 = t; v.t1 = t + dur + 0.05; v.bus = bus; v.detune = detune; v.send = send;
  v.freq.segs = {{t, freq, false}}; if (glide > 0) v.freq.segs.push_back({t + dur, glide, true});
  v.gain.segs = {{t, 0.0001f, false}, {t + attack, vol, true}, {t + dur, 0.0001f, true}}; return v;
}
Voice& Synth::noiseV(double t, float dur, double offset) {
  voices.emplace_back(); Voice& v = voices.back(); v.src = Voice::NOISE; v.t0 = t; v.t1 = t + dur; v.noisePos = uint32_t(offset * SR) % noise.size(); return v;
}
void Synth::click(float vol) {
  if (!enabled) return; double t = now(); Voice& v = noiseV(t, 0.03f, rnd()); v.nf = 1; v.ft[0] = Biquad::HP; v.ff[0] = 3500;
  v.gain.segs = {{t, vol, false}, {t + 0.02, 0.0001f, true}};
}
void Synth::bell() {
  if (!enabled) return; double t0 = now();
  for (double off : {0.0, 0.16}) { double t = t0 + off; float base = 2350; const float R[4] = {1, 2.63f, 4.17f, 5.43f};
    for (int i = 0; i < 4; i++) tone(base * R[i], 0, t, 1.4f - i * 0.25f, 0.18f / (i + 1), 0.005f, 0, i * 4.f, 0, 1);
    tone(base * 1.004f, 0, t, 1.2f, 0.08f); }
}
void Synth::honk() {
  if (!enabled) return; double t = now(); Voice& v = tone(190, 2, t, 0.5f, 0.7f, 0.03f, 0, 0, 0, 1); v.t1 = t + 0.55;
  v.freq.segs = {{t, 190, false}, {t + 0.08, 240, false}, {t + 0.45, 120, true}}; v.vibF = 28; v.vibA = 18;
  v.nf = 2; v.ft[0] = v.ft[1] = Biquad::BP; v.ff[0] = 720; v.fq[0] = 4; v.ff[1] = 1250; v.fq[1] = 5;
}
void Synth::gulp(bool golden) {
  if (!enabled) return; double t = now(); tone(420, 0, t, 0.18f, 0.35f, 0.005f, 0, 0, 90); tone(260, 0, t + 0.12, 0.2f, 0.25f, 0.005f, 0, 0, 70);
  static const int G[5] = {79, 83, 86, 91, 95}, N[3] = {76, 81, 88}; int n = golden ? 5 : 3;
  for (int i = 0; i < n; i++) tone(midi(golden ? G[i] : N[i]), 1, t + 0.05 + i * 0.07, 0.6f, 0.12f, 0.005f, 0, 0, 0, 1);
}
void Synth::splash() {
  if (!enabled) return; double t = now(); Voice& v = noiseV(t, 0.6f, rnd()); v.nf = 1; v.ft[0] = Biquad::BP; v.fq[0] = 1; v.fRamp = true; v.ffreq.segs = {{t, 2400, false}, {t + 0.5, 400, true}};
  v.gain.segs = {{t, 0.3f, false}, {t + 0.55, 0.0001f, true}}; v.pan = -0.5f;
}
void Synth::thump() { if (!enabled) return; tone(110, 0, now(), 0.25f, 0.5f, 0.005f, 0, 0, 45); click(0.2f); }
void Synth::whoosh() {
  if (!enabled) return; double t = now(); Voice& v = noiseV(t, 0.6f, rnd()); v.nf = 1; v.ft[0] = Biquad::BP; v.fq[0] = 1.5f; v.fRamp = true; v.ffreq.segs = {{t, 300, false}, {t + 0.35, 2200, true}};
  v.gain.segs = {{t, 0.0001f, false}, {t + 0.15, 0.35f, true}, {t + 0.5, 0.0001f, true}};
}
void Synth::gull(double t) {
  int n = 2 + int(rnd() * 3); float pan = -0.3f - rnd() * 0.6f;
  for (int i = 0; i < n; i++) { double s = t + i * 0.28; Voice& v = tone(1500, 2, s, 0.24f, 0.05f, 0.03f, 0, 0, 0, 1); v.t1 = s + 0.3;
    v.freq.segs = {{s, 1500, false}, {s + 0.06, 2300, true}, {s + 0.22, 1100, true}}; v.nf = 1; v.ft[0] = Biquad::BP; v.ff[0] = 2000; v.fq[0] = 3; v.pan = pan; }
}
void Synth::playStep(int st, double t, float dur) {
  static const int CH[4][5] = {{50, 57, 61, 64, 66}, {47, 54, 57, 61, 62}, {43, 50, 54, 57, 59}, {45, 52, 55, 59, 61}};
  int bar = (st / 16) % 4, s = st % 16; const int* ch = CH[bar];
  if (s == 0) { for (int i = 0; i < 5; i++) tone(midi(ch[i] + 12), 1, t, dur * 16, 0.045f, 0.4f, 1, i % 2 ? 6.f : -6.f); tone(midi(ch[0] - 12), 0, t, dur * 8, 0.16f, 0.02f, 1); }
  if (s == 8) tone(midi(ch[0] - 12 + (bar % 2 ? 7 : 0)), 0, t, dur * 6, 0.12f, 0.02f, 1);
  static const int PAT[8] = {0, 2, 3, 4, 1, 3, 2, 4};
  if (s % 2 == 0) { int n = ch[PAT[(s / 2) % 8]] + (s >= 8 && bar % 2 ? 24 : 12); tone(midi(n), 0, t, dur * 3.5f, 0.07f * (p.night > 0.5f ? 0.8f : 1), 0.005f, 1); tone(midi(n) * 2.001f, 0, t, dur * 1.5f, 0.012f, 0.005f, 1); }
  if (p.night > 0.6f) return;
  if (s == 0 || s == 10) tone(120, 0, t, 0.22f, 0.3f, 0.005f, 1, 0, 42);
  auto drum = [&](Biquad::T ft, float f, float q, float vol, float len) { Voice& v = noiseV(t, len + 0.04f, rnd()); v.bus = 1; v.nf = 1; v.ft[0] = ft; v.ff[0] = f; v.fq[0] = q; v.gain.segs = {{t, vol, false}, {t + len, 0.0001f, true}}; };
  if (s == 4 || s == 12) drum(Biquad::BP, 1800, 0.8f, 0.07f, 0.16f);
  if (s % 2 == 0) drum(Biquad::HP, 7000, 1, s % 4 == 2 ? 0.035f : 0.018f, 0.05f);
}
void Synth::schedule(double until) {
  if (!musicOn) { nextNote = std::max(nextNote, until); return; }
  float bpm = std::clamp(p.cadence > 0 ? p.cadence : 84.f, 72.f, 126.f), six = 60 / bpm / 4;
  while (nextNote < until) { playStep(step, nextNote, six); nextNote += six * (step % 2 ? 0.92f : 1.08f); step = (step + 1) % 256; }
}

void Synth::mix(float* out, int frames) {
  double t0 = now(); float dtS = 1.f / SR;
  // control-rate updates (once per block, like setTargetAtTime with small time constants)
  float v = p.speed, blk = frames * dtS;
  auto toward = [&](float& x, float target, float tc) { x += (target - x) * (1 - std::exp(-blk / tc)); };
  toward(windG, std::min(0.5f, v * v * 0.0022f), 0.2f); toward(windFreq, 250 + v * 70, 0.2f); windF.set(Biquad::BP, windFreq, 0.6f, SR);
  toward(rollG, p.airborne ? 0 : std::min(0.35f, v * 0.028f), 0.05f); toward(rollFreq, 160 + v * 22, 0.2f); rollF.set(Biquad::LP, rollFreq, 1, SR);
  toward(mvol, musicOn ? musicVolume : 0, 0.1f); toward(master, volume, 0.05f);
  if (!p.pedaling && v > 0.5f && t0 > nextTick) { click(0.05f); nextTick = t0 + std::max(0.012f, 0.35f / v); }
  if (t0 > nextGull) { gull(t0); nextGull = t0 + 7 + rnd() * 12; }
  if (nextNote < t0) nextNote = t0 + 0.05;
  schedule(t0 + blk + 0.12);
  // pooled scratch (no per-block heap traffic)
  bL.assign(frames, 0); bR.assign(frames, 0); bM.assign(frames * 2, 0); bRV.assign(frames, 0);
  float *L = bL.data(), *R = bR.data(), *M = bM.data(), *RV = bRV.data();
  // beds
  for (int i = 0; i < frames; i++) {
    double t = t0 + i * dtS;
    float w = windF.run(noise[windPos]) * windG; windPos = (windPos + 1) % noise.size();
    float r = rollF.run(brown[rollPos]) * rollG; rollPos = (rollPos + 1) % brown.size();
    float sg = 0.12f + 0.08f * std::sin(2 * float(M_PI) * 0.11f * float(t)); float s = surfF.run(brown[surfPos]) * sg; surfPos = (surfPos + 7) % brown.size() /*decorrelate*/;
    float pl = std::cos((-0.6f + 1) * float(M_PI) / 4), pr = std::sin((-0.6f + 1) * float(M_PI) / 4);
    L[i] += w + r + s * pl; R[i] += w + r + s * pr;
  }
  // voices
  for (auto& vc : voices) {
    if (vc.t0 >= t0 + blk || vc.t1 <= t0) continue;
    for (int k = 0; k < vc.nf; k++) if (!vc.fRamp && vc.f[k].b0 == 1 && vc.f[k].a1 == 0) vc.f[k].set(vc.ft[k], vc.ff[k], vc.fq[k], SR);
    float pl = std::cos((vc.pan + 1) * float(M_PI) / 4), pr = std::sin((vc.pan + 1) * float(M_PI) / 4); float det = std::pow(2.f, vc.detune / 1200);
    for (int i = 0; i < frames; i++) {
      double t = t0 + i * dtS; if (t < vc.t0 || t >= vc.t1) continue;
      float x;
      if (vc.src == Voice::NOISE) { x = noise[vc.noisePos]; vc.noisePos = (vc.noisePos + 1) % noise.size(); }
      else { float f = vc.freq.at(t) * det; if (vc.vibA > 0) { f += std::sin(vc.vphase) * vc.vibA; vc.vphase += 2 * float(M_PI) * vc.vibF * dtS; }
        vc.phase += f * dtS; vc.phase -= std::floor(vc.phase); float ph = vc.phase;
        x = vc.src == Voice::SINE ? std::sin(2 * float(M_PI) * ph) : vc.src == Voice::TRI ? 1 - 4 * std::fabs(ph - 0.5f) : 2 * ph - 1; }
      if (vc.fRamp && (i & 31) == 0) vc.f[0].set(vc.ft[0], vc.ffreq.at(t), vc.fq[0], SR);
      if (vc.nf == 2) x = vc.f[0].run(x) + vc.f[1].run(x); else if (vc.nf == 1) x = vc.f[0].run(x);
      x *= vc.gain.at(t);
      if (vc.bus == 1) { M[i * 2] += x; M[i * 2 + 1] += x; RV[i] += x * 0.5f; }
      else { if (vc.pan != 0) { L[i] += x * pl; R[i] += x * pr; } else { L[i] += x; R[i] += x; } if (vc.send > 0) RV[i] += x * vc.send; }
    }
  }
  voices.erase(std::remove_if(voices.begin(), voices.end(), [&](const Voice& vc) { return vc.t1 <= t0 + blk; }), voices.end());
  for (int i = 0; i < frames; i++) {
    float in = RV[i] * 0.06f, rv[2];
    for (int c = 0; c < 2; c++) { float a = 0; for (auto& cb : comb[c]) a += cb.run(in); for (auto& q : ap[c]) a = q.run(a); rv[c] = a * 0.35f; }
    float l = (L[i] * 0.9f + M[i * 2] * mvol + rv[0]) * master, r = (R[i] * 0.9f + M[i * 2 + 1] * mvol + rv[1]) * master;
    // compressor: threshold -16 dB, ratio 3
    float pk = std::max(std::fabs(l), std::fabs(r)); compEnv = pk > compEnv ? compEnv + (pk - compEnv) * 0.01f : compEnv + (pk - compEnv) * 0.0002f;
    float th = 0.158f, g = compEnv > th ? std::pow(compEnv / th, 1.f / 3 - 1) : 1; out[i * 2] = l * g; out[i * 2 + 1] = r * g;
  }
  rendered += frames;
}
}  // namespace pb
