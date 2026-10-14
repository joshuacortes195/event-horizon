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
