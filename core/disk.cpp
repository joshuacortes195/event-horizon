#include "disk.h"

#include <cmath>

// standard thin disk heating, T^4 goes as r^-3 (1 - sqrt(r_in / r)) with the isco at r_in (6M without spin)
static double heating(double r, double r_in) { return std::pow(r, -3.0) * (1.0 - std::sqrt(r_in / r)); }

double disk_temperature(double r, double peak_temp, double r_in) {
  // the hottest ring sits at 49/36 of the isco, scale everything so it reads peak_temp
  const double r_peak = 49.0 / 36.0 * r_in;
  return peak_temp * std::pow(std::fmax(heating(r, r_in), 0.0) / heating(r_peak, r_in), 0.25);
}

// one bump of the fitted cie color matching curves, a gaussian that's
// a different width on each side (wyman, sloan and shirley 2013)
static double lobe(double nm, double a, double b, double g, double d) {
  double t = (nm - b) * (nm < b ? g : d);
  return a * std::exp(-0.5 * t * t);
}

Vec3 blackbody_xyz(double temp) {
  if (temp <= 0.0) return {0.0, 0.0, 0.0};
  Vec3 xyz{0.0, 0.0, 0.0};
  // walk the visible spectrum in 10 nm slices
  for (double nm = 380.0; nm <= 780.0; nm += 10.0) {
    // planck's law, wavelength in micrometers, constant shapes don't matter here
    double um = nm / 1000.0;
    double planck = 1.0 / (um * um * um * um * um * (std::exp(14387.77 / (um * temp)) - 1.0));
    // how much of each cie primary this wavelength adds
    double x = lobe(nm, 0.362, 442.0, 0.0624, 0.0374) + lobe(nm, 1.056, 599.8, 0.0264, 0.0323) +
               lobe(nm, -0.065, 501.1, 0.0490, 0.0382);
    double y = lobe(nm, 0.821, 568.8, 0.0213, 0.0247) + lobe(nm, 0.286, 530.9, 0.0613, 0.0322);
    double z = lobe(nm, 1.217, 437.0, 0.0845, 0.0278) + lobe(nm, 0.681, 459.0, 0.0385, 0.0725);
    xyz = xyz + planck * Vec3{x, y, z};
  }
  return xyz;
}

double disk_g(double r, double L, double r_obs, bool gravity, bool doppler, double a, double obs_elev) {
  double g = 1.0;
  if (a != 0.0) {
    // the camera hovers, so its clock runs slow by sqrt(-g_tt) where it sits
    double sn = std::sin(obs_elev);
    double cam = std::sqrt(1.0 - 2.0 * r_obs / (r_obs * r_obs + a * a * sn * sn));
    // gravity part: what an observer swept around with space at r (a zamo) loses climbing out to the camera.
    // no spin this is sqrt(1 - 2M / r), and unlike a hovering one it exists all the way in to the horizon
    double delta = r * r - 2.0 * r + a * a, big_a = (r * r + a * a) * (r * r + a * a) - a * a * delta;
    double lapse = std::sqrt(delta * r * r / big_a);
    if (gravity) g *= lapse / cam;
    // doppler part: the rest of it, gas orbiting at omega = 1 / (r^1.5 + a)
    if (doppler) {
      double rs = r * std::sqrt(r), omega = 1.0 / (rs + a);
      double ut = (rs + a) / (std::pow(r, 0.75) * std::sqrt(rs - 3.0 * std::sqrt(r) + 2.0 * a));
      g /= ut * lapse * (1.0 + omega * L);
    }
    return g;
  }
  // climbing out of the well from r up to the camera
  if (gravity) g *= std::sqrt(1.0 - 2.0 / r) / std::sqrt(1.0 - 2.0 / r_obs);
  // gas orbiting at angular speed r^-1.5, its clock runs slow and it pushes on the light
  // depending on which way it's moving compared to the ray
  if (doppler) {
    double omega = std::pow(r, -1.5);
    g *= std::sqrt(1.0 - 3.0 / r) / std::sqrt(1.0 - 2.0 / r) / (1.0 + omega * L);
  }
  return g;
}
