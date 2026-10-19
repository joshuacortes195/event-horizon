// renders a black hole on the checkerboard sky with the cpu tracer and saves it as a png
// usage: render out.png [width height r fov_degrees [elevation_degrees disk_outer temp look [spin]]]
// look is physical (redshift and beaming on), flat (both off) or uniform (one temperature, no shifts).
// spin is the hole's spin from 0 up to 0.99, 0 if it's left out
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "render.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
  if (argc != 2 && argc != 6 && argc != 10 && argc != 11) {
    std::fprintf(stderr,
                 "usage: render out.png [width height r fov_degrees [elevation_degrees disk_outer temp "
                 "physical|flat|uniform [spin]]]\n");
    return 1;
  }
  // defaults, or whatever was passed on the command line
  int width = argc >= 6 ? std::atoi(argv[2]) : 512;
  int height = argc >= 6 ? std::atoi(argv[3]) : 512;
  double r = argc >= 6 ? std::atof(argv[4]) : 15.0;
  double fov_deg = argc >= 6 ? std::atof(argv[5]) : 60.0;

  // camera tilt and the disk, no disk unless all four are given
  double elev_deg = argc >= 10 ? std::atof(argv[6]) : 0.0;
  Disk disk;
  std::string look = argc >= 10 ? argv[9] : "";
  if (argc >= 10) {
    disk.outer = std::atof(argv[7]);
    disk.peak_temp = std::atof(argv[8]);
    disk.gravity = disk.doppler = look == "physical";
    disk.uniform = look == "uniform";
    if (look != "physical" && look != "flat" && look != "uniform") {
      std::fprintf(stderr, "look has to be physical, flat or uniform\n");
      return 1;
    }
  }

  // the hole's spin, 0 if it's left out
  double spin = argc == 11 ? std::atof(argv[10]) : 0.0;

  // the camera has to sit outside the photon sphere and the image can't be empty
  if (width <= 0 || height <= 0 || r <= 3.0 || spin < 0.0 || spin > 0.99) {
    std::fprintf(stderr, "need width and height above 0, r above 3 and spin from 0 to 0.99\n");
    return 1;
  }

  // trace every pixel and time it
  Camera cam{r, fov_deg * M_PI / 180.0, elev_deg * M_PI / 180.0};
  auto start = std::chrono::steady_clock::now();
  Image img = render(cam, width, height, disk, spin);
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

  // write the png and say what was rendered
  if (!stbi_write_png(argv[1], width, height, 3, img.rgb.data(), width * 3)) {
    std::fprintf(stderr, "could not write %s\n", argv[1]);
    return 1;
  }
  std::printf("%s: %dx%d, observer at r = %gM, fov %g deg, elevation %g deg", argv[1], width, height, r,
              fov_deg, elev_deg);
  if (disk.outer > 0.0) std::printf(", disk out to %gM at %g K, %s", disk.outer, disk.peak_temp, look.c_str());
  if (spin > 0.0) std::printf(", spin %g", spin);
  std::printf(", %.2f s\n", secs);
  return 0;
}
