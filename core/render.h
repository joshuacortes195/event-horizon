#pragma once

#include "disk.h"
#include "kerr.h"
#include "tracer.h"

#include <vector>

// a camera hovering at distance r from the hole (a static observer), looking at it.
// at elevation 0 it sits on the +z axis, in the disk plane, with +y as up.
// raising the elevation swings it up toward the +y spin axis
struct Camera {
  double r;
  double fov;              // field of view across the screen, in radians
  double elevation = 0.0;  // angle above the disk plane, pi/2 is looking straight down
};

// turns a point on the screen (sx, sy from -1 to 1) into the light ray that lands there
Ray camera_ray(const Camera& cam, double sx, double sy);

// same thing around a hole with spin a. the camera sits at boyer-lindquist radius cam.r,
// so its distance means the same thing as in the shadow formulas
KerrRay kerr_camera_ray(const Camera& cam, double a, double sx, double sy);

// an rgb image, 3 bytes per pixel, rows top to bottom
struct Image {
  int width, height;
  std::vector<unsigned char> rgb;
};

// the accretion disk: how far out it goes (0 means no disk), how hot its hottest ring is,
// whether gravitational redshift and doppler beaming are on, and whether the whole disk
// is one temperature (what the interstellar team did) instead of the thin disk profile
struct Disk {
  double outer = 0.0;
  double peak_temp = 0.0;  // kelvin
  bool gravity = true;
  bool doppler = true;
  bool uniform = false;
};

// traces one ray per pixel: black where it falls in, the disk's glow where it hits the disk,
// checkerboard sky where it escapes. fov is across the width, pixels are square.
// spin 0 uses the plain schwarzschild tracer, anything else the kerr one
Image render(const Camera& cam, int width, int height, const Disk& disk = {}, double spin = 0.0);
