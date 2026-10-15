#version 310 es
// Shadow map: the backend has no depth-compare samplers / depth textures / polygon offset, so the depth is written
// into an RGB8 colour target (24-bit fixed point, same precision as the old DEPTH_COMPONENT24) with
// glPolygonOffset(1.5, 3) applied in the shader: z + 1.5 * max|dz/dx,dz/dy| + 3 * 2^-24.
precision highp float;
layout(location = 0) in float vDepth;
layout(location = 0) out vec4 oCol;
void main() {
  float m = max(abs(dFdx(vDepth)), abs(dFdy(vDepth)));
  float z = clamp(vDepth + 1.5 * m + 3.0 / 16777215.0, 0.0, 1.0);
  float q = floor(z * 16777215.0 + 0.5);
  float r = floor(q / 65536.0); q -= r * 65536.0; float g = floor(q / 256.0); float b = q - g * 256.0;
  oCol = vec4(r, g, b, 255.0) / 255.0;
}
