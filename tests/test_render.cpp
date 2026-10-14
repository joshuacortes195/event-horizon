#include "doctest.h"
#include "render.h"

#include <cmath>

// angular radius of the shadow for a static observer at r (synge 1966),
// measured from the direction pointing at the hole
static double shadow_angle(double r) {
  return std::asin(3.0 * std::sqrt(3.0) / r * std::sqrt(1.0 - 2.0 / r));
}

// walks out from the screen center along (dx, dy) and finds where rays stop falling in
static double edge_angle(const Camera& cam, double dx, double dy) {
  double lo = 0.0, hi = 1.0;  // lo always captured, hi always escaped
  for (int i = 0; i < 40; ++i) {
    double mid = 0.5 * (lo + hi);
    if (trace(camera_ray(cam, mid * dx, mid * dy)).fate == Fate::captured) lo = mid;
    else hi = mid;
  }
  // screen distance from center back to an angle from straight ahead
  return std::atan(0.5 * (lo + hi) * std::tan(cam.fov / 2.0));
}

TEST_CASE("the shadow edge seen by a static observer matches sin(alpha) = 3 sqrt(3) M / r * sqrt(1 - 2M / r)") {
  for (double r : {6.0, 10.0, 30.0}) {
    // field of view a bit wider than the shadow so the edge is on screen
    Camera cam{r, 2.0 * shadow_angle(r) * 1.5};
    CHECK(edge_angle(cam, 1.0, 0.0) == doctest::Approx(shadow_angle(r)).epsilon(1e-5).scale(0));
    CHECK(edge_angle(cam, 0.0, 1.0) == doctest::Approx(shadow_angle(r)).epsilon(1e-5).scale(0));
  }
}

// whether the pixel at column x, row y of an rgb image is pure black
static bool is_black(const Image& img, int x, int y) {
  const unsigned char* p = &img.rgb[3 * (y * img.width + x)];
  return p[0] == 0 && p[1] == 0 && p[2] == 0;
}

TEST_CASE("the rendered shadow is as wide as the formula says, to the pixel") {
  // fov wide enough that the whole shadow (about 24 px radius) fits in the 80 px height
  Camera cam{15.0, 1.4};
  Image img = render(cam, 120, 80);
  REQUIRE(img.rgb.size() == 120u * 80u * 3u);

  // shadow radius in pixels: tan of its angle over tan of half the fov, times half the width
  double radius_px = std::tan(shadow_angle(cam.r)) / std::tan(cam.fov / 2.0) * 60.0;

  // count black pixels across the middle row and down the middle column
  int across = 0, down = 0;
  for (int x = 0; x < 120; ++x) across += is_black(img, x, 40);
  for (int y = 0; y < 80; ++y) down += is_black(img, 60, y);
  CHECK(std::fabs(across - 2.0 * radius_px) <= 1.0);
  CHECK(std::fabs(down - 2.0 * radius_px) <= 1.0);

  // the corners see the sky, which is never black
  CHECK_FALSE(is_black(img, 0, 0));
  CHECK_FALSE(is_black(img, 119, 79));
}
