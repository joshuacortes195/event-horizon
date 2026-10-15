// measures how much brighter the left half of the disk is than the right half, by adding up
// every channel of the disk pixels on each side. a pixel counts as disk if it changes when
// the disk is added (the checkerboard sky isn't mirror symmetric, so it's left out)
// usage: beaming_ratio width height r fov_degrees elevation_degrees disk_outer temp
#include "render.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// adds up the disk pixels in columns [x0, x1)
static double side(const Image& img, const Image& sky, int x0, int x1) {
  double sum = 0.0;
  for (int y = 0; y < img.height; ++y)
    for (int x = x0; x < x1; ++x) {
      const unsigned char* p = &img.rgb[3 * (y * img.width + x)];
      const unsigned char* q = &sky.rgb[3 * (y * img.width + x)];
      if (p[0] != q[0] || p[1] != q[1] || p[2] != q[2]) sum += p[0] + p[1] + p[2];
    }
  return sum;
}

int main(int argc, char** argv) {
  if (argc != 8) {
    std::fprintf(stderr, "usage: beaming_ratio width height r fov_degrees elevation_degrees disk_outer temp\n");
    return 1;
  }
  // camera and disk from the command line
  int w = std::atoi(argv[1]), h = std::atoi(argv[2]);
  Camera cam{std::atof(argv[3]), std::atof(argv[4]) * M_PI / 180.0, std::atof(argv[5]) * M_PI / 180.0};
  Disk disk{std::atof(argv[6]), std::atof(argv[7]), false, false};

  // the same view with no disk, then with the shifts off and on
  Image sky = render(cam, w, h);
  Image flat = render(cam, w, h, disk);
  disk.gravity = disk.doppler = true;
  Image physical = render(cam, w, h, disk);
  std::printf("left / right, shifts off: %.3f, redshift and beaming on: %.3f\n",
              side(flat, sky, 0, w / 2) / side(flat, sky, w / 2, w),
              side(physical, sky, 0, w / 2) / side(physical, sky, w / 2, w));
  return 0;
}
