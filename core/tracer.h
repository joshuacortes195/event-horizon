#pragma once

// everything is in units where G = c = 1 and the black hole's mass M = 1,
// so the horizon is at r = 2 and the photon sphere at r = 3

// plain 3d vector for ray positions and velocities
struct Vec3 {
  double x, y, z;
};

// vector math the tracer needs
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(double s, Vec3 a) { return {s * a.x, s * a.y, s * a.z}; }
inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(Vec3 a);
Vec3 normalize(Vec3 a);

// a light ray: where it is and which way it's going
struct Ray {
  Vec3 pos, vel;
};

// how a traced ray ends up
enum class Fate { captured, escaped, disk };

// what tracing a ray tells us
struct TraceResult {
  Fate fate;
  double min_r;  // closest the ray got to the hole
  Vec3 dir;      // which way it's heading at the end (unit length)
  double disk_r = 0.0;  // where it hit the disk, if it did
};

// moves a ray forward by dl along its path with one rk4 step
Ray rk4_step(const Ray& ray, double dl);

// follows a ray until it falls in, gets past escape_r heading outward, or hits the
// disk in the y = 0 plane between the isco (6M) and disk_outer (0 means no disk)
TraceResult trace(Ray ray, double escape_r = 1000.0, double disk_outer = 0.0);
