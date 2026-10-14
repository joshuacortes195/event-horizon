#include "tracer.h"

#include <algorithm>
#include <cmath>

double length(Vec3 a) { return std::sqrt(dot(a, a)); }

Vec3 normalize(Vec3 a) { return (1.0 / length(a)) * a; }

// the pull of gravity on light, -3M h^2 x / r^5 with M = 1.
// h is the ray's angular momentum, which stays the same along the whole path
static Vec3 accel(Vec3 pos, Vec3 vel) {
  Vec3 h = cross(pos, vel);
  double r2 = dot(pos, pos);
  double r5 = r2 * r2 * std::sqrt(r2);
  return (-3.0 * dot(h, h) / r5) * pos;
}

// rk4 samples the pull four times across the step and blends them,
// way more accurate than just doing pos += vel * dl
Ray rk4_step(const Ray& ray, double dl) {
  Vec3 p1 = ray.pos, v1 = ray.vel, a1 = accel(p1, v1);
  Vec3 p2 = p1 + (0.5 * dl) * v1, v2 = v1 + (0.5 * dl) * a1, a2 = accel(p2, v2);
  Vec3 p3 = p1 + (0.5 * dl) * v2, v3 = v1 + (0.5 * dl) * a2, a3 = accel(p3, v3);
  Vec3 p4 = p1 + dl * v3, v4 = v1 + dl * a3, a4 = accel(p4, v4);
  return {p1 + (dl / 6.0) * (v1 + 2.0 * v2 + 2.0 * v3 + v4),
          v1 + (dl / 6.0) * (a1 + 2.0 * a2 + 2.0 * a3 + a4)};
}

TraceResult trace(Ray ray, double escape_r) {
  // step size grows with distance, so far away rays don't waste steps
  const double step_frac = 0.01;
  // safety net so a ray balanced on the photon sphere can't loop forever
  const int max_steps = 100000;

  double min_r = length(ray.pos);
  for (int i = 0; i < max_steps; ++i) {
    double r = length(ray.pos);
    min_r = std::min(min_r, r);
    // inside the horizon, the ray is gone
    if (r < 2.0) return {Fate::captured, min_r, normalize(ray.vel)};
    // far away and heading out, nothing left to bend it
    if (r > escape_r && dot(ray.pos, ray.vel) > 0.0)
      return {Fate::escaped, min_r, normalize(ray.vel)};
    ray = rk4_step(ray, step_frac * r);
  }
  return {Fate::captured, min_r, normalize(ray.vel)};
}
