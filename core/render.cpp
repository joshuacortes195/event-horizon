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
  Vec3 v{d.x / f, d.y / f, d.z};
  // tilt the whole camera up about the x axis by its elevation
  double c = std::cos(cam.elevation), s = std::sin(cam.elevation);
  return {{0.0, cam.r * s, cam.r * c}, {v.x, v.y * c + v.z * s, v.z * c - v.y * s}};
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

// one channel of linear light to a screen byte: soft roll off so bright spots don't clip
// to flat white, then the srgb gamma curve
static unsigned char to_byte(double c) {
  c = 1.0 - std::exp(-std::fmax(c, 0.0));
  c = c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
  return static_cast<unsigned char>(std::lround(255.0 * c));
}

// paints a blackbody seen at temp kelvin, with exposure picked so the hottest ring
// with no shifts has brightness 1
static void disk_color(double temp, double exposure, unsigned char* out) {
  Vec3 c = exposure * blackbody_xyz(temp);
  // cie xyz to linear srgb
  out[0] = to_byte(3.2406 * c.x - 1.5372 * c.y - 0.4986 * c.z);
  out[1] = to_byte(-0.9689 * c.x + 1.8758 * c.y + 0.0415 * c.z);
  out[2] = to_byte(0.0557 * c.x - 0.2040 * c.y + 1.0570 * c.z);
}

Image render(const Camera& cam, int width, int height, const Disk& disk) {
  Image img{width, height, std::vector<unsigned char>(3 * width * height)};
  double exposure = disk.peak_temp > 0.0 ? 1.0 / blackbody_xyz(disk.peak_temp).y : 0.0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // pixel center to screen coords, sy scaled down so pixels stay square
      double sx = 2.0 * (x + 0.5) / width - 1.0;
      double sy = (1.0 - 2.0 * (y + 0.5) / height) * height / width;
      Ray ray = camera_ray(cam, sx, sy);
      TraceResult res = trace(ray, 1000.0, disk.outer);
      unsigned char* px = &img.rgb[3 * (y * width + x)];
      // fell in: the black shadow. got away: whatever part of the sky it points at
      if (res.fate == Fate::captured) px[0] = px[1] = px[2] = 0;
      else if (res.fate == Fate::escaped) checker_sky(res.dir, px);
      else {
        // hit the disk: the ray's angular momentum about the spin axis never changes,
        // so read it off the camera ray. the glow is a blackbody at g times the gas temperature
        double L = cross(ray.pos, ray.vel).y;
        double g = disk_g(res.disk_r, L, cam.r, disk.gravity, disk.doppler);
        double temp = disk.uniform ? disk.peak_temp : disk_temperature(res.disk_r, disk.peak_temp);
        disk_color(g * temp, exposure, px);
      }
    }
  }
  return img;
}
