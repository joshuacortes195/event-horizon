# Plan

Event Horizon is a real-time black hole renderer that runs in the browser. Light rays bend around a Schwarzschild (non-spinning) black hole, so you see the black shadow, the starfield lensed around it and an accretion disk that is brighter on one side.

It is a portfolio project. I care more about physics checked against known values, measured performance and a clean demo than about a long feature list.

## Decisions

I made these before writing any physics code.

| Decision | Choice | Why |
| --- | --- | --- |
| Repo | Its own repo, not a mode inside gravity-sandbox | It is a separate project with its own README and demo. I copy the sandbox's build setup (CMake, doctest, Vite) instead of sharing code with it |
| Units | G = c = 1, distances in units of M (the black hole's mass) | Standard in relativity. The horizon sits at 2M, the photon sphere at 3M and the ISCO at 6M, so the numbers stay small and easy to check |
| Ray math | Step each light ray in plain x, y, z with acceleration -3M h^2 x / r^5, where h is the ray's angular momentum, using RK4 | Exact for light in Schwarzschild. About 10 lines, identical in C++ and GLSL, and the disk crossing is just y changing sign. I derive it from the metric in my notes |
| Two renderers | A slow C++ CPU tracer in double precision is the source of truth. The GLSL shader has to match it | The CPU version is easy to test against textbook numbers. The shader is tested against the CPU version |
| Camera | A static observer at a fixed distance, with ray directions set in that observer's own frame | Then the shadow size can be checked against the exact formula for a static observer, not just "looks round" |
| Sky | Procedural stars computed from the escape direction, plus a checkerboard debug sky | No assets and no license. Same result on CPU and GPU, and the checkerboard makes the lensing easy to see and test |
| Disk | Thin disk in the equatorial plane from the ISCO (6M) outward, standard thin disk temperature profile (zero at the ISCO, falling as r^(-3/4) far out), colored as a blackbody | One formula I can explain and test. The full relativistic profile (Page-Thorne) is harder to verify and to explain |
| Disk look | Physical by default: Doppler beaming and gravitational redshift on, toggles to turn them off | This is the real look. The Interstellar preset copies what the film team did (James, von Tunzelmann, Franklin and Thorne 2015, checked in phase 2): no Doppler or gravitational shifts at all, one temperature (4500K) across the whole disk, camera at r = 74.1M and 3.44 degrees above the disk plane. Their black hole spins (a/M = 0.6) and mine doesn't, so it is the same idea, not the same picture |
| WebGL | Raw WebGL2: one full screen quad (two triangles) and a fragment shader. No three.js | A quad does not need a 3D engine. Much smaller bundle, and I can explain every GL call |
| Control panel | React, only for the panel | Same pattern as the sandbox |
| CPU vs GPU test | Playwright drives headless Chromium, renders a fixed view, reads the pixels back and compares them with the CPU image | One command anyone can run, part of the test suite |
| Particle mode | Optional. The sandbox's N-body core drops particles around the hole, drawn on top without lensing | Lensing every particle is a research-grade problem. I will say plainly that the overlay is an approximation, and drop the mode if it costs too much time |
| Machines | This Intel Mac for development and most numbers. My PC and a phone as extra data points | The Mac has the weakest GPU, so it sets the bar |
| Frame rate goal | 60 fps at the default quality on the Intel Mac | High quality presets can drop below. Whatever I measure gets recorded, hit or miss |
| Tests | doctest, vendored as one header in `third_party/` | No package manager and no network needed to build |
| Hosting | GitHub Pages, deployed by a GitHub Actions workflow | Free, lives next to the code, no extra account. I switched from Netlify before phase 1 |
| PNG output | stb_image_write, one public domain header in `third_party/` | The standard tiny PNG writer, nothing to build |

## Out of scope

- Spinning (Kerr) black holes, unless I ask for it as a stretch goal after phase 6
- A thick or volumetric disk, disk turbulence, jets
- Light travel time effects across the disk, multiple frequencies per pixel
- A real Milky Way photo as the sky
- WebGPU, unless phase 5 shows WebGL2 cannot hit the goal
- Accounts, saving views, any backend

## Layout

```
core/         C++ reference tracer (phase 1)
tests/        native tests (doctest)
third_party/  doctest header, later a PNG writer
tools/        native CLI that renders reference images (phase 1)
web/          Vite + TypeScript app, the shader and the panel (phase 3)
docs/         this plan and my physics notes
results/      reference images, measured numbers and how each was measured
```

## Known values the tests check

All in G = c = 1 with M the black hole's mass.

| Quantity | Value | Phase |
| --- | --- | --- |
| Event horizon radius | 2M | 1 |
| Photon sphere radius | 3M | 1 |
| Critical impact parameter | 3 * sqrt(3) * M, about 5.196M. Rays aimed closer are captured, rays aimed farther escape | 1 |
| Closest approach r0 of an escaping ray | b = r0 / sqrt(1 - 2M / r0) | 1 |
| Weak bending angle | about 4M / b when b is much bigger than M | 1 |
| Shadow size for a static observer at distance r | sin(alpha) = (3 * sqrt(3) * M / r) * sqrt(1 - 2M / r) | 1 |
| ISCO | 6M | 2 |
| Orbital speed of the disk | angular velocity sqrt(M / r^3) | 2 |
| Hottest point of the thin disk | r = (49 / 36) * 6M, about 8.17M | 2 |
| Redshift seen face on, from far away | g = sqrt(1 - 3M / r), so 1 / sqrt(2) at the ISCO | 2 |

The research subagent double checks each value and its source before I write the test for it.

## Milestones

One phase per session. Each phase ends with tests passing, a reviewer pass on the diff, a commit, a breakdown update and a handoff file.

### Phase 0: plan and setup (done)

- Scope questions answered, decisions above
- Repo skeleton: CMake, doctest, an empty test runner
- Rules and handoff files outside the repo

Done when: `cmake --build build && ctest --test-dir build` passes and this plan is committed.

### Phase 1: CPU geodesic tracer (done)

Concepts to teach first: G = c = 1 units, the Schwarzschild metric, geodesics, the impact parameter, the photon sphere, RK4.

- `core/`: a ray state (position, velocity), the acceleration -3M h^2 x / r^5, one RK4 step, and a trace function that runs until the ray falls in (r below about 2M) or escapes (far away and moving outward)
- Camera: a static observer at distance r with a field of view, turning each pixel into a starting ray in its own frame
- Tests, written first with tdd:
  - rays with b below 3 * sqrt(3) * M are captured, rays above it escape, checked just either side of it
  - a ray started sideways at r = 3M stays near 3M for a while (the photon sphere)
  - the closest approach of an escaping ray matches b = r0 / sqrt(1 - 2M / r0)
  - far away rays bend by about 4M / b
  - the edge of the rendered shadow matches the static observer formula
- `tools/render`: renders a small black hole shadow image on the checkerboard sky and saves it as a PNG in `results/`

Done when: all of the above pass and the shadow image is in `results/` with the settings used to make it.

### Phase 2: accretion disk on the CPU (done)

Concepts to teach first: the ISCO, the thin disk temperature profile, blackbody color, gravitational redshift, Doppler beaming.

- Disk crossing: when a ray's y changes sign between steps and the crossing point is between 6M and the outer edge, it hits the disk
- Temperature from the thin disk formula, observed temperature = g times emitted temperature, color and brightness from a blackbody at the observed temperature
- g from the disk's orbital speed and the ray's conserved angular momentum, with toggles for beaming and redshift
- Tests: ISCO at 6M, hottest point near 8.17M, face-on g = sqrt(1 - 3M / r), the approaching side has g above 1 and the receding side below, and with both toggles off the disk is symmetric
- Reference images in `results/`: face on, edge on and the Interstellar angle, with and without beaming

Done when: tests pass and the reference images are saved with their settings.

What changed while building it:
- The camera needed an elevation angle. It sat on +z inside the disk plane, so every view was edge on
- The ISCO test checks rays just inside and just outside the 6M ring (b = 6.88 falls through, b = 6.98 lands) instead of aiming at the ring itself, because a ray aimed right at the edge lands on either side of it depending on the RK4 step error
- The beaming render test only counts disk pixels. The checkerboard sky isn't mirror symmetric and swamped the comparison
- Added a `uniform` disk option (one temperature everywhere) for the Interstellar look
- Review found a camera sitting in the disk plane counted its own position as a disk hit. Fixed, with a test
- Review also found the g tests never checked the size of the Doppler shift or the camera's own redshift, and nothing checked the color pipeline. Added tests for both and checked them with mutants

### Phase 3: shader port (done)

Concepts to teach first: what a fragment shader is, the full screen quad, why float32 on the GPU differs from double on the CPU.

- `web/`: Vite + TypeScript, raw WebGL2 setup, the same math in GLSL, uniforms for the camera, mouse orbit camera, procedural starfield
- Playwright test: render the same view on CPU and GPU at the same size and check the pixel difference stays under a threshold. The threshold and why it is that size go in `results/`

Done when: the page renders, the pixel test passes and the console is clean.

What changed while building it:
- Headless Chromium runs WebGL on SwiftShader, a software GPU, not the Intel chip. The test passes on both, and I recorded both in `results/`
- My first threshold (1% wrong pixels) let a broken ISCO cut through. The final test allows 0.2% wrong pixels and also checks the mean difference, because some bugs shift every color a little without making any single pixel wrong
- The shader caps a ray at 4000 steps where the CPU allows 100000. Rays that would need more are circling the photon sphere and count as captured
- The dev server reloaded the page whenever Playwright wrote its results into `web/`, so the test never got its pixels. Vite now ignores that folder
- Headed Chrome asked for a favicon that wasn't there and logged a 404, so the page has an empty inline icon

### Phase 4: controls and presets (done)

- React panel: camera distance and angle, disk on or off, beaming and redshift toggles, step count and quality sliders
- Presets: edge on, face on, Interstellar angle
- Touch orbit controls for phones
- Optional: particle mode with the sandbox's N-body core, drawn unlensed on top

Done when: every control and preset works in the browser with a clean console, checked with agent-browser.

What changed while building it:
- Particle mode dropped for good
- Extra controls I decided on up front: field of view, disk size and temperature, starfield or checkerboard, exposure in stops (0 matches the CPU), and an address bar that follows the view
- The step slider is a uniform that breaks out of the loop, with the old 4000 as the hard cap. Quality scales the canvas, the browser scales it back up
- On phones the panel is a bottom sheet that starts closed. One finger orbits, two fingers pinch
- A Playwright panel test runs with `npm test` as well as the agent-browser check, so the check can be repeated
- The azimuth slider's right end jumped to the left end (180 wrapped to -180), caught by the panel test. Angles only wrap once they leave -180 to 180
- After review: blank URL values keep the defaults instead of snapping to a slider end, a 0K disk is always off, the exposure slider is called disk brightness because it only scales the disk, the disk sliders gray out with the disk off, and the panel test now proves a setting reaches the pixels instead of only reading the URL

### Phase 5: performance (done)

- Measure first: fps at several resolutions and step counts on the Intel Mac, recorded in `results/`
- Then adaptive step size (big steps far from the hole), early ray exit, resolution scaling
- Measure again with the same settings, record before and after

Done when: before and after tables are in `results/` with machine, browser, resolution, step count and build settings. 60 fps at default quality on the Intel Mac is the goal, and the result gets recorded either way.

What changed while building it:
- The Mac crashed on full runs in phase 3, so the first before and after tables are from my PC (RTX 3060). The benchmark is a page, `?bench`, so the Mac run is just opening it and copying the table
- Measuring steps per pixel first showed the step limit wasn't the problem. Sky rays spent most of their steps far from the hole where they barely bend
- Early exit stops a ray once the bend it has left is under 1e-4 rad, and only once it's past the disk. I checked the leftover bend formula on the CPU before using it
- The adaptive step grows past r = b. I tried a stronger one (past b/2) and measured both, and kept the one where no pixel changes fate
- Resolution scaling only kicks in while dragging and only if a full frame takes over 16 ms
- The Intel Mac run came last, on the fast shader only. Default quality got 40 fps at 640 x 360, 11.4 at 720p and 5.2 at 1080p, so the 60 fps goal on the Mac is a miss, recorded in `results/`
- Taking the per pixel blackbody normalization out of the shader was measured at 1 to 7% and not worth a second copy of the code
- The browser tests had been allowed to reuse any server on their port, and another project's dev server was sitting on it. They now get their own port and never reuse one

### Phase 6: polish and ship

- UI pass (impeccable)
- README with screenshots, gifs and the numbers table
- Numbers from my PC and a phone
- GitHub Pages deploy, final breakdown update

Done when: the live demo is up and the README links to it.

## How numbers get measured

Every metric goes in `results/` with the machine, browser and version, resolution, step count and build flags. No estimates.
