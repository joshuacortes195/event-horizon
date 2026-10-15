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

TEST_CASE("a camera tilted up over the pole sees the disk face on") {
  // far up the spin axis, looking down at the disk
  Camera cam{1e6, 1e-4, M_PI / 2.0};
  CHECK(camera_ray(cam, 0.0, 0.0).pos.y == doctest::Approx(1e6));
  // screen offset of a ray with b = 10.95612, the impact parameter of the 10M ring
  double b = 10.95612;
  double s = std::tan(std::asin(b * std::sqrt(1.0 - 2.0 / cam.r) / cam.r)) / std::tan(cam.fov / 2.0);
  // left, right, up and down all land on the same ring
  for (auto [sx, sy] : {std::pair{s, 0.0}, std::pair{-s, 0.0}, std::pair{0.0, s}, std::pair{0.0, -s}}) {
    TraceResult res = trace(camera_ray(cam, sx, sy), 1000.0, 20.0);
    CHECK(res.fate == Fate::disk);
    CHECK(res.disk_r == doctest::Approx(10.0).epsilon(2e-5));
  }
}

// adds up every channel of the disk pixels in columns [x0, x1). a pixel is on the disk if
// it differs from the same view with no disk (the checkerboard isn't mirror symmetric, so
// leaving the sky in would swamp the comparison)
static double disk_brightness(const Image& img, const Image& sky, int x0, int x1) {
  double sum = 0.0;
  for (int y = 0; y < img.height; ++y)
    for (int x = x0; x < x1; ++x) {
      const unsigned char* p = &img.rgb[3 * (y * img.width + x)];
      const unsigned char* q = &sky.rgb[3 * (y * img.width + x)];
      if (p[0] != q[0] || p[1] != q[1] || p[2] != q[2]) sum += p[0] + p[1] + p[2];
    }
  return sum;
}

TEST_CASE("a tilted camera's rays still have energy exactly 1") {
  // the radial equation says (dr/dl)^2 + (1 - 2M/r) h^2 / r^2 = E^2, and the camera sets E = 1
  for (double elevation : {0.0, 0.7, M_PI / 2.0}) {
    Camera cam{6.0, 1.0, elevation};
    for (auto [sx, sy] : {std::pair{0.3, 0.2}, std::pair{-0.8, 0.5}}) {
      Ray ray = camera_ray(cam, sx, sy);
      double r = length(ray.pos);
      double dr = dot(ray.pos, ray.vel) / r;
      double h = length(cross(ray.pos, ray.vel));
      CHECK(dr * dr + (1.0 - 2.0 / r) * h * h / (r * r) == doctest::Approx(1.0).epsilon(1e-12));
    }
  }
}

TEST_CASE("beaming makes the side of the disk spinning toward us brighter") {
  // nearly edge on, a bit above the disk so it isn't a line
  Camera cam{30.0, 1.2, 0.1};
  Image sky = render(cam, 120, 80);
  Disk disk{20.0, 6000.0, false, false};
  // shifts off: what's on the left is a mirror of what's on the right
  Image flat = render(cam, 120, 80, disk);
  CHECK(disk_brightness(flat, sky, 0, 60) / disk_brightness(flat, sky, 60, 120) ==
        doctest::Approx(1.0).epsilon(0.01));
  // the gas on the left is coming at the camera, the gas on the right is moving away
  disk.gravity = disk.doppler = true;
  Image beamed = render(cam, 120, 80, disk);
  double ratio = disk_brightness(beamed, sky, 0, 60) / disk_brightness(beamed, sky, 60, 120);
  CHECK(ratio > 1.5);
  // and by how much: pinned to what this view measured when i checked it (2.17), so a change
  // to the color math that shifts it shows up
  CHECK(ratio == doctest::Approx(2.17).epsilon(0.05));
}

// color of the first disk pixel found, for a disk that's one color everywhere
static void uniform_disk_color(double temp, int rgb[3]) {
  Camera cam{30.0, 1.2, 0.1};
  Image sky = render(cam, 120, 80);
  Image img = render(cam, 120, 80, Disk{20.0, temp, false, false, true});
  for (int i = 0; i < 120 * 80; ++i) {
    const unsigned char* p = &img.rgb[3 * i];
    const unsigned char* q = &sky.rgb[3 * i];
    if (p[0] == q[0] && p[1] == q[1] && p[2] == q[2]) continue;
    for (int c = 0; c < 3; ++c) rgb[c] = p[c];
    return;
  }
}

TEST_CASE("disk colors go from orange to white to blue as it gets hotter") {
  int rgb[3];
  // 6500K is about white, and the exposure gives it brightness 1: 1 - e^-1 = 0.632 linear,
  // which is 208 after the srgb curve
  uniform_disk_color(6500.0, rgb);
  for (int c = 0; c < 3; ++c) CHECK(std::abs(rgb[c] - 208) <= 4);
  // cooler is orange, red over green over blue
  uniform_disk_color(3000.0, rgb);
  CHECK(rgb[0] > rgb[1]);
  CHECK(rgb[1] > rgb[2]);
  // hotter is blue
  uniform_disk_color(10000.0, rgb);
  CHECK(rgb[2] > rgb[0]);
}

TEST_CASE("the interstellar look paints the whole disk one color") {
  // one temperature everywhere and no shifts, like the film
  Camera cam{30.0, 1.2, 0.1};
  Image sky = render(cam, 120, 80);
  Disk disk{20.0, 4500.0, false, false, true};
  Image img = render(cam, 120, 80, disk);
  // every pixel that isn't sky should match the first disk pixel found
  int disk_px = 0, off_color = 0;
  const unsigned char* first = nullptr;
  for (int i = 0; i < 120 * 80; ++i) {
    const unsigned char* p = &img.rgb[3 * i];
    const unsigned char* q = &sky.rgb[3 * i];
    if (p[0] == q[0] && p[1] == q[1] && p[2] == q[2]) continue;
    if (!first) first = p;
    ++disk_px;
    off_color += p[0] != first[0] || p[1] != first[1] || p[2] != first[2];
  }
  CHECK(disk_px > 500);
  CHECK(off_color == 0);
}
