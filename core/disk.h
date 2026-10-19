#pragma once

#include "tracer.h"

// thin disk temperature at radius r, scaled so the hottest ring is peak_temp kelvin.
// r_in is where the disk starts, the isco (6M without spin)
double disk_temperature(double r, double peak_temp, double r_in = 6.0);

// color of a blackbody at temp kelvin as cie xyz, in arbitrary but consistent units
Vec3 blackbody_xyz(double temp);

// how much the light from the disk at radius r is shifted when the camera at r_obs sees it
// (above 1 bluer and brighter, below 1 redder and dimmer). L is the traced ray's angular
// momentum about the spin axis (+y), and the two flags turn each effect on or off.
// a is the hole's spin and obs_elev the camera's angle above the disk plane, which only
// matters once the hole spins
double disk_g(double r, double L, double r_obs, bool gravity, bool doppler, double a = 0.0, double obs_elev = 0.0);
