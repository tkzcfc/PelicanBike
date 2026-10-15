// HDR (RGBA32F) sampling. ax::backend only offers RGBA32F as a float colour format, and linear filtering of 32-bit
// float textures is optional on some backends: WebGL2/GLES3 need OES_texture_float_linear, and most iOS GPUs lack it.
// When man == true the targets are created with NEAREST sampling and the bilinear filter runs here with 4 texelFetch.
vec3 pbTex(sampler2D s, vec2 uv, bool man) {
  if (!man) return texture(s, uv).rgb;
  ivec2 sz = textureSize(s, 0); ivec2 m = sz - 1;
  vec2 st = uv * vec2(sz) - 0.5; vec2 i0 = floor(st); vec2 f = st - i0; ivec2 c = ivec2(i0);
  vec3 a = texelFetch(s, clamp(c, ivec2(0), m), 0).rgb, b = texelFetch(s, clamp(c + ivec2(1, 0), ivec2(0), m), 0).rgb;
  vec3 d = texelFetch(s, clamp(c + ivec2(0, 1), ivec2(0), m), 0).rgb, e = texelFetch(s, clamp(c + ivec2(1, 1), ivec2(0), m), 0).rgb;
  return mix(mix(a, b, f.x), mix(d, e, f.x), f.y);
}
