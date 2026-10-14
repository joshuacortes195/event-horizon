#pragma once

#include "tracer.h"

#include <vector>

// a camera hovering at distance r from the hole (a static observer), looking at it.
// it sits on the +z axis with +y as up
struct Camera {
  double r;
  double fov;  // field of view across the screen, in radians
};

// turns a point on the screen (sx, sy from -1 to 1) into the light ray that lands there
Ray camera_ray(const Camera& cam, double sx, double sy);

// an rgb image, 3 bytes per pixel, rows top to bottom
struct Image {
  int width, height;
  std::vector<unsigned char> rgb;
};

// traces one ray per pixel: black where it falls in, checkerboard sky where it escapes.
// fov is across the width, pixels are square
Image render(const Camera& cam, int width, int height);
