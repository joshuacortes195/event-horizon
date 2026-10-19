#include "kerr.h"

#include <algorithm>
#include <cmath>

double horizon_r(double a) { return 1.0 + std::sqrt(1.0 - a * a); }

// innermost stable orbit for gas going with the spin, bardeen, press and teukolsky's formula
double isco_r(double a) {
  double z1 = 1.0 + std::cbrt(1.0 - a * a) * (std::cbrt(1.0 + a) + std::cbrt(1.0 - a));
  double z2 = std::sqrt(3.0 * a * a + z1 * z1);
  return 3.0 + z2 - std::sqrt((3.0 - z1) * (3.0 + z1 + 2.0 * z2));
}

double kerr_radius(Vec3 pos, double a) {
  // solves (x^2 + z^2) / (r^2 + a^2) + y^2 / r^2 = 1 for r, y being the spin axis
  double k = 0.5 * (dot(pos, pos) - a * a);
  return std::sqrt(k + std::sqrt(k * k + a * a * pos.y * pos.y));
}

// the metric is flat space plus f l l. the backward traced ray is really light around a hole
// spinning the other way (time runs backward, so the spin flips), so l uses -a
struct Field {
  double r, f;
  Vec3 l;
  Vec3 dr, df;  // slopes of r and f in x, y, z
};

static Field field(Vec3 pos, double a) {
  Field F;
  double a2 = a * a, y = pos.y;
  double k = 0.5 * (dot(pos, pos) - a2), d = std::sqrt(k * k + a2 * y * y);
  double r2 = k + d, r = std::sqrt(r2);
  F.r = r;
  // how r changes when x, y or z move, from differentiating the equation above
  F.dr = {r * pos.x / (2.0 * d), y * (r2 + a2) / (r * 2.0 * d), r * pos.z / (2.0 * d)};
  // how strongly space is curved here, 2r / (r^2 + a^2 cos^2 theta) in kerr-schild form
  double sig = r2 * r2 + a2 * y * y;
  F.f = 2.0 * r2 * r / sig;
  Vec3 dsig = (4.0 * r2 * r) * F.dr + Vec3{0.0, 2.0 * a2 * y, 0.0};
  F.df = F.f * ((3.0 / r) * F.dr - (1.0 / sig) * dsig);
  // the direction light falls in along, swirled around the spin axis by the spin
  double s = -a, q = r2 + a2;
  F.l = {(r * pos.x - s * pos.z) / q, y / r, (r * pos.z + s * pos.x) / q};
  return F;
}

double kerr_hamiltonian(const KerrRay& ray, double a) {
  // H = (flat space length of p, with energy 1) - f (l . p)^2, all halved. light has H = 0
  Field F = field(ray.pos, a);
  double w = 1.0 + dot(F.l, ray.p);
  return 0.5 * (dot(ray.p, ray.p) - 1.0 - F.f * w * w);
}

KerrDeriv kerr_derivs(const KerrRay& ray, double a) {
  Field F = field(ray.pos, a);
  Vec3 x = ray.pos, p = ray.p;
  double r = F.r, s = -a, q = r * r + a * a;
  double w = 1.0 + dot(F.l, p);
  // slopes of l . p in x, y, z. the swirled part first, then the y / r part
  double m = x.x * p.x + x.z * p.z, n = x.x * p.z - x.z * p.x, num = r * m + s * n;
  Vec3 dnum = m * F.dr + Vec3{r * p.x + s * p.z, 0.0, r * p.z - s * p.x};
  Vec3 dlp = (1.0 / q) * dnum - (2.0 * r * num / (q * q)) * F.dr;
  dlp = dlp + (p.y / (r * r)) * (Vec3{0.0, r, 0.0} - x.y * F.dr);
  // hamilton's equations: position follows dH/dp, momentum follows -dH/dpos
  return {p - (F.f * w) * F.l, (0.5 * w * w) * F.df + (F.f * w) * dlp};
}

// same rk4 as the non spinning tracer, just on position and momentum
KerrRay kerr_step(const KerrRay& ray, double a, double dl) {
  KerrDeriv k1 = kerr_derivs(ray, a);
  KerrDeriv k2 = kerr_derivs({ray.pos + (0.5 * dl) * k1.dpos, ray.p + (0.5 * dl) * k1.dp}, a);
  KerrDeriv k3 = kerr_derivs({ray.pos + (0.5 * dl) * k2.dpos, ray.p + (0.5 * dl) * k2.dp}, a);
  KerrDeriv k4 = kerr_derivs({ray.pos + dl * k3.dpos, ray.p + dl * k3.dp}, a);
  return {ray.pos + (dl / 6.0) * (k1.dpos + 2.0 * k2.dpos + 2.0 * k3.dpos + k4.dpos),
          ray.p + (dl / 6.0) * (k1.dp + 2.0 * k2.dp + 2.0 * k3.dp + k4.dp)};
}

// angular momentum about the y axis, the same formula as the non spinning tracer's cross(pos, vel).y
double kerr_L(const KerrRay& ray) { return ray.pos.z * ray.p.x - ray.pos.x * ray.p.z; }

// a 4-vector: time part and space part
struct V4 {
  double t;
  Vec3 s;
};

KerrRay kerr_launch(Vec3 pos, Vec3 right, Vec3 up, Vec3 forward, Vec3 dir, double a) {
  Field F = field(pos, a);
  // dot product of two 4-vectors in the curved metric
  auto g = [&](V4 A, V4 B) {
    return -A.t * B.t + dot(A.s, B.s) + F.f * (A.t + dot(F.l, A.s)) * (B.t + dot(F.l, B.s));
  };
  // A + c B, and A scaled to length 1
  auto add = [](V4 A, double c, V4 B) { return V4{A.t + c * B.t, A.s + c * B.s}; };
  auto unit = [&](V4 A) { return add({0.0, {0.0, 0.0, 0.0}}, 1.0 / std::sqrt(g(A, A)), A); };
  // the hovering observer's clock ticks along t only, slowed by the gravity here
  V4 u{1.0 / std::sqrt(1.0 - F.f), {0.0, 0.0, 0.0}};
  // its own right, forward and up: each seed with the parts along the earlier ones taken out
  V4 e_right = unit(add({0.0, right}, g({0.0, right}, u), u));
  V4 e_fwd = add({0.0, forward}, g({0.0, forward}, u), u);
  e_fwd = unit(add(e_fwd, -g(e_fwd, e_right), e_right));
  V4 e_up = add({0.0, up}, g({0.0, up}, u), u);
  e_up = unit(add(add(e_up, -g(e_up, e_right), e_right), -g(e_up, e_fwd), e_fwd));
  // light moving through the observer's time at speed 1 in direction dir
  V4 k = add(add(add(u, dir.x, e_right), dir.y, e_up), dir.z, e_fwd);
  // lower the index, then scale so the energy is exactly 1
  double lk = k.t + dot(F.l, k.s);
  double pt = -k.t + F.f * lk;
  return {pos, (-1.0 / pt) * (k.s + (F.f * lk) * F.l)};
}

double kerr_step_size(double r, double a) {
  // the same as before past 1.5 times the horizon (3M without spin). closer in it shrinks with the
  // gap to the horizon, at a = 0.99 the prograde light orbit is only 0.03M outside it.
  // never under 1e-4, or a falling ray would creep up on the horizon forever and never cross it
  return std::fmax(0.01 * std::fmin(r, 3.0 * (r - horizon_r(a))), 1e-4);
}

TraceResult kerr_trace(KerrRay ray, double a, double escape_r, double disk_outer) {
  // same safety net as the non spinning tracer
  const int max_steps = 100000;
  const double r_h = horizon_r(a), r_in = isco_r(a);

  double min_r = kerr_radius(ray.pos, a);
  for (int i = 0; i < max_steps; ++i) {
    double r = kerr_radius(ray.pos, a);
    min_r = std::min(min_r, r);
    Vec3 vel = kerr_derivs(ray, a).dpos;
    // inside the horizon, the ray is gone
    if (r < r_h) return {Fate::captured, min_r, normalize(vel)};
    // far away and heading out, nothing left to bend it
    if (r > escape_r && dot(ray.pos, vel) > 0.0) return {Fate::escaped, min_r, normalize(vel)};
    KerrRay next = kerr_step(ray, a, kerr_step_size(r, a));
    // crossed the disk plane this step, find where and check it's on the disk
    if (disk_outer > 0.0 && ray.pos.y != 0.0 && (ray.pos.y > 0.0) != (next.pos.y > 0.0)) {
      double t = ray.pos.y / (ray.pos.y - next.pos.y);
      double hit_r = kerr_radius(ray.pos + t * (next.pos - ray.pos), a);
      if (hit_r >= r_in && hit_r <= disk_outer)
        return {Fate::disk, min_r, normalize(kerr_derivs(next, a).dpos), hit_r};
    }
    ray = next;
  }
  return {Fate::captured, min_r, normalize(kerr_derivs(ray, a).dpos)};
}
