// renders a black hole on the checkerboard sky with the cpu tracer and saves it as a png
// usage: render out.png [width height r fov_degrees]
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "render.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  if (argc != 2 && argc != 6) {
    std::fprintf(stderr, "usage: render out.png [width height r fov_degrees]\n");
    return 1;
  }
  // defaults, or whatever was passed on the command line
  int width = argc == 6 ? std::atoi(argv[2]) : 512;
  int height = argc == 6 ? std::atoi(argv[3]) : 512;
  double r = argc == 6 ? std::atof(argv[4]) : 15.0;
  double fov_deg = argc == 6 ? std::atof(argv[5]) : 60.0;

  // the camera has to sit outside the photon sphere and the image can't be empty
  if (width <= 0 || height <= 0 || r <= 3.0) {
    std::fprintf(stderr, "need width and height above 0 and r above 3\n");
    return 1;
  }

  // trace every pixel and time it
  Camera cam{r, fov_deg * M_PI / 180.0};
  auto start = std::chrono::steady_clock::now();
  Image img = render(cam, width, height);
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

  // write the png and say what was rendered
  if (!stbi_write_png(argv[1], width, height, 3, img.rgb.data(), width * 3)) {
    std::fprintf(stderr, "could not write %s\n", argv[1]);
    return 1;
  }
  std::printf("%s: %dx%d, observer at r = %gM, fov %g deg, %.2f s\n", argv[1], width, height, r,
              fov_deg, secs);
  return 0;
}
