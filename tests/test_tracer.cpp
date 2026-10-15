#include "doctest.h"
#include "tracer.h"

#include <cmath>
#include <initializer_list>

// critical impact parameter, 3 sqrt(3) M with M = 1
static const double b_crit = 3.0 * std::sqrt(3.0);

// a ray coming in from far down the -x axis, offset sideways by b, heading +x
static Ray incoming(double b, double start = 1e4) {
  return Ray{{-start, b, 0.0}, {1.0, 0.0, 0.0}};
}

TEST_CASE("rays just inside the critical impact parameter fall in") {
  CHECK(trace(incoming(b_crit * (1.0 - 1e-6))).fate == Fate::captured);
  CHECK(trace(incoming(b_crit * 0.5)).fate == Fate::captured);
}

TEST_CASE("rays just outside the critical impact parameter escape") {
  CHECK(trace(incoming(b_crit * (1.0 + 1e-6))).fate == Fate::escaped);
  CHECK(trace(incoming(b_crit * 2.0)).fate == Fate::escaped);
}

TEST_CASE("closest approach matches b = r0 / sqrt(1 - 2M / r0)") {
  // pick the closest approach, work out the b that should give it, then trace
  for (double r0 : {3.5, 4.0, 6.0, 10.0, 50.0}) {
    double b = r0 / std::sqrt(1.0 - 2.0 / r0);
    TraceResult res = trace(incoming(b));
    CHECK(res.fate == Fate::escaped);
    // min_r is only sampled at step points, which can be off by up to about 1.25e-5 relative
    CHECK(res.min_r == doctest::Approx(r0).epsilon(2e-5));
  }
}

// how far a ray's final direction turned away from +x
static double bend_angle(const TraceResult& res) { return std::atan2(-res.dir.y, res.dir.x); }

TEST_CASE("far away rays bend by about 4M / b") {
  // start and finish 1e7 out so the bending left outside the traced part is tiny (about b^3 / R^4)
  for (double b : {1000.0, 300.0}) {
    TraceResult res = trace(incoming(b, 1e7), 1e7);
    REQUIRE(res.fate == Fate::escaped);
    // first order, the classic result (twice what newton predicts). only close enough at b = 1000,
    // at b = 300 the higher order terms already add about 1%
    if (b == 1000.0) CHECK(bend_angle(res) == doctest::Approx(4.0 / b).epsilon(0.01).scale(0));
    // weak field series up to fourth order (keeton and petters 2005),
    // what's left over is the fifth order term, about 2e-8 relative at b = 300
    double u = 1.0 / b;
    double series = 4.0 * u + 15.0 * M_PI / 4.0 * u * u + 128.0 / 3.0 * u * u * u +
                    3465.0 * M_PI / 64.0 * u * u * u * u;
    // scale(0) makes the tolerance purely relative, angles this small would sneak past otherwise
    CHECK(bend_angle(res) == doctest::Approx(series).epsilon(1e-7).scale(0));
  }
}

// launches a ray sideways at r = 3M with exactly the critical b, so it should circle,
// then steps it n times per orbit and returns how far r has drifted from 3 after each orbit
static void photon_sphere_drift(int steps_per_orbit, double drift[2]) {
  // b = 3 sqrt(3) at r = 3 means a sideways speed of sqrt(3), and one orbit takes 2 pi sqrt(3)
  Ray ray{{3.0, 0.0, 0.0}, {0.0, std::sqrt(3.0), 0.0}};
  double dl = 2.0 * M_PI * std::sqrt(3.0) / steps_per_orbit;
  for (int orbit = 0; orbit < 2; ++orbit) {
    for (int i = 0; i < steps_per_orbit; ++i) ray = rk4_step(ray, dl);
    drift[orbit] = length(ray.pos) - 3.0;
  }
}

TEST_CASE("a ray launched sideways at 3M circles on the photon sphere") {
  double drift[2];
  photon_sphere_drift(360, drift);
  CHECK(std::fabs(drift[0]) < 1e-5);
}

TEST_CASE("the photon sphere orbit is unstable, errors grow by e^(2 pi) per orbit") {
  // linearizing u'' + u = 3M u^2 around u = 1/3M gives growth of e^phi
  double drift[2];
  photon_sphere_drift(360, drift);
  CHECK(drift[1] / drift[0] == doctest::Approx(std::exp(2.0 * M_PI)).epsilon(0.02));
}

TEST_CASE("rk4 error shrinks 16x when the step is halved") {
  // fourth order method, so half the step means 2^4 less error
  double coarse[2], fine[2];
  photon_sphere_drift(360, coarse);
  photon_sphere_drift(720, fine);
  CHECK(coarse[0] / fine[0] == doctest::Approx(16.0).epsilon(0.05));
}
