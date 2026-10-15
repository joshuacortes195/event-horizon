#version 300 es
// the whole black hole, run once per pixel. same math as the c++ tracer in core/,
// just in float32 instead of double. units are G = c = 1 and M = 1
precision highp float;
precision highp int;

uniform vec2 u_resolution;  // canvas size in pixels
uniform float u_r;          // camera distance from the hole
uniform float u_fov;        // field of view across the width, radians
uniform float u_elev;       // camera angle above the disk plane, radians
uniform float u_azim;       // camera angle around the spin axis, radians
uniform float u_outer;      // disk outer edge, 0 means no disk
uniform float u_peak;       // hottest ring temperature in kelvin
uniform bool u_gravity;     // gravitational redshift on or off
uniform bool u_doppler;     // doppler beaming on or off
uniform bool u_uniform;     // whole disk one temperature (the interstellar look)
uniform bool u_stars;       // starfield sky instead of the checkerboard

out vec4 outColor;

const float PI = 3.14159265358979;
const float ESCAPE_R = 1000.0;
const float STEP_FRAC = 0.01;
// the cpu allows 100000 steps, 4000 is about six laps of the photon sphere,
// so only a hair thin ring at the shadow edge can come out different. phase 5 tunes this
const int MAX_STEPS = 4000;

// the pull of gravity on light, -3M h^2 x / r^5, h is the ray's angular momentum
vec3 accel(vec3 pos, vec3 vel) {
  vec3 h = cross(pos, vel);
  float r2 = dot(pos, pos);
  return (-3.0 * dot(h, h) / (r2 * r2 * sqrt(r2))) * pos;
}

// one rk4 step: samples the pull four times across the step and blends them
void rk4_step(inout vec3 pos, inout vec3 vel, float dl) {
  vec3 p1 = pos, v1 = vel, a1 = accel(p1, v1);
  vec3 p2 = p1 + 0.5 * dl * v1, v2 = v1 + 0.5 * dl * a1, a2 = accel(p2, v2);
  vec3 p3 = p1 + 0.5 * dl * v2, v3 = v1 + 0.5 * dl * a2, a3 = accel(p3, v3);
  vec3 p4 = p1 + dl * v3, v4 = v1 + dl * a3, a4 = accel(p4, v4);
  pos = p1 + (dl / 6.0) * (v1 + 2.0 * v2 + 2.0 * v3 + v4);
  vel = v1 + (dl / 6.0) * (a1 + 2.0 * a2 + 2.0 * a3 + a4);
}

// thin disk heating, T^4 goes as r^-3 (1 - sqrt(6 / r)) with the isco at 6M
float heating(float r) { return (1.0 - sqrt(6.0 / r)) / (r * r * r); }

// thin disk temperature, scaled so the ring at 49/36 of the isco reads peak kelvin
float disk_temperature(float r, float peak) {
  return peak * pow(max(heating(r), 0.0) / heating(49.0 / 36.0 * 6.0), 0.25);
}

// one bump of the fitted cie color matching curves (wyman, sloan and shirley 2013)
float lobe(float nm, float a, float b, float g, float d) {
  float t = (nm - b) * (nm < b ? g : d);
  return a * exp(-0.5 * t * t);
}

// color of a blackbody at temp kelvin as cie xyz, walking the visible spectrum in 10 nm slices
vec3 blackbody_xyz(float temp) {
  if (temp <= 0.0) return vec3(0.0);
  vec3 xyz = vec3(0.0);
  for (int i = 0; i <= 40; ++i) {
    float nm = 380.0 + 10.0 * float(i);
    // planck's law, wavelength in micrometers
    float um = nm / 1000.0;
    // past e^80 this slice adds nothing, and skipping it keeps exp from overflowing on cold rings
    float e = 14387.77 / (um * temp);
    if (e > 80.0) continue;
    float planck = 1.0 / (um * um * um * um * um * (exp(e) - 1.0));
    // how much of each cie primary this wavelength adds
    float x = lobe(nm, 0.362, 442.0, 0.0624, 0.0374) + lobe(nm, 1.056, 599.8, 0.0264, 0.0323) +
              lobe(nm, -0.065, 501.1, 0.0490, 0.0382);
    float y = lobe(nm, 0.821, 568.8, 0.0213, 0.0247) + lobe(nm, 0.286, 530.9, 0.0613, 0.0322);
    float z = lobe(nm, 1.217, 437.0, 0.0845, 0.0278) + lobe(nm, 0.681, 459.0, 0.0385, 0.0725);
    xyz += planck * vec3(x, y, z);
  }
  return xyz;
}

// how shifted the disk's light is when it reaches the camera, above 1 bluer and brighter
float disk_g(float r, float L) {
  float g = 1.0;
  // climbing out of the well from r up to the camera
  if (u_gravity) g *= sqrt(1.0 - 2.0 / r) / sqrt(1.0 - 2.0 / u_r);
  // gas orbiting at angular speed r^-1.5, its clock runs slow and it pushes on the light
  if (u_doppler) g *= sqrt(1.0 - 3.0 / r) / sqrt(1.0 - 2.0 / r) / (1.0 + pow(r, -1.5) * L);
  return g;
}

// one channel of linear light to screen brightness: soft roll off, then the srgb gamma curve
float to_screen(float c) {
  c = 1.0 - exp(-max(c, 0.0));
  return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

// paints the disk's blackbody glow, exposure set so the hottest ring with no shifts reads 1
vec3 disk_color(float temp) {
  // a zero temperature disk is just black, same as the cpu
  if (u_peak <= 0.0) return vec3(0.0);
  vec3 c = blackbody_xyz(temp) / blackbody_xyz(u_peak).y;
  // cie xyz to linear srgb
  return vec3(to_screen(3.2406 * c.x - 1.5372 * c.y - 0.4986 * c.z),
              to_screen(-0.9689 * c.x + 1.8758 * c.y + 0.0415 * c.z),
              to_screen(0.0557 * c.x - 0.2040 * c.y + 1.0570 * c.z));
}

// debug sky: checkerboard on a huge sphere, 18 squares around and 9 top to bottom
vec3 checker_sky(vec3 dir) {
  float lon = atan(dir.x, -dir.z);  // 0 is straight behind the hole
  float lat = acos(clamp(dir.y, -1.0, 1.0));
  int square = int(floor(lon / (PI / 9.0)) + floor(lat / (PI / 9.0)));
  return (square & 1) == 0 ? vec3(225.0) / 255.0 : vec3(40.0, 80.0, 160.0) / 255.0;
}

// cheap random numbers from a position, no sin so it acts the same on every gpu
vec3 hash33(vec3 p) {
  p = fract(p * vec3(0.1031, 0.1030, 0.0973));
  p += dot(p, p.yxz + 33.33);
  return fract((p.xxy + p.yxx) * p.zyx);
}

// starfield: chops the sky into little cells, a few of them get a star at a random spot.
// two layers, lots of faint stars and a handful of bright ones
vec3 star_sky(vec3 dir) {
  vec3 col = vec3(0.0);
  for (int layer = 0; layer < 2; ++layer) {
    float scale = layer == 0 ? 150.0 : 50.0;
    vec3 cell = floor(dir * scale);
    vec3 h = hash33(cell + 71.0 * float(layer));
    // most cells stay empty
    if (h.x > 0.12) continue;
    // where the star sits, pushed onto the sky sphere, and how far this pixel is from it
    vec3 star = normalize(cell + 0.25 + 0.5 * hash33(cell + 13.0)) * scale;
    float d = length(dir * scale - star);
    // brightness falls off fast from the center, color goes from orange to blue white
    float glow = exp(-d * d * 40.0) * (0.3 + 3.0 * h.y * h.y) * (layer == 0 ? 0.6 : 1.5);
    col += glow * mix(vec3(1.0, 0.75, 0.5), vec3(0.75, 0.85, 1.0), h.z);
  }
  return col;
}

void main() {
  // pixel center to screen coords from -1 to 1 across, scaled up and down so pixels stay square
  vec2 s = (2.0 * gl_FragCoord.xy / u_resolution - 1.0) * vec2(1.0, u_resolution.y / u_resolution.x);

  // pinhole direction in the camera's own frame, looking down -z at the hole
  float t = tan(u_fov / 2.0);
  vec3 d = normalize(vec3(s * t, -1.0));
  // the hovering camera's blueshift only stretches the sideways part, keeps the ray's energy at 1
  float f = sqrt(1.0 - 2.0 / u_r);
  vec3 v = vec3(d.x / f, d.y / f, d.z);

  // tilt the camera up by its elevation, then swing it around the spin axis by its azimuth
  float ce = cos(u_elev), se = sin(u_elev), ca = cos(u_azim), sa = sin(u_azim);
  vec3 pos = vec3(0.0, u_r * se, u_r * ce);
  vec3 vel = vec3(v.x, v.y * ce + v.z * se, v.z * ce - v.y * se);
  pos = vec3(pos.x * ca + pos.z * sa, pos.y, pos.z * ca - pos.x * sa);
  vel = vec3(vel.x * ca + vel.z * sa, vel.y, vel.z * ca - vel.x * sa);
  // angular momentum about the spin axis never changes, so the disk's doppler shift can use it
  float L = cross(pos, vel).y;

  // follow the ray until it falls in, escapes or hits the disk. running out of steps counts as falling in
  vec3 color = vec3(0.0);
  for (int i = 0; i < MAX_STEPS; ++i) {
    float r = length(pos);
    // inside the horizon, the ray is gone and the pixel stays black
    if (r < 2.0) break;
    // far away and heading out, paint whatever part of the sky it points at
    if (r > ESCAPE_R && dot(pos, vel) > 0.0) {
      vec3 dir = normalize(vel);
      color = u_stars ? star_sky(dir) : checker_sky(dir);
      break;
    }
    vec3 prev = pos;
    rk4_step(pos, vel, STEP_FRAC * r);
    // crossed the disk plane this step, find where and check it's on the disk.
    // a ray starting right on the plane is leaving it, not hitting it
    if (u_outer > 0.0 && prev.y != 0.0 && (prev.y > 0.0) != (pos.y > 0.0)) {
      float hit_r = length(mix(prev, pos, prev.y / (prev.y - pos.y)));
      if (hit_r >= 6.0 && hit_r <= u_outer) {
        float temp = u_uniform ? u_peak : disk_temperature(hit_r, u_peak);
        color = disk_color(disk_g(hit_r, L) * temp);
        break;
      }
    }
  }
  outColor = vec4(color, 1.0);
}
