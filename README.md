# Event Horizon

A real-time black hole renderer for the browser. Light rays bend around a non-spinning black hole, so you see its black shadow, the stars lensed around it, and an accretion disk that is brighter on the side spinning toward you.

Work in progress. The plan is in [docs/plan.md](docs/plan.md) and my notes on the physics are in [docs/physics-notes.md](docs/physics-notes.md).

So far: a C++ reference ray tracer that follows light around the hole and is tested against known values (the critical impact parameter, the photon sphere, light bending, the shadow size). Here is its render of the shadow on a checkerboard sky, from 15M away:

![black hole shadow on a checkerboard sky](results/shadow.png)

Then a thin accretion disk from the innermost stable orbit (6M) outward, colored as a blackbody, with gravitational redshift and Doppler beaming. The left side is moving toward the camera, so it's bluer and brighter. The top arc is the back of the disk, bent up over the hole:

![edge on accretion disk with redshift and beaming](results/disk_edge_on_physical.png)

Then the same math as a WebGL2 fragment shader, so it runs in the browser in real time with a starfield sky. Drag to orbit, scroll to zoom. A browser test renders six views on both the CPU and the GPU and checks they match: at most 39 of 57,600 pixels differ, all on knife edges like the rim of the shadow or the edge of a checkerboard square.

Now there's a control panel: camera distance and angles, field of view, the disk's size and temperature, switches for redshift, beaming and one temperature everywhere, a disk brightness slider, starfield or checkerboard, and max steps and resolution. Three presets jump to edge on, face on and the camera from the Interstellar paper. On a phone the panel is a sheet along the bottom, one finger orbits and two fingers pinch to zoom. The address bar follows the view, so a link opens the same picture.

Then I made it faster. Rays that are heading away and have almost no bend left stop early, and steps get bigger once a ray is far out compared to how close it passes the hole. On my RTX 3060 the default view at 1080p went from 25.8 ms to 15.2 ms a frame (39 to 66 fps), and the other presets about doubled, and in the five views I checked pixel by pixel, not one pixel changed whether it shows sky, disk or shadow. While you drag, a slow GPU drops the resolution to keep up and goes back to full quality when you stop. On my Intel Mac's integrated GPU (Iris Plus 655) the default view runs at 40 fps at 640 x 360, 11.4 fps at 720p and 5.2 fps at 1080p, so it misses the 60 fps goal I set for it. All the numbers are in `results/README.md`.

The settings for every image and the measured numbers are in [results/README.md](results/README.md).

## Build and test

On Windows I build with MinGW-w64 GCC, add `-G "MinGW Makefiles"` to the first cmake line.

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

# the cpu vs gpu pixel test and the control panel tests, needs the native build above first
npm test

# frame time benchmark: opens the ?bench page in your browser, wait for "done" and copy the table
npm run bench
```
