# Results

Every image and number here says how it was made. Nothing in this folder is an estimate.

Machine for everything below unless it says otherwise: Intel Core i5-8259U @ 2.30GHz (4 cores, 8 threads), 8 GB RAM, macOS 15.7.7, Apple clang 17.0.0, CMake Release build (`-O3 -DNDEBUG`).

## Phase 1: CPU tracer

### shadow.png

![black hole shadow on the checkerboard sky](shadow.png)

The black hole's shadow on the checkerboard debug sky, seen by a static observer.

| Setting | Value |
| --- | --- |
| Command | `./build/render results/shadow.png` (the defaults) |
| Size | 512 x 512 |
| Observer | static, at r = 15M, looking straight at the hole |
| Field of view | 60 degrees across the width |
| Integrator | RK4, step = 0.01 r, captured below r = 2M, escaped past r = 1000M heading out |
| Threads | 1 |
| Render time | 16.27 s (one run, timed inside the tool with `std::chrono::steady_clock`) |

I counted the black pixels in the saved PNG: 302 across the middle row and 302 down the middle column. The static observer formula sin(alpha) = (3 sqrt(3) M / r) sqrt(1 - 2M / r) gives alpha = 0.3284 rad at r = 15M, so the shadow should be 2 * tan(alpha) / tan(30 deg) * 256 = 302.1 px across. The test suite checks the same thing to the pixel on a smaller image.

### Accuracy of the tracer

From the test suite (`tests/test_tracer.cpp`, `tests/test_render.cpp`), all at step = 0.01 r unless it says otherwise.

| Check | Expected | Measured |
| --- | --- | --- |
| Capture vs escape around b = 3 sqrt(3) M | captured at b_crit (1 - 1e-6), escapes at b_crit (1 + 1e-6) | both right |
| Closest approach r0 for r0 = 3.5, 4, 6, 10, 50 | b = r0 / sqrt(1 - 2M / r0) | largest relative error 5.3e-6 (at r0 = 4). It comes from only checking r at step points, which can be off by up to about 1.25e-5 relative depending on where the steps land |
| Bending at b = 1000M | 4-term weak field series, 4.011823809e-3 rad | relative error 2.7e-10 |
| Bending at b = 300M | 4-term weak field series | relative error 2.2e-8 (about the size of the 5th order term the series leaves out) |
| Photon sphere, launched sideways at r = 3M, 360 steps per orbit | stays at r = 3M | off by 1.2e-6 after one orbit |
| Photon sphere instability | errors grow by e^(2 pi) = 535.5 per orbit | 537.5 from orbit 1 to orbit 2 |
| RK4 order | error drops 16x when the step is halved | 16.0 (1.155e-6 at 360 steps per orbit, 7.24e-8 at 720) |
| Shadow edge for a static observer at r = 6M, 10M, 30M | Synge's formula | within 1e-5 relative, both across and up |

The bending numbers and the photon sphere numbers come from small probe programs run once with the same tracer, and the tests check them with some margin.

## Phase 2: accretion disk on the CPU

Six reference images of a thin disk around the hole. All of them are 640 x 360 on the checkerboard sky, rendered one at a time on one thread with the same tracer settings as above. Render times are single runs, timed inside the tool.

The disk sits in the y = 0 plane from the ISCO (6M) out to 20M. 20M is my own pick so the disk fits on screen, not a value from a paper. A ray hits the disk when its y changes sign during a step and the crossing point (linear interpolation along that step) is between 6M and 20M. The disk is opaque, so the first hit is the color.

Colors: the gas at radius r has the thin disk temperature T(r), the light reaches the camera shifted by g, and a blackbody at T is seen as a blackbody at gT. I turn that into CIE XYZ and then sRGB. Exposure is set so the hottest ring with no shifts has brightness Y = 1, then each channel goes through 1 - exp(-c) so bright spots roll off instead of clipping, then the sRGB gamma curve.

The 6000K peak is picked so the disk looks white hot in visible light. A real disk around a stellar mass black hole is far hotter and glows mostly in X-rays, so the colors are a look, not a measurement.

| Image | Command (`./build/render ...`) | Camera | Disk | Time |
| --- | --- | --- | --- | --- |
| [disk_face_on_physical.png](disk_face_on_physical.png) | `results/disk_face_on_physical.png 640 360 50 80 90 20 6000 physical` | r = 50M, fov 80 deg, 90 deg above the disk (over the pole) | 6000K peak, redshift and beaming on | 9.81 s |
| [disk_face_on_flat.png](disk_face_on_flat.png) | `results/disk_face_on_flat.png 640 360 50 80 90 20 6000 flat` | same | 6000K peak, both off | 9.87 s |
| [disk_edge_on_physical.png](disk_edge_on_physical.png) | `results/disk_edge_on_physical.png 640 360 50 80 2 20 6000 physical` | r = 50M, fov 80 deg, 2 deg above the disk | 6000K peak, both on | 12.72 s |
| [disk_edge_on_flat.png](disk_edge_on_flat.png) | `results/disk_edge_on_flat.png 640 360 50 80 2 20 6000 flat` | same | 6000K peak, both off | 12.79 s |
| [disk_interstellar.png](disk_interstellar.png) | `results/disk_interstellar.png 640 360 74.1 40 3.44 20 4500 uniform` | r = 74.1M, fov 40 deg, 3.44 deg above the disk | one temperature (4500K) everywhere, no shifts | 14.00 s |
| [disk_interstellar_angle_physical.png](disk_interstellar_angle_physical.png) | `results/disk_interstellar_angle_physical.png 640 360 74.1 40 3.44 20 4500 physical` | same | 4500K peak, both on | 17.42 s |

![edge on disk with redshift and beaming](disk_edge_on_physical.png)

![the interstellar setup](disk_interstellar.png)

The Interstellar camera and disk come from the film team's paper (James, von Tunzelmann, Franklin and Thorne 2015, arXiv:1502.03808): no Doppler or gravitational shifts, one 4500K temperature, camera at 74.1M and 3.44 degrees above the disk. Their hole spins (a/M = 0.6) and mine doesn't, so it's the same idea, not the same picture.

### How much brighter the approaching side is

Sum of R + G + B over the disk pixels in the left half of the image divided by the same sum for the right half. A pixel counts as disk if it differs from the same view rendered with no disk. Measured with `./build/beaming_ratio 640 360 50 80 2 20 6000` and `./build/beaming_ratio 640 360 74.1 40 3.44 20 4500` (`tools/beaming_ratio.cpp`). The numbers come out the same on every run, nothing in it is random.

| View | Shifts off | Redshift and beaming on |
| --- | --- | --- |
| Edge on (r = 50M, 2 deg, 6000K) | 1.000 | 2.054 |
| Interstellar angle (r = 74.1M, 3.44 deg, 4500K) | 1.000 | 2.896 |

On screen the gap is smaller than in the light itself. Take the edge on test view in `tests/test_render.cpp` (r = 30M, fov 1.2 rad, 0.1 rad above the disk, 6000K) and the two rays at screen x = -0.5 and +0.5, y = -0.05. Both hit the disk at r = 10.75M. The left one has g = 1.227, the right one g = 0.685, and their brightness Y relative to the unshifted hottest ring is 1.784 and 0.091, about 20 times apart. (g at that radius ranges from about 0.66 to 1.32 depending on where on the ring the ray lands.)

Two steps squeeze that 20x. For a grey of those brightnesses, the 1 - exp(-c) roll off takes it to 0.832 vs 0.087 (about 9.6x), and the sRGB gamma curve takes that to 0.922 vs 0.326 (about 2.8x). The test on that view measures 2.17 over all disk pixels and checks it stays within 5%.

### Accuracy of the disk hits

From `tests/test_disk.cpp`. Face on rays from far up the spin axis with the impact parameter of a known ring:

| Ring | b | Lands at |
| --- | --- | --- |
| 8.1667M (hottest) | 9.11432 | 8.1666219 (relative error 5.5e-6) |
| 10M | 10.95612 | 9.9999860 (1.4e-6) |
| 6M (ISCO) | 6.93215 | 5.9999634, so it falls just inside the hole in the disk. With a 10x smaller step it lands at 6.0000011 |

Because the 6M ray sits right on the edge, the RK4 step error decides which side it lands on, so the test checks the edge with b = 6.88 (falls through) and b = 6.98 (lands between 6M and 6.1M) instead. The ring checks use a relative tolerance of 2e-5.

### What the disk tests catch

After review I broke the code on purpose in a scratch copy and ran the tests each time. Caught: a wrong orbital speed (Omega = r^-1 or r^-1.25), (1 - Omega L) in place of 1 / (1 + Omega L), dropping the camera's sqrt(1 - 2M / r_obs), red and blue swapped, no gamma curve, g^2 T in place of g T, the sqrt(1 - 2M / r) camera fix applied after the tilt, and the sign of L flipped. Not caught: exposure set from X instead of Y, which only moves brightness by about 3%, inside the color test's 4 count margin.

## Phase 3: shader port

### CPU vs GPU pixel test

`web/tests/cpu-match.spec.ts`, run with `npm test` in `web/` (needs the native build first, it calls `build/render`, or `build/render.exe` on Windows). For each view it renders a 320x180 PNG with the CPU tracer, renders the same view in the browser with the shader on the checkerboard sky, reads the pixels back with `readPixels` and compares them channel by channel. It also fails on any console error.

| View | Camera | Disk |
| --- | --- | --- |
| shadow | r = 15M, fov 60 deg, elevation 0 | none |
| physical disk | r = 30M, fov 60 deg, 10 deg above the disk | out to 20M, 6000K peak, redshift and beaming on |
| interstellar uniform disk | r = 74.1M, fov 40 deg, 3.44 deg above the disk | out to 20M, 4500K everywhere, no shifts |
| face on physical disk | r = 30M, fov 60 deg, 90 deg above the disk | out to 20M, 6000K peak, redshift and beaming on |
| near edge on physical disk, swung around | r = 50M, fov 50 deg, 2 deg above the disk, GPU camera swung 40 deg around the spin axis | out to 20M, 6000K peak, redshift and beaming on |

The last two views were added after the phase 3 review. The CPU camera has no azimuth, so the swung view only swings the GPU camera. 40 deg is exactly two checkerboard squares and the disk is round, so the picture has to come out the same as the CPU's unswung one.

Measured on 2026-10-15, Chromium 156.0.8078.4 from Playwright 1.64.1. Headless Chromium runs WebGL on SwiftShader (a software GPU on the CPU), so I also ran it headed (`npx playwright test --headed`) to get the real Intel GPU. A pixel counts as wrong if any channel is off by more than 16 out of 255.

| View | SwiftShader (headless): wrong pixels | mean channel diff | Iris Plus 655 via ANGLE Metal (headed): wrong pixels | mean channel diff |
| --- | --- | --- | --- | --- |
| shadow | 8 of 57600 (0.014%) | 0.018 | 24 of 57600 (0.042%) | 0.055 |
| physical disk | 24 of 57600 (0.042%) | 0.065 | 2 of 57600 (0.003%) | 0.008 |
| interstellar uniform disk | 8 of 57600 (0.014%) | 0.021 | 4 of 57600 (0.007%) | 0.009 |

The same numbers came back on every rerun. Whole suite: 20.7 s headless, 21.1 s headed, including the three CPU renders (Playwright's own timing). The Mac table only has the first three views because the other two came later.

#### Same test on my PC

Measured on 2026-10-15 on my PC: AMD Ryzen 7 3700X, NVIDIA GeForce RTX 3060, 16 GB RAM, Windows 10 Home 10.0.19045. Native build with MinGW-w64 GCC 16.2.0 (WinLibs, UCRT), `-O3` Release through CMake 4.4.4. Chromium 156.0.8078.4 from Playwright 1.64.1, one run each.

| View | SwiftShader (headless): wrong pixels | mean channel diff | RTX 3060 via ANGLE D3D11 (headed): wrong pixels | mean channel diff |
| --- | --- | --- | --- | --- |
| shadow | 8 of 57600 (0.014%) | 0.018 | 0 of 57600 (0.000%) | 0.000 |
| physical disk | 24 of 57600 (0.042%) | 0.065 | 0 of 57600 (0.000%) | 0.021 |
| interstellar uniform disk | 8 of 57600 (0.014%) | 0.021 | 0 of 57600 (0.000%) | 0.000 |
| face on physical disk | 0 of 57600 (0.000%) | 0.000 | 4 of 57600 (0.007%) | 0.031 |
| near edge on physical disk, swung around | 11 of 57600 (0.019%) | 0.027 | 4 of 57600 (0.007%) | 0.014 |

The SwiftShader numbers for the first three views are exactly the same as on the Mac, which makes sense since it is the same software GPU. Whole suite: 22.2 s headless, 21.1 s headed, including the five CPU renders (Playwright's own timing).

### Why the thresholds are what they are

The test fails if more than 0.2% of pixels are wrong or the mean channel difference is above 0.3. Both are about 5x the worst honest result above (0.042% and 0.065), so float32 noise on a different GPU has room, but real bugs don't fit.

My first guess was 1% wrong pixels. Breaking the shader on purpose showed that was far too loose: moving the ISCO cut from 6M to 5.5M only made 556 pixels wrong (0.97%) and passed. Two other bugs, dropping the camera's own redshift and setting the exposure from X instead of Y, shift colors by a few percent everywhere, which never pushes a single pixel past 16 but moves the mean to 5.83 and 0.93. That is why there is a mean check too.

### What the pixel test catches

Each mutant was a one line change to `web/src/blackhole.frag`, run through the full test, then the file was copied back. All 10 caught with the final thresholds:

| Mutant | Views failed |
| --- | --- |
| Doppler sign flipped, (1 - Omega L) | 1 of 3 |
| ISCO cut at 5.5M | 2 of 3 |
| no sideways blueshift for the hovering camera | 3 of 3 |
| camera redshift dropped | 1 of 3 |
| acceleration -2.9 in place of -3 | 3 of 3 |
| no soft roll off, plain clamp | 2 of 3 |
| exposure from X in place of Y | 1 of 3 |
| elevation sign flipped | 2 of 3 |
| outer edge of the disk ignored | 2 of 3 |
| checkerboard squares 10 deg tall in place of 20 | 3 of 3 |

After the review I tried one more on my PC, run against the swung view only: rotating the camera's position around the spin axis but not its ray directions. It made 34966 of 57600 pixels wrong (60.7%) and failed.

Not compared with the CPU: the starfield (the CPU only has the checkerboard).

## Phase 4: controls and presets

### Control panel tests

`web/tests/controls.spec.ts`, part of `npm test`. Seven tests: the three presets set the camera and disk and the address bar follows, every slider reaches its end and every switch flips (and resolution at 25% really shrinks the canvas to 250 px across), the disk brightness slider changes the pixels on screen, a shared link opens the same settings in the panel, a messy link (blank values, an angle of 270, a 0K disk) falls back to sensible values, dragging 100 px swings the camera 30 deg and the panel follows, and on a 390 x 844 phone screen the panel starts as a closed sheet and opens. Every test also fails on any console error.

The brightness test is the one that proves a setting reaches the shader. I checked it by not sending `u_exposure` at all, and it failed.

Headless Chromium draws on SwiftShader, and a full size frame at 4000 steps took long enough that the panel never showed up inside the test timeouts. These tests are about the panel, not the picture, so they open at 1000 x 500 with `quality=0.5&steps=500`. The picture itself is still covered by the pixel test.

Measured on 2026-10-16 on my PC (AMD Ryzen 7 3700X, RTX 3060, Windows 10, Chromium 156.0.8078.4 from Playwright 1.64.1): the full web suite, 7 panel tests plus the 5 pixel views, passed in 35.1 s headless. The pixel numbers came out exactly the same as in phase 3, so the new step count and exposure settings leave the default picture alone.

### Checked by hand in a real browser

agent-browser 0.39.1, headed Chrome on my PC, WebGL on the RTX 3060 through ANGLE D3D11, 1280 x 720 canvas at full resolution. I clicked all three presets, turned every disk switch off and on, switched to the checkerboard, turned the disk off, moved the disk brightness to -3 stops, then reopened at 390 x 844 and opened the sheet. The console only had Vite's connection messages and React's DevTools tip, no warnings or errors.

### Bundle size

`npm run build` (Vite 8.3.4): the page script is 15.80 kB (6.91 kB gzip) and the panel, loaded separately so the black hole draws first, is 221.54 kB (69.23 kB gzip), almost all of it React.

## Phase 5: performance

All of these were measured on 2026-10-18 on my PC: AMD Ryzen 7 3700X, NVIDIA GeForce RTX 3060, Windows 10 Home 10.0.19045, Chromium 156.0.8078.4 (Playwright 1.64.1, headed so it uses the real GPU), WebGL2 through ANGLE Direct3D11. Vite 8.3.4 dev server. The Intel Mac numbers, which are the headline ones, are in their own section right below.

### Intel Mac (headline numbers)

Measured 2026-10-18 on my Intel Mac: Intel Core i5-8259U, Intel Iris Plus Graphics 655 (integrated), 8 GB RAM, macOS 15.7.7, plugged in. Headed Chrome 155 (the one agent-browser 0.39.1 drives), WebGL2 through ANGLE Metal. Vite 8.3.4 dev server, commit `a479e88` (same shader as phase 5's `15155d2`). Same `?bench` page and settings as the PC. This is the fast shader only, I didn't measure a before on the Mac.

| View | Size | Steps | Median ms | fps |
| --- | --- | --- | --- | --- |
| Default | 640x360 | 500 | 25.0 | 40.0 |
| Default | 640x360 | 1000 | 25.1 | 39.8 |
| Default | 640x360 | 4000 | 24.9 | 40.2 |
| Default | 1280x720 | 500 | 88.0 | 11.4 |
| Default | 1280x720 | 1000 | 87.7 | 11.4 |
| Default | 1280x720 | 4000 | 88.0 | 11.4 |
| Default | 1920x1080 | 500 | 191.5 | 5.2 |
| Default | 1920x1080 | 1000 | 192.0 | 5.2 |
| Default | 1920x1080 | 4000 | 192.1 | 5.2 |
| Edge on | 640x360 | 500 | 30.2 | 33.1 |
| Edge on | 640x360 | 1000 | 30.3 | 33.0 |
| Edge on | 640x360 | 4000 | 30.4 | 32.9 |
| Edge on | 1280x720 | 500 | 109.3 | 9.1 |
| Edge on | 1280x720 | 1000 | 108.8 | 9.2 |
| Edge on | 1280x720 | 4000 | 108.8 | 9.2 |
| Edge on | 1920x1080 | 500 | 240.2 | 4.2 |
| Edge on | 1920x1080 | 1000 | 241.0 | 4.1 |
| Edge on | 1920x1080 | 4000 | 240.5 | 4.2 |
| Interstellar | 640x360 | 500 | 32.9 | 30.4 |
| Interstellar | 640x360 | 1000 | 33.0 | 30.3 |
| Interstellar | 640x360 | 4000 | 33.1 | 30.2 |
| Interstellar | 1280x720 | 500 | 119.8 | 8.3 |
| Interstellar | 1280x720 | 1000 | 120.1 | 8.3 |
| Interstellar | 1280x720 | 4000 | 119.5 | 8.4 |
| Interstellar | 1920x1080 | 500 | 264.0 | 3.8 |
| Interstellar | 1920x1080 | 1000 | 265.0 | 3.8 |
| Interstellar | 1920x1080 | 4000 | 264.6 | 3.8 |

**60 fps at default quality on the Intel Mac: missed.** Default quality is the default view, 4000 steps and the resolution slider at 100% (one ray per CSS pixel). It got 40 fps at 640 x 360, 11.4 fps at 1280 x 720 and 5.2 fps at 1920 x 1080, so it doesn't hit 60 at any size I measured. The RTX 3060 is 9 to 14 times faster on the same rows.

Like on the PC, the step limit makes no difference here (every 500, 1000 and 4000 row is within 1% of the others), so the time goes into the steps rays actually take, not the cap. While dragging, the live page drops resolution on the Mac (any full frame over 16 ms triggers it), so moving the camera stays usable even though a still frame at full quality is slow.

### How the benchmark works

Open the page with `?bench`. It goes through three views (the default view, the Edge on preset and the Interstellar preset), three canvas sizes (640 x 360, 1280 x 720, 1920 x 1080) and three step limits (500, 1000, 4000). For each one it draws 5 untimed frames, then times 30. After every frame it reads one pixel back, which makes the browser wait until the GPU has finished, so the time is the real trace time and not the screen's 60 Hz. The table shows the median frame. Timer resolution in Chrome is 0.1 ms.

### Before and after, 1920 x 1080, 4000 steps

| View | Before | Early exit | Early exit + adaptive step | Speedup |
| --- | --- | --- | --- | --- |
| Default (30M, 8 deg above the disk) | 25.8 ms (38.8 fps) | 20.4 ms (49.0 fps) | 15.2 ms (65.8 fps) | 1.70x |
| Edge on | 33.9 ms (29.5 fps) | 23.0 ms (43.5 fps) | 16.8 ms (59.5 fps) | 2.02x |
| Interstellar | 38.8 ms (25.8 fps) | 28.1 ms (35.6 fps) | 19.1 ms (52.4 fps) | 2.03x |

### Full tables, median ms per frame (fps)

| View | Size | Steps | Before | After |
| --- | --- | --- | --- | --- |
| Default | 640x360 | 500 | 3.7 (270) | 2.7 (370) |
| Default | 640x360 | 1000 | 4.1 (244) | 2.8 (357) |
| Default | 640x360 | 4000 | 3.9 (256) | 2.7 (370) |
| Default | 1280x720 | 500 | 9.9 (101) | 7.8 (128) |
| Default | 1280x720 | 1000 | 12.4 (81) | 7.6 (132) |
| Default | 1280x720 | 4000 | 12.5 (80) | 7.6 (132) |
| Default | 1920x1080 | 500 | 20.5 (49) | 15.0 (67) |
| Default | 1920x1080 | 1000 | 25.6 (39) | 15.2 (66) |
| Default | 1920x1080 | 4000 | 25.8 (39) | 15.2 (66) |
| Edge on | 640x360 | 500 | 3.6 (278) | 2.3 (435) |
| Edge on | 640x360 | 1000 | 4.3 (233) | 2.4 (417) |
| Edge on | 640x360 | 4000 | 4.4 (227) | 2.4 (417) |
| Edge on | 1280x720 | 500 | 12.6 (79) | 7.7 (130) |
| Edge on | 1280x720 | 1000 | 15.4 (65) | 7.8 (128) |
| Edge on | 1280x720 | 4000 | 15.4 (65) | 7.9 (127) |
| Edge on | 1920x1080 | 500 | 27.7 (36) | 16.6 (60) |
| Edge on | 1920x1080 | 1000 | 33.6 (30) | 16.8 (60) |
| Edge on | 1920x1080 | 4000 | 33.9 (30) | 16.8 (60) |
| Interstellar | 640x360 | 500 | 3.7 (270) | 2.8 (357) |
| Interstellar | 640x360 | 1000 | 5.0 (200) | 2.8 (357) |
| Interstellar | 640x360 | 4000 | 5.2 (192) | 2.7 (370) |
| Interstellar | 1280x720 | 500 | 12.7 (79) | 8.8 (114) |
| Interstellar | 1280x720 | 1000 | 17.4 (57) | 8.9 (112) |
| Interstellar | 1280x720 | 4000 | 17.6 (57) | 9.0 (111) |
| Interstellar | 1920x1080 | 500 | 27.8 (36) | 18.9 (53) |
| Interstellar | 1920x1080 | 1000 | 38.4 (26) | 19.0 (53) |
| Interstellar | 1920x1080 | 4000 | 38.8 (26) | 19.1 (52) |

Before is commit `c3d96fe`. The 1000 and 4000 step rows were already almost the same before, because hardly any ray needs more than 1000 steps. After the changes the step limit barely matters at all.

### Steps per pixel

To see where the time went I swapped in a throwaway shader that writes each pixel's step count into its color, at 640 x 360 (not committed).

| View | Early exit only | Early exit + adaptive step |
| --- | --- | --- |
| Default, all pixels | 274 mean | 186 mean |
| Default, pixels that see the sky | 432 | 301 |
| Default, pixels on the disk | 161 | 111 |
| Default, pixels that fall in | 322 | 177 |
| Interstellar, all pixels | 446 | 284 |

I also tried a stronger adaptive step (growing once r is past b/2 instead of b), which got the default view down to 98 steps per pixel. I didn't keep it, see the accuracy section.

### Accuracy of the fast shader

The CPU vs GPU pixel test (now 6 views) still passes, on SwiftShader headless:

| View | Before: wrong pixels | After: wrong pixels | After: mean channel diff |
| --- | --- | --- | --- |
| shadow | 8 of 57600 | 20 of 57600 (0.035%) | 0.046 |
| physical disk | 24 of 57600 | 24 of 57600 (0.042%) | 0.065 |
| interstellar uniform disk | 8 of 57600 | 39 of 57600 (0.068%) | 0.092 |
| face on physical disk | 0 of 57600 | 0 of 57600 (0.000%) | 0.000 |
| near edge on physical disk, swung around | 11 of 57600 | 13 of 57600 (0.023%) | 0.032 |
| widest disk nearly edge on (new) | | 12 of 57600 (0.021%) | 0.028 |

The extra wrong pixels are on the checkerboard. Its squares are either one color or the other, so a sky direction that moves by 1e-4 rad flips the pixels sitting right on a square's edge. Everything stays far under the 0.2% limit (115 pixels).

The pixel test is too coarse to tell the step rules apart, so I measured the error directly with another throwaway shader that traces every pixel twice, once the old way (1% steps out to 1000M) and once the new way, and writes out the difference. 640 x 360, five views including the camera at 6M and at 200M:

| | Adaptive past b (kept) | Adaptive past b/2 (not kept) |
| --- | --- | --- |
| Pixels that end differently (sky vs disk vs hole) | 0 in every view | 2 in the default view |
| Sky direction error, median | 90 to 97 microradians | 84 to 92 microradians |
| Sky direction error, 99th percentile | 102 to 131 microradians | 101 to 122 microradians |
| Disk hit radius error, median | 0.00001 to 0.00003 M | 0.00009 to 0.00012 M |

The sky error comes almost all from early exit, which is set to stop when the bend left is under 1e-4 rad. For scale, one pixel at the narrowest field of view (20 deg) on a 1080p screen is about 1.8e-4 rad. A few pixels right on the shadow edge are off by more, where any difference grows by e^(2 pi) each time the ray laps the photon sphere.

### What the new tests catch

- The leftover bend test (`tests/test_tracer.cpp`) checks b = 10, 20 and 50 at the radius where the shader stops and at 10b and 20b. The formula was within 2.7% everywhere and within 2.1% at the shader's stopping radius. Before I added the stopping radius cases, changing the 3/4 to 1 failed all 6 checks
- The disk guard: rays heading out can only stop early once they are past the disk's outer edge. Taking the guard out made 10 more pixels wrong in the widest disk view (14 to 24, in a probe run), which the 115 pixel limit can't catch. It's one comparison and it's the right physics, so it stays, but the suite doesn't prove it
- Resolution drop while dragging: the panel test fails if the canvas never shrinks during a drag (checked by turning the drop off)

### Things I measured and didn't do

- The disk color normalizes by the hottest ring's color, which is the same for every pixel but gets worked out per pixel (41 wavelength slices). Replacing it with a constant only sped up 1080p by 1 to 7% (Default 15.2 to 14.2 ms, Edge on 16.8 to 16.6, Interstellar 19.1 to 18.6). Moving it out of the shader means a second copy of the blackbody code in TypeScript, so I left it

### Resolution while dragging

Whenever the view sits still, the live page times one full quality frame (it reads a pixel back before and after, so only that frame counts, and the very first draw after loading is left untimed since it also sets up the shader). If it took more than 16 ms, frames drawn while the view is moving use sqrt(16 / ms) of the resolution slider's value, but never less than a quarter of it, so a moving frame fits in about 16 ms. 150 ms after the view stops changing it draws at full quality again. Nothing is timed mid drag, so a fast GPU never waits on itself. On the RTX 3060 at 1080p the default view takes 15.2 ms, so nothing changes there, but Edge on (16.8 ms) and Interstellar (19.1 ms) drop a little. It matters most on slower GPUs like the Mac's.

The panel test only checks that a slow GPU (SwiftShader) drops and comes back. Nothing checks that a fast GPU doesn't drop, since the test browser doesn't have one.

### Test suite

Measured 2026-10-18 on my PC: 104 of 104 C++ assertions, and the web suite (8 panel tests and 6 pixel views) passed in 30.8 s headless.

Same day on the Intel Mac: 104 of 104 C++ assertions. I ran the two web spec files one at a time since full runs crashed the Mac back in phase 3: the 6 pixel views passed in 37.6 s and the 8 panel tests in 54.7 s.

## Phase 6: polish and ship

### README pictures

All made on 2026-10-18 on the Intel Mac with headed Playwright Chromium 156 (so the real Iris Plus 655 GPU), from the page's `?test` mode, which draws one frame at an exact size with no UI.

| File | Size | View |
| --- | --- | --- |
| `docs/images/edge-on.png` | 640 x 360 | Edge on preset: r = 50M, fov 80 deg, 2 deg above the disk |
| `docs/images/face-on.png` | 640 x 360 | Face on preset: r = 50M, fov 80 deg, 90 deg above the disk |
| `docs/images/interstellar.png` | 640 x 360 | Interstellar preset: r = 74.1M, fov 40 deg, 3.44 deg above the disk, 4500K everywhere, no shifts |
| `docs/images/checkerboard.png` | 640 x 360 | default view on the checkerboard sky |
| `docs/images/orbit.gif` | 480 x 270, 72 frames, 0.6 s each | r = 32M, fov 70 deg. Height above the disk goes 40 - 37 cos(t) degrees (3 to 77 and back) while the camera swings 30 deg either way around the hole. 256 colors per frame |
| `docs/images/live-page.png` | 1280 x 760 | the live page with its panel, default view |
| `web/public/preview.png` | 1200 x 630 | default view, the picture a shared link shows |

The frame time in the live page screenshot (126.6 ms at 1280 x 760) is what that page measured on the Mac when the picture was taken, one frame, not a median.

### Frame time in the corner

The live page now prints the time of the last full quality frame it timed (the same one the resolution drop uses: a pixel read back before and after so only that frame counts). It's a single frame, so it moves around more than the `?bench` medians.

### Bundle size

`npm run build` (Vite 8.3.4) on 2026-10-18: index.html 5.44 kB (2.17 kB gzip), the page script 17.08 kB (7.43 kB gzip), the panel 222.19 kB (69.51 kB gzip), the benchmark page 1.33 kB (0.87 kB gzip, only loaded with `?bench`). The page script was 16.88 kB (7.34 kB gzip) at the start of phase 6, so the intro and the frame time added 0.20 kB.

### Test suite

2026-10-18 on the Intel Mac: 104 of 104 C++ assertions (26 test cases). Web specs one at a time: 6 pixel views passed in 35.0 s and 9 panel tests (one new: the frame time shows up in the corner, the matching preset lights up, and Reset keeps the quality sliders) in 1.0 min. I checked the new test fails when Reset also resets the quality sliders.

### Phone

Measured 2026-10-18 on my iPhone, from the live site (`https://joshuacortes195.github.io/event-horizon/?bench`, commit `35c20b5`). The page reported the GPU as "Apple GPU" and the browser as Safari 26.6.1 (user agent `Mozilla/5.0 (iPhone; CPU iPhone OS 18_7 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.6.1 Mobile/15E148 Safari/604.1`). Same `?bench` settings as the Mac and the PC: 5 warmup frames, median of 30, a one pixel read back after every frame. The canvas is drawn at the listed size whatever the phone's screen size is.

Safari only gives the page whole milliseconds, so every median here is a whole number of ms and the fps column moves in big jumps at the small sizes (10 ms is 100 fps, 11 ms is 90.9).

| View | Size | Steps | Median ms | fps |
| --- | --- | --- | --- | --- |
| Default | 640x360 | 500 | 10 | 100.0 |
| Default | 640x360 | 1000 | 11 | 90.9 |
| Default | 640x360 | 4000 | 11 | 90.9 |
| Default | 1280x720 | 500 | 34 | 29.4 |
| Default | 1280x720 | 1000 | 35 | 28.6 |
| Default | 1280x720 | 4000 | 34 | 29.4 |
| Default | 1920x1080 | 500 | 70 | 14.3 |
| Default | 1920x1080 | 1000 | 71 | 14.1 |
| Default | 1920x1080 | 4000 | 73 | 13.7 |
| Edge on | 640x360 | 500 | 12 | 83.3 |
| Edge on | 640x360 | 1000 | 12 | 83.3 |
| Edge on | 640x360 | 4000 | 12 | 83.3 |
| Edge on | 1280x720 | 500 | 40 | 25.0 |
| Edge on | 1280x720 | 1000 | 53 | 18.9 |
| Edge on | 1280x720 | 4000 | 63 | 15.9 |
| Edge on | 1920x1080 | 500 | 100 | 10.0 |
| Edge on | 1920x1080 | 1000 | 88 | 11.4 |
| Edge on | 1920x1080 | 4000 | 90 | 11.1 |
| Interstellar | 640x360 | 500 | 14 | 71.4 |
| Interstellar | 640x360 | 1000 | 14 | 71.4 |
| Interstellar | 640x360 | 4000 | 14 | 71.4 |
| Interstellar | 1280x720 | 500 | 45 | 22.2 |
| Interstellar | 1280x720 | 1000 | 46 | 21.7 |
| Interstellar | 1280x720 | 4000 | 47 | 21.3 |
| Interstellar | 1920x1080 | 500 | 104 | 9.6 |
| Interstellar | 1920x1080 | 1000 | 105 | 9.5 |
| Interstellar | 1920x1080 | 4000 | 106 | 9.4 |

At default quality (default view, 4000 steps) the phone gets 90.9 fps at 640 x 360, 29.4 at 720p and 13.7 at 1080p. That's faster than my Intel Mac on every row (40.2, 11.4 and 5.2 fps on the same rows) and slower than the RTX 3060. The 60 fps goal was only ever for the Mac, but the phone clears 60 at 640 x 360 in all three views.

Edge on at 1280 x 720 is the odd one out: 40, 53 and 63 ms for 500, 1000 and 4000 steps, where every other view and size stays within a few ms across step limits. At 1080p the same view goes the other way (100, 88, 90 ms). I only ran it once, so I can't tell yet whether that's real or the phone throttling partway through the run.
