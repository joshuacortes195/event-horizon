# Event Horizon

A real-time black hole renderer for the browser. Light rays bend around a non-spinning black hole, so you see its black shadow, the stars lensed around it, and an accretion disk that is brighter on the side spinning toward you.

Work in progress. The plan is in [docs/plan.md](docs/plan.md) and my notes on the physics are in [docs/physics-notes.md](docs/physics-notes.md).

So far: a C++ reference ray tracer that follows light around the hole and is tested against known values (the critical impact parameter, the photon sphere, light bending, the shadow size). Here is its render of the shadow on a checkerboard sky, from 15M away:

![black hole shadow on a checkerboard sky](results/shadow.png)

Then a thin accretion disk from the innermost stable orbit (6M) outward, colored as a blackbody, with gravitational redshift and Doppler beaming. The left side is moving toward the camera, so it's bluer and brighter. The top arc is the back of the disk, bent up over the hole:

![edge on accretion disk with redshift and beaming](results/disk_edge_on_physical.png)

Then the same math as a WebGL2 fragment shader, so it runs in the browser in real time with a starfield sky. Drag to orbit, scroll to zoom. A browser test renders five views on both the CPU and the GPU and checks they match: at most 24 of 57,600 pixels differ, all on knife edges where float32 rounding tips a ray one way or the other.

The settings for every image and the measured numbers are in [results/README.md](results/README.md).

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build

# render the shadow image above
./build/render results/shadow.png

# render the disk image above: width height r fov elevation disk_outer temp look
./build/render results/disk_edge_on_physical.png 640 360 50 80 2 20 6000 physical
```

## Run it in the browser

```bash
cd web
npm install
npx playwright install chromium   # once, for the browser test
npm run dev                       # open the printed localhost link

# the cpu vs gpu pixel test, needs the native build above first
npm test
```
