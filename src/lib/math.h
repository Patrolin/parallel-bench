#pragma once
#include "builtin.h"

// TODO: better math functions that don't error...
#define sin(x) _Generic((x), \
  f32: __builtin_sinf(x),    \
  f64: __builtin_sin(x))
#define cos(x) _Generic((x), \
  f32: __builtin_cosf(x),    \
  f64: __builtin_cos(x))
#define sqrt(x) (assume((x) >= 0), _Generic((x), f32: __builtin_sqrtf(x), f64: __builtin_sqrt(x)))
#define cbrt(x) (assume((x) >= 0), _Generic((x), f32: __builtin_cbrtf(x), f64: __builtin_cbrt(x)))

// f32 vector
STRUCT(f32v2) {
  union {
    // vector
    struct {
      f32 x, y;
    };
    // rotor
    struct {
      f32 sc, xy;
    };
  };
};
#define f32v2(x, y) \
  (f32v2) { x, y }
f32v2 add(f32v2 a, f32v2 b) {
  return f32v2(a.x + b.x, a.y + b.y);
}
f32v2 sub(f32v2 a, f32v2 b) {
  return f32v2(a.x - b.x, a.y - b.y);
}
f32v2 mul(f32v2 a, f32v2 b) {
  return f32v2(a.x * b.x, a.y * b.y);
}
f32v2 div(f32v2 a, f32v2 b) {
  return f32v2(a.x / b.x, a.y / b.y);
}
f32 norm_squared(f32v2 a) {
  return a.x * a.x + a.y * a.y;
}
f32 norm(f32v2 a) {
  return sqrt(a.x * a.x + a.y * a.y);
}
f32v2 normalized(f32v2 a) {
  f32 a_norm = 1 / norm(a);
  return f32v2(a.x * a_norm, a.y * a_norm);
}
// f32 normalized rotor
STRUCT(f32r2) {
  f32 xy;
};
f32v2 rotor_expand(f32r2 r) {
  return f32v2(sqrt(1 - r.xy * r.xy), r.xy);
}
f32r2 rotor_from_angle(f32 angle) {
  return (f32r2){sin(angle)};
}
f32r2 rotor_from_vectors(f32v2 a, f32v2 b) {
  f32v2 R = (f32v2){
    a.x * b.x + a.y * b.y,
    a.x * b.y - a.y * b.x,
  };
  return (f32r2){R.xy / norm(R)};
}
f32r2 rotor_nlerp(f32 t, f32r2 ra, f32r2 rb) {
  f32v2 Ra = rotor_expand(ra);
  f32v2 Rb = rotor_expand(rb);
  f32v2 R = (f32v2){
    (1 - t) * Ra.sc + t * Rb.sc,
    (1 - t) * Ra.xy + t * Rb.xy,
  };
  return (f32r2){R.xy / norm(R)};
}
f32v2 rotate(f32v2 a, f32r2 r) {
  f32v2 R = rotor_expand(r);
  return (f32v2){
    a.x * R.sc - a.y * R.xy,
    a.x * R.xy + a.y * R.sc,
  };
}
