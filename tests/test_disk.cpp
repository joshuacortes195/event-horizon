#include "doctest.h"
#include "disk.h"

#include <cmath>
#include <initializer_list>

TEST_CASE("the disk is cold at the isco and hottest at 49/36 of it") {
  // no torque at the inner edge, so no heating there
  CHECK(disk_temperature(6.0, 5000.0) == doctest::Approx(0.0));
  // scan outward and find the hottest radius
  double best_r = 6.0, best_t = 0.0;
  for (double r = 6.0; r < 20.0; r += 1e-4) {
    double t = disk_temperature(r, 5000.0);
    if (t > best_t) best_t = t, best_r = r;
  }
  CHECK(best_r == doctest::Approx(49.0 / 36.0 * 6.0).epsilon(1e-4));
  CHECK(best_t == doctest::Approx(5000.0));
}

TEST_CASE("far out the disk cools as r^(-3/4)") {
  double ratio = disk_temperature(1e4, 5000.0) / disk_temperature(2e4, 5000.0);
  CHECK(ratio == doctest::Approx(std::pow(2.0, 0.75)).epsilon(0.005).scale(0));
}

// where a color sits on the cie 1931 chromaticity chart
static void chromaticity(Vec3 xyz, double& x, double& y) {
  double sum = xyz.x + xyz.y + xyz.z;
  x = xyz.x / sum;
  y = xyz.y / sum;
}

TEST_CASE("blackbody colors land on the planckian locus") {
  // (x, y) of a blackbody from the official cie 1931 tables
  struct Point { double temp, x, y; };
  for (Point p : {Point{3000, 0.4369, 0.4041}, Point{4500, 0.3608, 0.3635},
                  Point{6500, 0.3135, 0.3236}, Point{10000, 0.2806, 0.2883}}) {
    double x, y;
    chromaticity(blackbody_xyz(p.temp), x, y);
    CHECK(std::fabs(x - p.x) < 0.002);
    CHECK(std::fabs(y - p.y) < 0.002);
  }
  // the fitted color curves are off by about 0.011 this far into the red
  double x, y;
  chromaticity(blackbody_xyz(1000), x, y);
  CHECK(std::fabs(x - 0.6528) < 0.015);
  CHECK(std::fabs(y - 0.3445) < 0.015);
}

TEST_CASE("a hotter blackbody is brighter, a cold one is dark") {
  CHECK(blackbody_xyz(0.0).y == 0.0);
  CHECK(blackbody_xyz(6000.0).y > blackbody_xyz(3000.0).y);
}

TEST_CASE("face on from far away the disk is only redshifted by sqrt(1 - 3M/r)") {
  // a ray with no angular momentum about the spin axis sees no doppler push either way
  CHECK(disk_g(6.0, 0.0, 1e12, true, true) == doctest::Approx(0.7071068));
  CHECK(disk_g(12.0, 0.0, 1e12, true, true) == doctest::Approx(0.8660254));
  // gravity alone is sqrt(1 - 2M/r), climbing out of the well
  CHECK(disk_g(6.0, 0.0, 1e12, true, false) == doctest::Approx(0.8164966));
}

TEST_CASE("the side spinning toward us is blueshifted, the other side redshifted") {
  // the disk spins with positive angular momentum about +y, the traced ray runs backwards
  // so negative traced L means the light left heading the way the gas moves
  CHECK(disk_g(10.0, -8.0, 1e12, false, true) > 1.0);
  CHECK(disk_g(10.0, 8.0, 1e12, false, true) < 1.0);
}

TEST_CASE("the doppler shift matches special relativity for the orbiting gas") {
  // light sent straight along the gas's motion, seen by a static observer right there:
  // the gas moves at v = 1 / sqrt(r - 2) past it, so g = sqrt((1 + v) / (1 - v))
  for (double r : {6.0, 10.0}) {
    double v = 1.0 / std::sqrt(r - 2.0);
    double L = -r / std::sqrt(1.0 - 2.0 / r);  // biggest |L| a photon can have at r, heading forward
    CHECK(disk_g(r, L, 1e300, false, true) == doctest::Approx(std::sqrt((1.0 + v) / (1.0 - v))));
  }
}

TEST_CASE("a closer camera sees less gravitational redshift") {
  // the camera at 15M is partway down the well too: (sqrt(2/3)) / sqrt(13/15)
  CHECK(disk_g(6.0, 0.0, 15.0, true, false) == doctest::Approx(0.8770580));
}

TEST_CASE("with both effects off nothing is shifted") {
  CHECK(disk_g(6.0, -8.0, 15.0, false, false) == 1.0);
  CHECK(disk_g(20.0, 8.0, 15.0, false, false) == 1.0);
}

TEST_CASE("face on rays land on the disk at the radius their impact parameter says") {
  // ray from far up the +y axis heading straight down at the disk, b to the side
  auto from_above = [](double b) { return Ray{{b, 1e6, 0.0}, {0.0, -1.0, 0.0}}; };
  // b worked out from the bending integral for rings at 8.1667M and 10M
  struct Ring { double b, r; };
  for (Ring ring : {Ring{9.11432, 49.0 / 6.0}, Ring{10.95612, 10.0}}) {
    TraceResult res = trace(from_above(ring.b), 1000.0, 20.0);
    CHECK(res.fate == Fate::disk);
    CHECK(res.disk_r == doctest::Approx(ring.r).epsilon(2e-5));
  }
  // the 6M ring is at b = 6.93215, so a bit inside it falls through the hole in the middle
  // and a bit outside it lands on the inner edge
  CHECK(trace(from_above(6.88), 1000.0, 20.0).fate == Fate::escaped);
  TraceResult edge = trace(from_above(6.98), 1000.0, 20.0);
  CHECK(edge.fate == Fate::disk);
  CHECK(edge.disk_r > 6.0);
  CHECK(edge.disk_r < 6.1);
  // same ring at 10M, but the disk stops at 8M so the ray goes right past
  CHECK(trace(from_above(10.95612), 1000.0, 8.0).fate == Fate::escaped);
}

TEST_CASE("a ray that starts in the disk plane doesn't count as hitting the disk where it starts") {
  // camera sitting in the plane at 15M, which is inside the disk's span, looking up and out
  Ray up{{0.0, 0.0, 15.0}, {0.0, 0.1, 1.0}};
  TraceResult res = trace(up, 1000.0, 20.0);
  CHECK(res.fate == Fate::escaped);
}
