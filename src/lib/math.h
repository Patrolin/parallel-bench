#pragma once
#include "builtin.h"

// TODO: better math functions that don't error...
#define sin(x) _Generic((x), \
  f32: __builtin_sinf(x),    \
  f64: __builtin_sin(x))
#define cos(x) _Generic((x), \
  f32: __builtin_cosf(x),    \
  f64: __builtin_cos(x))
#define sqrt(x)  (assume((x) >= 0), _Generic((x), f32: __builtin_sqrtf(x), f64: __builtin_sqrt(x)))
#define rsqrt(x) (1 / sqrt(x))
#define cbrt(x)  (assume((x) >= 0), _Generic((x), f32: __builtin_cbrtf(x), f64: __builtin_cbrt(x)))

// f32 vector
STRUCT(f32v2) {
  union {
    struct {
      f32 x, y;
    };
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
  f32 a_norm = norm(a);
  return f32v2(a.x / a_norm, a.y / a_norm);
}
// f32 rotor (with half angles like in quaternions for double cover)
f32v2 rotor_from_angle(f32 radians) {
  return (f32v2){cos(radians * 0.5f), sin(radians * 0.5f)};
}
f32v2 rotor_from_vectors(f32v2 a, f32v2 b) {
  f32 cosine = a.x * b.x + a.y * b.y;
  f32 half_cosine = sqrt(cosine * 0.5f + 0.5f);
  f32 half_sine = sqrt(cosine * -0.5f + 0.5f);
  return normalized(f32v2(half_cosine, half_sine));
}
f32v2 rotor_nlerp(f32 t, f32v2 Ra, f32v2 Rb) {
  f32v2 R = (f32v2){
    (1 - t) * Ra.sc + t * Rb.sc,
    (1 - t) * Ra.xy + t * Rb.xy,
  };
  return normalized(R);
}
/* NOTE: `R*a*reverse(R)` */
f32v2 rotate(f32v2 a, f32v2 R) {
  f32v2 middle = (f32v2){
    R.sc * a.x + R.xy * a.y,
    R.sc * a.y - R.xy * a.x,
  };
  return (f32v2){
    middle.x * R.sc + middle.y * R.xy,
    middle.y * R.sc - middle.x * R.xy,
  };
}
