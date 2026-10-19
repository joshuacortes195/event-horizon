#include "doctest.h"
#include "kerr.h"
#include "render.h"

#include <cmath>
#include <initializer_list>

TEST_CASE("the horizon and the isco shrink with spin, from 2M and 6M down to 1M") {
  CHECK(horizon_r(0.0) == doctest::Approx(2.0));
  CHECK(horizon_r(0.6) == doctest::Approx(1.8));
  CHECK(horizon_r(1.0) == doctest::Approx(1.0));
  CHECK(isco_r(0.0) == doctest::Approx(6.0));
  CHECK(isco_r(1.0) == doctest::Approx(1.0));
  // values worked out separately from the same paper's formula
  CHECK(horizon_r(0.9) == doctest::Approx(1.4358899));
  CHECK(isco_r(0.5) == doctest::Approx(4.2330025));
  CHECK(isco_r(0.9) == doctest::Approx(2.3208830));
}

// energy per unit mass of gas on a circular orbit at r, spinning the same way as the hole
// (bardeen, press and teukolsky 1972)
static double orbit_energy(double r, double a) {
  double s = std::sqrt(r);
  return (r * s - 2.0 * s + a) / (std::pow(r, 0.75) * std::sqrt(r * s - 3.0 * s + 2.0 * a));
}

TEST_CASE("circular orbits need the least energy right at the isco") {
  // that's what makes it the innermost stable one: any closer and orbits get less tightly bound
  for (double a : {0.3, 0.6, 0.9}) {
    double r = isco_r(a);
    CHECK(orbit_energy(r * 1.001, a) > orbit_energy(r, a));
    CHECK(orbit_energy(r * 0.999, a) > orbit_energy(r, a));
  }
}

TEST_CASE("kerr-schild r is the plain distance on the spin axis and sqrt(R^2 - a^2) in the disk plane") {
  CHECK(kerr_radius({0.0, 7.0, 0.0}, 0.9) == doctest::Approx(7.0));
  CHECK(kerr_radius({3.0, 0.0, 4.0}, 0.9) == doctest::Approx(std::sqrt(25.0 - 0.81)));
  CHECK(kerr_radius({3.0, 2.0, 4.0}, 0.0) == doctest::Approx(std::sqrt(29.0)));
  // anywhere else it solves (x^2 + z^2) / (r^2 + a^2) + y^2 / r^2 = 1
  Vec3 p{1.5, -2.5, 0.7};
  double r = kerr_radius(p, 0.7);
  CHECK((p.x * p.x + p.z * p.z) / (r * r + 0.49) + p.y * p.y / (r * r) == doctest::Approx(1.0));
}

TEST_CASE("the hand derived ray equations match the slopes of the hamiltonian") {
  // dpos = dH/dp and dp = -dH/dpos, checked with central differences at a few spots
  const double a = 0.9, h = 1e-6;
  for (KerrRay ray : {KerrRay{{3.0, 1.0, -2.0}, {0.3, -0.5, 0.8}}, KerrRay{{-1.2, 0.4, 1.9}, {-0.7, 0.2, 0.1}},
                      KerrRay{{6.0, -5.0, 0.5}, {0.1, 0.9, -0.4}}}) {
    KerrDeriv d = kerr_derivs(ray, a);
    double want_dpos[3], want_dp[3];
    for (int i = 0; i < 3; ++i) {
      KerrRay hi = ray, lo = ray;
      (&hi.p.x)[i] += h;
      (&lo.p.x)[i] -= h;
      want_dpos[i] = (kerr_hamiltonian(hi, a) - kerr_hamiltonian(lo, a)) / (2.0 * h);
      hi = ray, lo = ray;
      (&hi.pos.x)[i] += h;
      (&lo.pos.x)[i] -= h;
      want_dp[i] = -(kerr_hamiltonian(hi, a) - kerr_hamiltonian(lo, a)) / (2.0 * h);
    }
    for (int i = 0; i < 3; ++i) {
      CHECK((&d.dpos.x)[i] == doctest::Approx(want_dpos[i]).epsilon(1e-6));
      CHECK((&d.dp.x)[i] == doctest::Approx(want_dp[i]).epsilon(1e-6));
    }
  }
}

TEST_CASE("with no spin the kerr tracer gives the same rays as the schwarzschild one") {
  Camera cam{15.0, 1.2, 0.4};
  for (double sx : {-0.9, -0.35, 0.0, 0.5, 0.8}) {
    for (double sy : {-0.4, 0.1, 0.45}) {
      Ray old_ray = camera_ray(cam, sx, sy);
      KerrRay ray = kerr_camera_ray(cam, 0.0, sx, sy);
      // same start and the same velocity, energy 1 in both
      Vec3 vel = kerr_derivs(ray, 0.0).dpos;
      CHECK(length(ray.pos - old_ray.pos) < 1e-12);
      CHECK(length(vel - old_ray.vel) < 1e-12);
      // and they end up the same way. the step sizes match outside 3M, inside it the kerr tracer steps
      // smaller, but nothing in there gets back out, so only the fate of a falling ray matters
      TraceResult a = trace(old_ray, 1000.0, 20.0), b = kerr_trace(ray, 0.0, 1000.0, 20.0);
      CHECK(a.fate == b.fate);
      if (a.fate != Fate::captured) CHECK(length(a.dir - b.dir) < 1e-6);
      CHECK(b.disk_r == doctest::Approx(a.disk_r).epsilon(1e-6));
    }
  }
}

// steps a ray the way kerr_trace does until it's far away or falls in, and reports how far H and L drifted
static void drift(KerrRay ray, double a, double* worst_H, double* worst_L, double* min_r) {
  double L0 = kerr_L(ray);
  *worst_H = *worst_L = 0.0;
  *min_r = 1e9;
  for (int i = 0; i < 100000; ++i) {
    double r = kerr_radius(ray.pos, a);
    if (r > 200.0 || r < horizon_r(a)) break;
    *min_r = std::fmin(*min_r, r);
    ray = kerr_step(ray, a, kerr_step_size(r, a));
    *worst_H = std::fmax(*worst_H, std::fabs(kerr_hamiltonian(ray, a)));
    *worst_L = std::fmax(*worst_L, std::fabs(kerr_L(ray) - L0));
  }
}

TEST_CASE("light, energy and angular momentum stay put along rays that whip around the hole") {
  // rays just outside the shadow's flattened edge, so they circle close in before getting out,
  // at a = 0.9 and at the top of the slider
  for (double a : {0.9, 0.99}) {
    Camera cam{15.0, 1.0};
    // find the edge 0.02 above the middle row, then go 1e-4 of the way further out
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 40; ++i) {
      double mid = 0.5 * (lo + hi);
      if (kerr_trace(kerr_camera_ray(cam, a, -mid, 0.02), a).fate == Fate::captured) lo = mid;
      else hi = mid;
    }
    KerrRay ray = kerr_camera_ray(cam, a, -hi * 1.0001, 0.02);
    REQUIRE(kerr_trace(ray, a).fate == Fate::escaped);
    double worst_H, worst_L, min_r;
    drift(ray, a, &worst_H, &worst_L, &min_r);
    MESSAGE("a = ", a, ": closest approach ", min_r, ", worst |H| ", worst_H, ", worst L drift ", worst_L);
    // it really went in close, right by the prograde light orbit (1.558M at 0.9, 1.168M at 0.99)
    CHECK(min_r < 1.01 * 2.0 * (1.0 + std::cos(2.0 / 3.0 * std::acos(-a))));
    // drift this small is a quarter of a pixel's angle at the narrowest view (2e-4 rad) or less
    CHECK(worst_H < 5e-5);
    CHECK(worst_L < 5e-5);
  }
}

TEST_CASE("steps shrink near the horizon but never to nothing") {
  // 1% of r like the non spinning tracer outside 1.5 times the horizon
  CHECK(kerr_step_size(4.0, 0.0) == doctest::Approx(0.04));
  CHECK(kerr_step_size(3.0, 0.99) == doctest::Approx(0.03));
  // the gap to the horizon sets it closer in, at a = 0.99 the prograde light orbit is 1.168M
  CHECK(kerr_step_size(1.168, 0.99) < 0.001);
  // and a ray right on the horizon still moves, so it falls through instead of creeping forever
  CHECK(kerr_step_size(horizon_r(0.99) + 1e-9, 0.99) >= 1e-4);
}

TEST_CASE("every pixel of a tilted camera launches light") {
  // H = 0 for each pixel only if the camera's frame is really orthonormal in the curved metric
  const double a = 0.9;
  Camera cam{8.0, 1.6, 0.6};
  for (double sx : {-1.0, -0.4, 0.3, 0.9})
    for (double sy : {-0.5, 0.0, 0.2, 0.55}) CHECK(std::fabs(kerr_hamiltonian(kerr_camera_ray(cam, a, sx, sy), a)) < 1e-12);
}

// screen spots from the center out along (dx, dy) where rays stop falling in, as an angle from
// straight ahead, plus that edge ray's angular momentum
static double kerr_edge(const Camera& cam, double a, double dx, double dy, double* L = nullptr) {
  double lo = 0.0, hi = 1.0;  // lo always captured, hi always escaped
  for (int i = 0; i < 40; ++i) {
    double mid = 0.5 * (lo + hi);
    if (kerr_trace(kerr_camera_ray(cam, a, mid * dx, mid * dy), a).fate == Fate::captured) lo = mid;
    else hi = mid;
  }
  double s = 0.5 * (lo + hi);
  if (L) *L = kerr_L(kerr_camera_ray(cam, a, s * dx, s * dy));
  return std::atan(s * std::tan(cam.fov / 2.0));
}

// critical impact parameter of light orbiting in the disk plane, with the spin (prograde) or
// against it (retrograde). 3 sqrt(3) for both without spin, 2 and 7 at a = 1
static double b_prograde(double a) { return 6.0 * std::cos(std::acos(-a) / 3.0) - a; }
static double b_retrograde(double a) { return 6.0 * std::cos(std::acos(a) / 3.0) + a; }

TEST_CASE("in the disk plane the shadow's edges sit at the prograde and retrograde critical rays") {
  CHECK(b_prograde(0.0) == doctest::Approx(3.0 * std::sqrt(3.0)));
  CHECK(b_prograde(1.0) == doctest::Approx(2.0));
  CHECK(b_retrograde(1.0) == doctest::Approx(7.0));
  // and against bardeen's xi worked out at each photon orbit radius
  CHECK(b_prograde(0.9) == doctest::Approx(2.8444214));
  CHECK(b_retrograde(0.9) == doctest::Approx(6.8323192));
  for (double a : {0.5, 0.9}) {
    Camera cam{15.0, 1.6};
    double L_left, L_right;
    kerr_edge(cam, a, -1.0, 0.0, &L_left);
    kerr_edge(cam, a, 1.0, 0.0, &L_right);
    // the left edge is light that came around with the spin, so it can pass closer before it's caught.
    // the traced ray runs backward, so its angular momentum has the opposite sign of the real light's
    CHECK(L_left < 0.0);
    CHECK(-L_left == doctest::Approx(b_prograde(a)).epsilon(1e-5));
    CHECK(L_right == doctest::Approx(b_retrograde(a)).epsilon(1e-5));
  }
}

// where a static observer in the disk plane at r sees light with energy 1 and angular momentum b:
// its angle from straight at the hole, from the boyer-lindquist metric in the plane
static double static_angle(double r, double a, double b) {
  double gtt = -(1.0 - 2.0 / r), gtp = -2.0 * a / r, gpp = r * r + a * a + 2.0 * a * a / r;
  return std::asin(std::fabs(b + gtp / gtt) * std::sqrt(-gtt) / std::sqrt(gpp - gtp * gtp / gtt));
}

TEST_CASE("a static observer sees the lopsided shadow's edges where the metric says") {
  for (double a : {0.5, 0.9}) {
    for (double r : {6.0, 15.0, 40.0}) {
      Camera cam{r, 2.0 * static_angle(r, a, -b_retrograde(a)) * 1.3};
      // the real prograde light has angular momentum +b, the retrograde light -b
      CHECK(kerr_edge(cam, a, -1.0, 0.0) == doctest::Approx(static_angle(r, a, b_prograde(a))).epsilon(1e-5));
      CHECK(kerr_edge(cam, a, 1.0, 0.0) == doctest::Approx(static_angle(r, a, -b_retrograde(a))).epsilon(1e-5));
    }
  }
}

TEST_CASE("a camera above the disk plane looks straight down the radius at the middle of the screen") {
  // the middle pixel's light has no push in the theta direction, p_theta = p . (d pos / d theta)
  const double a = 0.9, r = 12.0;
  for (double elev : {0.3, 0.8, 1.2}) {
    KerrRay ray = kerr_camera_ray({r, 1.0, elev}, a, 0.0, 0.0);
    Vec3 dpos_dtheta{0.0, -r * std::cos(elev), std::sqrt(r * r + a * a) * std::sin(elev)};
    CHECK(std::fabs(dot(ray.p, dpos_dtheta)) < 1e-12);
  }
}

// how far a ray coming in from far away at offset x in the disk plane gets turned, toward the hole
static double kerr_bend(double x, double a) {
  // straight along +z, momentum scaled so it's light, from (kappa^2 - 1) = f (1 + l_z kappa)^2
  const double R = 1e7;
  Vec3 pos{x, 0.0, -R};
  // f and l_z here, in the disk plane f is just 2 / r. l uses -a like the tracer
  double r = kerr_radius(pos, a), f = 2.0 / r, lz = (r * pos.z - a * x) / (r * r + a * a);
  double A = 1.0 - f * lz * lz, B = -2.0 * f * lz, C = -1.0 - f;
  double kappa = (-B + std::sqrt(B * B - 4.0 * A * C)) / (2.0 * A);
  KerrRay ray{pos, {0.0, 0.0, kappa}};
  CHECK(std::fabs(kerr_hamiltonian(ray, a)) < 1e-15);
  TraceResult res = kerr_trace(ray, a, R);
  REQUIRE(res.fate == Fate::escaped);
  return std::atan2(x > 0.0 ? -res.dir.x : res.dir.x, res.dir.z);
}

TEST_CASE("far from the hole, light going around with the spin bends a little less") {
  // weak field series (sereno and de luca 2006): the spin part is -4a / b^2 - 10 pi a / b^3 for prograde
  // light and the opposite for retrograde, so the two differ by 8a / b^2 + 20 pi a / b^3.
  // x > 0 here is prograde for the real light, since the traced ray runs backward
  const double a = 0.9, b = 300.0;
  double diff = kerr_bend(b, a) - kerr_bend(-b, a);
  CHECK(diff == doctest::Approx(-8.0 * a / (b * b) - 20.0 * M_PI * a / (b * b * b)).epsilon(0.01).scale(0));
}

TEST_CASE("a spinning hole lets the disk reach in past 6M") {
  // looking straight down from above: going out from the center, the first disk hit is the isco.
  // at a = 0.9 the plain distance there is 7% more than r, so measuring the wrong one shows
  const double a = 0.9;
  Camera cam{30.0, 0.8, M_PI / 2.0};
  double first = 0.0;
  KerrRay ray;
  for (int i = 1; i <= 2000 && first == 0.0; ++i) {
    ray = kerr_camera_ray(cam, a, i / 2000.0, 0.0);
    TraceResult res = kerr_trace(ray, a, 1000.0, 20.0);
    if (res.fate == Fate::disk) first = res.disk_r;
  }
  // step that ray to the disk plane myself: the hit is the kerr-schild r of the crossing point,
  // not its plain distance
  while (true) {
    KerrRay next = kerr_step(ray, a, kerr_step_size(kerr_radius(ray.pos, a), a));
    if (next.pos.y <= 0.0) {
      Vec3 hit = ray.pos + (ray.pos.y / (ray.pos.y - next.pos.y)) * (next.pos - ray.pos);
      CHECK(first == doctest::Approx(kerr_radius(hit, a)).epsilon(1e-12));
      break;
    }
    ray = next;
  }
  CHECK(first > isco_r(a));
  CHECK(first < isco_r(a) * 1.02);
  CHECK(first < 6.0);
}

TEST_CASE("the disk temperature profile starts at whatever isco it's given") {
  CHECK(disk_temperature(isco_r(0.6), 5000.0, isco_r(0.6)) == doctest::Approx(0.0));
  // hottest ring still at 49/36 of the inner edge
  double r_peak = 49.0 / 36.0 * isco_r(0.6);
  CHECK(disk_temperature(r_peak, 5000.0, isco_r(0.6)) == doctest::Approx(5000.0));
  CHECK(disk_temperature(r_peak * 1.01, 5000.0, isco_r(0.6)) < 5000.0);
  CHECK(disk_temperature(r_peak * 0.99, 5000.0, isco_r(0.6)) < 5000.0);
}

TEST_CASE("seen from far away face on, orbiting gas is redshifted by 1 / u^t, straight from the metric") {
  // u^t from normalizing the orbit's 4-velocity in the boyer-lindquist metric, with omega = 1 / (r^1.5 + a)
  for (double a : {0.3, 0.9}) {
    for (double r : {isco_r(a) * 1.1, 8.0, 20.0}) {
      double omega = 1.0 / (r * std::sqrt(r) + a);
      double gtt = -(1.0 - 2.0 / r), gtp = -2.0 * a / r, gpp = r * r + a * a + 2.0 * a * a / r;
      double ut = 1.0 / std::sqrt(-(gtt + 2.0 * omega * gtp + omega * omega * gpp));
      CHECK(disk_g(r, 0.0, 1e12, true, true, a) == doctest::Approx(1.0 / ut));
    }
  }
  // the gas's orbital speed, read back out of g: it has to be a circular orbit of the metric,
  // d/dr of g_tt + 2 omega g_tphi + omega^2 g_phiphi = 0 in the boyer-lindquist plane
  for (double r : {3.0, 8.0}) {
    double a = 0.9, g0 = disk_g(r, 0.0, 1e12, true, true, a), g1 = disk_g(r, 1.0, 1e12, true, true, a);
    double omega = g0 / g1 - 1.0;
    double d_gtt = -2.0 / (r * r), d_gtp = 2.0 * a / (r * r), d_gpp = 2.0 * r - 2.0 * a * a / (r * r);
    CHECK(d_gtt + 2.0 * omega * d_gtp + omega * omega * d_gpp == doctest::Approx(0.0).scale(1e-3));
  }
  // gravity part only: a zamo's clock rate, alpha^2 = -g_tt + g_tphi^2 / g_phiphi in the plane
  for (double r : {3.0, 8.0}) {
    double a = 0.9, gtt = -(1.0 - 2.0 / r), gtp = -2.0 * a / r, gpp = r * r + a * a + 2.0 * a * a / r;
    CHECK(disk_g(r, 0.0, 1e12, true, false, a) == doctest::Approx(std::sqrt(-gtt + gtp * gtp / gpp)));
  }
  // the camera's own clock rate: -g_tt = 1 - 2r / (r^2 + a^2 cos^2 theta), theta measured from the spin axis
  {
    double a = 0.9, r_obs = 8.0, elev = 1.0, cth = std::cos(M_PI / 2.0 - elev);
    double cam = std::sqrt(1.0 - 2.0 * r_obs / (r_obs * r_obs + a * a * cth * cth));
    CHECK(disk_g(8.0, 0.0, r_obs, true, false, a, elev) * cam == doctest::Approx(disk_g(8.0, 0.0, 1e300, true, false, a)));
  }
  // and the spin terms vanish without spin
  CHECK(disk_g(8.0, 3.0, 15.0, true, true, 0.0, 0.7) == doctest::Approx(disk_g(8.0, 3.0, 15.0, true, true)));
}

TEST_CASE("a barely spinning hole renders the same picture as a non spinning one") {
  Camera cam{30.0, 0.9, 0.15};
  Disk disk{20.0, 6000.0};
  Image old_img = render(cam, 120, 80, disk), new_img = render(cam, 120, 80, disk, 1e-9);
  int differ = 0;
  for (size_t i = 0; i < old_img.rgb.size(); ++i) differ += std::abs(old_img.rgb[i] - new_img.rgb[i]) > 1;
  MESSAGE(differ, " of ", old_img.rgb.size(), " channels differ by more than 1");
  CHECK(differ <= 10);
}
