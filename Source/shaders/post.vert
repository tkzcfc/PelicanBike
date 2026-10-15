#version 310 es
// fullscreen triangle: a_position = (-1,-1) (3,-1) (-1,3)
precision highp float;
layout(location = 0) in vec2 a_position;
layout(location = 0) out vec2 vUv;
void main() { vUv = a_position * 0.5 + 0.5; gl_Position = vec4(a_position, 0.0, 1.0);
#ifdef METAL
  vUv.y = 1.0 - vUv.y;  // Metal render targets are stored top-down
#endif
}
