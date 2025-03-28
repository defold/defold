// Defold modification, not included in original Box2D

#ifndef B2_POW_H
#define B2_POW_H

#include <stdint.h>

inline float b2FastLog2(float x) {
    union { float f; uint32_t i; } vx = { x };
    union { uint32_t i; float f; } mx = { (vx.i & 0x007FFFFF) | 0x3f000000 };
    float y = vx.i;
    y *= 1.1920928955078125e-7f;

    return y - 124.22551499f
           - 1.498030302f * mx.f
           - 1.72587999f / (0.3520887068f + mx.f);
}

inline float b2FastPow2(float p) {
    float offset = (p < 0) ? 1.0f : 0.0f;
    float clipp = (p < -126) ? -126.0f : p;
    int w = clipp;
    float z = clipp - w + offset;
    union {
        uint32_t i;
        float f;
    } v = { (uint32_t)( (1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z) ) };

    return v.f;
}

inline float b2FastPow(float x, float p) {
  return b2FastPow2(p * b2FastLog2(x));
}

#endif // B2_POW_H
