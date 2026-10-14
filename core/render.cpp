#include "render.h"

#include <cmath>

Ray camera_ray(const Camera& cam, double sx, double sy) {
  // pinhole camera: the direction this pixel looks in, as the observer measures it.
  // forward is -z (at the hole), right is +x, up is +y
  double t = std::tan(cam.fov / 2.0);
  Vec3 d = normalize({sx * t, sy * t, -1.0});
  // the hovering camera sees the light blueshifted by 1 / sqrt(1 - 2M / r). radial distances
  // are stretched by the same amount, which cancels it for the radial part, so only the
  // sideways part keeps it. this keeps the ray's energy at exactly 1
  double f = std::sqrt(1.0 - 2.0 / cam.r);
  return {{0.0, 0.0, cam.r}, {d.x / f, d.y / f, d.z}};
}

// debug sky: a checkerboard painted on the inside of a huge sphere, looked up by the
// direction a ray flies off in. 18 squares around, 9 from top to bottom
static void checker_sky(Vec3 dir, unsigned char* out) {
  double lon = std::atan2(dir.x, -dir.z);  // 0 is straight behind the hole
  double lat = std::acos(std::fmax(-1.0, std::fmin(1.0, dir.y)));
  int square = static_cast<int>(std::floor(lon / (M_PI / 9.0)) + std::floor(lat / (M_PI / 9.0)));
  bool light = (square & 1) == 0;
  out[0] = light ? 225 : 40;
  out[1] = light ? 225 : 80;
  out[2] = light ? 225 : 160;
}

Image render(const Camera& cam, int width, int height) {
  Image img{width, height, std::vector<unsigned char>(3 * width * height)};
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // pixel center to screen coords, sy scaled down so pixels stay square
      double sx = 2.0 * (x + 0.5) / width - 1.0;
      double sy = (1.0 - 2.0 * (y + 0.5) / height) * height / width;
      TraceResult res = trace(camera_ray(cam, sx, sy));
      unsigned char* px = &img.rgb[3 * (y * width + x)];
      // fell in: the black shadow. got away: whatever part of the sky it points at
      if (res.fate == Fate::captured) px[0] = px[1] = px[2] = 0;
      else checker_sky(res.dir, px);
    }
  }
  return img;
}
