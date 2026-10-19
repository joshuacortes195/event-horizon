#pragma once

#include "tracer.h"

// the spinning (kerr) black hole. same units as tracer.h, M = 1, and a is the spin from 0 to
// just under 1, pointing along +y. positions are cartesian kerr-schild x, y, z, which are plain
// x, y, z far away and still smooth at the poles and through the horizon

// a light ray around the spinning hole: where it is and its momentum p (the lowered index kind).
// its energy is always exactly 1
struct KerrRay {
  Vec3 pos, p;
};

// outer event horizon, 1 + sqrt(1 - a^2)
double horizon_r(double a);

// innermost stable circular orbit for gas orbiting the same way the hole spins
// (bardeen, press and teukolsky 1972), 6 at a = 0 down to 1 at a = 1
double isco_r(double a);

// the radius r of a kerr-schild point, the one the horizon and the disk are measured in.
// it's the plain distance at a = 0, a bit less than it near the disk plane when the hole spins
double kerr_radius(Vec3 pos, double a);

// the ray's hamiltonian, zero for light. the tests use it to check nothing drifts
double kerr_hamiltonian(const KerrRay& ray, double a);

// how fast the ray's position and momentum change along its path
struct KerrDeriv {
  Vec3 dpos, dp;
};
KerrDeriv kerr_derivs(const KerrRay& ray, double a);

// moves a ray forward by dl along its path with one rk4 step
KerrRay kerr_step(const KerrRay& ray, double a, double dl);

// how far kerr_trace steps a ray at radius r: 1% of r like the non spinning tracer, smaller
// close to the horizon where a fast spinning hole's light orbits hug it, but never under 1e-4
double kerr_step_size(double r, double a);

// the ray's angular momentum about the spin axis, it never changes along the path
double kerr_L(const KerrRay& ray);

// light leaving a hovering (static) observer at pos in direction dir, where dir is in the observer's
// own frame: dir.x along right, dir.y along up, dir.z along forward. the frame is built from the
// three seed directions so right is exactly around the spin axis and forward exactly at the hole
KerrRay kerr_launch(Vec3 pos, Vec3 right, Vec3 up, Vec3 forward, Vec3 dir, double a);

// follows a ray until it falls in, gets past escape_r heading outward, or hits the disk in the
// y = 0 plane between the isco and disk_outer (0 means no disk). same rules as trace() in tracer.h
TraceResult kerr_trace(KerrRay ray, double a, double escape_r = 1000.0, double disk_outer = 0.0);
