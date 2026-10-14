# Event Horizon

A real-time black hole renderer for the browser. Light rays bend around a non-spinning black hole, so you see its black shadow, the stars lensed around it, and an accretion disk that is brighter on the side spinning toward you.

Work in progress. The plan is in [docs/plan.md](docs/plan.md) and my notes on the physics are in [docs/physics-notes.md](docs/physics-notes.md).

So far: a C++ reference ray tracer that follows light around the hole and is tested against known values (the critical impact parameter, the photon sphere, light bending, the shadow size). Here is its render of the shadow on a checkerboard sky, from 15M away:

![black hole shadow on a checkerboard sky](results/shadow.png)

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build

# render the shadow image above
./build/render results/shadow.png
```
