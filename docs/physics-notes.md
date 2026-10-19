# Physics notes

My notes on the physics in this project, written as I learn it. Each section says what the idea is, where the code for it lives, and what I would expect to be asked about it.

## What I am rendering

A black hole bends light. A ray that passes far away bends a little, a ray that passes close bends a lot, and a ray that gets too close falls in and never comes out. To draw what a camera would see, I run that backwards: for every pixel I send a ray out of the camera, follow its bent path, and color the pixel by whatever the ray hits. That might be the disk, a star far away, or nothing at all if it falls in, which is where the black shadow comes from.

The rest of the project is about getting each path right (the physics), checking it against numbers that are known exactly (the tests), and doing it for every pixel 60 times a second (the shader).

## Units: G = c = 1

Physicists pick units where the gravitational constant G and the speed of light c are both 1. Then a mass is also a length (GM / c^2 is a length), so I measure every distance in units of M, the black hole's mass. For a black hole as heavy as the Sun, M is about 1.5 km.

The nice part is that the important radii become small round numbers: the horizon is at 2M, the photon sphere at 3M and the inner edge of the disk at 6M. In the code M is just 1, so "r < 2.0" literally means "inside the horizon".

Code: the comment at the top of `core/tracer.h`.

Interview questions:
- Why can you measure distance in units of mass? Because once G and c are 1, GM / c^2 is a length.
- How big is 1M for a real black hole? About 1.5 km per solar mass, so Sagittarius A* (about 4 million suns) has M of about 6 million km.

## The Schwarzschild metric

A metric is the rule for measuring distances and times. Around a non-spinning, uncharged mass it is

```
ds^2 = -(1 - 2M/r) dt^2 + dr^2 / (1 - 2M/r) + r^2 (dtheta^2 + sin^2(theta) dphi^2)
```

The factor (1 - 2M/r) does all the work. Close to the hole, clocks tick slower (the dt part shrinks) and radial distances stretch (the dr part grows). At r = 2M the factor hits zero. That is the event horizon: from inside it, not even light gets back out.

Interview questions:
- What happens at r = 2M? Nothing that falls in can get out, light included. The metric looks broken there in these coordinates, but that is a coordinate problem, a falling observer feels nothing special at the horizon.
- What does "non-spinning" leave out? A spinning hole is the Kerr metric. It drags space around with it and makes the shadow lopsided. I added it later as phase 7, see the Kerr sections at the end.

## Geodesics and the ray equation

In general relativity gravity is not a force. Things move along the straightest possible lines through curved spacetime, called geodesics. Light follows null geodesics, where ds^2 = 0.

Two things stay constant along a light ray in Schwarzschild, because the metric does not change with time or with angle: the energy E = (1 - 2M/r) dt/dl and the angular momentum L = r^2 dphi/dl (l is the "affine parameter", the natural clock along a light ray). Putting those into ds^2 = 0 gives

```
(dr/dl)^2 = E^2 - (1 - 2M/r) L^2 / r^2
```

and taking the derivative of both sides gives

```
d2r/dl2 = L^2 / r^3 - 3M L^2 / r^4
```

Here is the trick I use. Pretend the ray moves in flat 3D space, with position x and velocity v = dx/dl, pulled toward the center by some acceleration a(r). For any pull toward the center, h = |x cross v| stays constant, and in polar coordinates the radial motion is d2r/dl2 = h^2 / r^3 + a. Matching that with the line above (with h = L) says the pull has to be a = -3M h^2 / r^4, or as a vector

```
acceleration = -3M h^2 x / r^5
```

So a flat 3D ray with that pull traces exactly the same r(l) and phi(l) as real light in Schwarzschild. Light always stays in one plane through the center, so working in x, y, z just means I never have to set up that plane by hand. The equation is only about 10 lines in C++ and will be the same in GLSL.

I first wrote this as -1.5 h^2 x / r^5 in my plan. That is only right when M = 1/2 (when the horizon is at r = 1). The research check caught it before I wrote any code.

Code: `accel` in `core/tracer.cpp`.

Interview questions:
- Is this Newtonian gravity? No. Newton's pull falls off as 1/r^2 and does not depend on h. This one goes as 1/r^4 times h^2, which is exactly the extra term relativity adds. With Newton's gravity, light bends only half as much (2M/b instead of 4M/b).
- Why is h conserved? The pull points straight at the center, so it can't twist the ray around. Same reason a planet's angular momentum is conserved.
- What do you lose with this trick? Coordinate time t. I know where the ray goes and in what order, but not how long the trip takes for a far away clock. Rendering a still image doesn't need it.

## Impact parameter

Picture a ray coming in from very far away. Its impact parameter b is how far it would miss the center if gravity were switched off. It equals L / E, so it is constant along the ray, and a single number decides the ray's fate.

For a ray that escapes, b and the closest approach r0 are tied together by

```
b = r0 / sqrt(1 - 2M / r0)
```

which comes from setting dr/dl = 0 in the equation above. Because of the square root, r0 is always a bit less than b: gravity pulls the ray in closer than its straight line would go.

Code: `incoming` in `tests/test_tracer.cpp` builds a ray with a given b. `min_r` in `TraceResult` records the closest approach.

Interview questions:
- Why can you reduce a whole ray to one number? Energy and angular momentum are both conserved, and the path shape only depends on their ratio.
- What is the bending angle for a far away ray? About 4M / b. The first test of general relativity (Eddington, 1919) measured it for starlight passing the Sun.

## Photon sphere and the critical impact parameter

At r = 3M light can circle the hole. Gravity there is exactly strong enough to bend a sideways ray into a circle. But the orbit is unstable, like a ball on a hilltop: a ray a tiny bit inside falls in, a ray a tiny bit outside flies off.

A ray coming in from far away grazes the photon sphere when b = 3 sqrt(3) M, about 5.196M. Rays with smaller b fall in, rays with bigger b escape. That is what draws the black shadow.

The instability can be measured. Writing u = 1/r, the path follows u'' + u = 3M u^2 (with ' meaning d/dphi). Nudging it a little off u = 1/(3M) gives delta'' = delta, so any error grows like e^phi, which is a factor of e^(2 pi), about 535, per orbit. In my tracer the error grew by 537.5 from the first orbit to the second. It also means that even with perfect steps, a double precision ray can only stay near 3M for about six orbits before roundoff kicks it off, and a float32 ray on the GPU for under three. In practice the RK4 step error (about 1e-6 per orbit at my step size) is bigger than roundoff, so my rays leave after about two.

Code: the photon sphere tests in `tests/test_tracer.cpp`, and `max_steps` in `trace` as a safety net.

Interview questions:
- Why is the shadow bigger than the horizon? Because a ray doesn't need to aim at the horizon to fall in, it just needs b below 5.196M. Seen from far away the shadow's radius is 5.196M, more than 2.5 times the horizon's.
- What are the thin rings at the edge of the shadow? Rays that went around the hole an extra half orbit, a full orbit and so on before escaping. Each ring is half an orbit further around than the one outside it, so it is about e^pi, roughly 23 times, thinner. Only the first couple are visible.

## RK4

To move a ray forward by a small step, RK4 (fourth order Runge-Kutta) works out the acceleration four times across the step (at the start, twice at the middle and at the end) and takes a weighted average. Its error per step shrinks like step^5, so over the whole path the error shrinks like step^4. Halve the step and the error drops 16 times. My test checks exactly that and gets 16.0.

The step size is 0.01 r, so it is small near the hole where the path curves a lot and big far away where the ray is almost straight. A ray stops when r < 2M (captured) or when r > 1000M and it is heading outward (escaped). In this flat picture the sideways pull on a passing ray falls off as 1 / r^4, so past R the bending that is left is only about (3/4) M b^3 / R^4, way under a pixel. (Integrated along the whole line, that same pull adds up to exactly 4M / b for a far away ray.)

Code: `rk4_step` and `trace` in `core/tracer.cpp`.

Interview questions:
- Why not Euler (pos += vel * dt)? Euler's error only shrinks like the step size, and near the photon sphere every error gets multiplied by 535 per orbit. RK4 at the same step is many orders of magnitude more accurate for four times the work.
- Why does the step grow with r? Far away the ray barely bends, so big steps cost nothing in accuracy. A ray going out to 1000M takes a few hundred steps instead of tens of thousands.

## The camera: a static observer

The camera hovers at a fixed r, held in place by rockets, which is called a static observer. It measures angles with its own rulers and clocks, so a direction the camera measures is not the same as the direction of the ray's velocity in my x, y, z.

The fix is one line. Light falling down to the hovering camera arrives blueshifted by 1 / sqrt(1 - 2M / r), which scales the whole local direction up by that factor. Radial distances are stretched by the same factor, which cancels it for the radial part, so only the sideways part keeps it. In the camera's own frame a pixel's direction has a radial part n_r and a sideways part n_t. The ray's starting velocity is n_r along the radial direction plus n_t / sqrt(1 - 2M / r) sideways. That keeps the ray's energy at exactly 1 and gives it the right impact parameter, b = r sin(alpha) / sqrt(1 - 2M / r).

Put b = 3 sqrt(3) M in that and you get the shadow's size as seen by the camera:

```
sin(alpha) = (3 sqrt(3) M / r) * sqrt(1 - 2M / r)
```

(Synge, 1966.) My test walks out from the center of the screen to find where rays stop falling in and matches this to 1e-5. Without the sqrt(1 - 2M / r) fix the shadow's angular radius at r = 6M comes out 50.8 degrees instead of 45, and the test fails.

Code: `camera_ray` in `core/render.cpp`.

Interview questions:
- Why does the observer need to be "static"? Angles depend on how the camera is moving (aberration). A falling camera would see a smaller shadow. Fixing the observer makes the shadow size a known exact number to test against.
- What happens if the camera is inside r = 3M? The shadow covers more than half the sky, so alpha goes past 90 degrees. My camera only looks forward, so I keep it outside 3M.

## Coloring a pixel

For every pixel I build the camera ray, trace it, and color it. A captured ray is black. An escaped ray looks up the sky in the direction it is flying when it leaves, which for now is a checkerboard so the lensing is easy to see.

Code: `render` and `checker_sky` in `core/render.cpp`, `tools/render.cpp` writes the PNG. The image and its settings are in `results/`.

## The accretion disk

Gas falling toward the hole doesn't drop straight in. It has some sideways motion, so it ends up circling in a flat disk and slowly spirals inward as friction (really magnetic turbulence) drains its orbital energy. That lost energy heats the gas and it glows. I model it as a thin, flat disk in the y = 0 plane, spinning counterclockwise when seen from +y.

Code: `core/disk.{h,cpp}`, the disk options are `Disk` in `core/render.h`.

## ISCO: where the disk stops

In Newtonian gravity you can circle as close to a mass as you want. Around a black hole there's a closest circular orbit that is still stable, at r = 6M, the innermost stable circular orbit (ISCO). Inside it the smallest nudge sends you spiraling into the hole, so gas there plunges in instead of orbiting. That's why my disk has a hole in the middle from 2M to 6M.

The photon sphere at 3M is a different thing. It's where light can orbit, and that orbit is unstable for light too.

Interview questions:
- Why does the disk have an inner edge? Below 6M there's no stable circular orbit, so gas falls in instead of piling up in a disk.
- What changes for a spinning hole? The ISCO moves in for gas orbiting the same way as the spin (down to 1M for a maximal spin) and out for gas orbiting the other way. See "ISCO with spin" near the end.

## Thin disk temperature

The disk is hottest where the most orbital energy is being lost per area. The standard thin disk result is T^4 proportional to r^-3 (1 - sqrt(6M / r)). The bracket is zero at the ISCO, because there's no torque on the inner edge (nothing further in for it to rub against). So the disk is cold at 6M, peaks at 49/36 of the ISCO (8.1667M) and cools as r^(-3/4) far out.

I scale it so the hottest ring is whatever peak temperature I choose (`disk_temperature(r, peak_temp)`).

Tests: zero at 6M, hottest at 8.1667M, and the ratio between 10000M and 20000M is 2^(3/4).

Interview questions:
- Why isn't the disk hottest right at the inner edge? The zero torque boundary at the ISCO kills the heating there.
- Is this the fully relativistic profile? No, it's the Newtonian thin disk with the ISCO boundary. The full version (Page and Thorne) adds relativistic corrections that change the numbers but not the shape much. I picked the one I can explain and test.

## Blackbody color

Anything hot glows with a spectrum set only by its temperature (Planck's law). Cool is dull red, around 6500K looks white, hotter looks blue. To get a color I walk the visible spectrum (380 to 780 nm in 10 nm steps), weigh Planck's curve by the CIE color matching functions (how the eye's three cone responses add up), and get CIE XYZ. The matching functions are a fit by Wyman, Sloan and Shirley (2013) made of a few lopsided gaussians, so I don't need the big tables. XYZ goes to linear sRGB with the standard matrix.

Code: `blackbody_xyz` in `core/disk.cpp`, the XYZ to sRGB step is `disk_color` in `core/render.cpp`.

Test: the (x, y) chromaticity of 3000K, 4500K, 6500K and 10000K lands within 0.002 of the CIE Planckian locus. At 1000K the fit is off by about 0.011, so that check is looser.

Interview question: how do you turn a temperature into an RGB color? Integrate Planck's spectrum against the color matching functions to get XYZ, then a 3x3 matrix to linear RGB, then gamma.

## g: redshift and Doppler beaming

g is the ratio of the frequency the camera sees to the frequency the gas sent out. Two things go into it:

- Gravitational redshift: light loses energy climbing out of the well. Between the gas at r and my camera at r_obs that's sqrt(1 - 2M/r) / sqrt(1 - 2M/r_obs)
- Doppler: the gas orbits at angular speed Omega = r^-1.5 (exactly Kepler, even in Schwarzschild). Gas moving toward the camera blueshifts its light, gas moving away redshifts it, and the gas's clock runs slow because it's moving (that's the sqrt(1 - 3M/r) part)

Put together: g = sqrt(1 - 3M/r) / (sqrt(1 - 2M/r_obs) (1 + Omega L)), where L is my traced ray's angular momentum about the spin axis. I trace rays backwards from the camera, so a negative traced L means the light actually left the gas heading the same way the gas moves, which is the approaching side. L never changes along the ray, so I read it off the camera ray.

For the toggles I split it into a gravity part and a Doppler part: g_grav = sqrt(1 - 2M/r) / sqrt(1 - 2M/r_obs) and g_dop = sqrt(1 - 3M/r) / sqrt(1 - 2M/r) / (1 + Omega L). Multiplied together they give the full g.

The big deal is brightness. A blackbody at temperature T seen with shift g looks exactly like a blackbody at gT (because I_nu / nu^3 doesn't change along a ray). Total brightness goes as T^4, so it scales as g^4. On my edge on test view, two rays at the same height on opposite sides land at r = 10.75M with g = 1.227 on the left and 0.685 on the right, and their visible brightness differs by about 20 times. That's why one side of the disk glows. On screen it looks more like 2 to 3 times, because my exposure roll off and the sRGB gamma curve both squeeze bright and dim values together. The beaming comes from the color of a hotter blackbody, I don't multiply by g^4 separately.

Code: `disk_g` in `core/disk.cpp`, used in `render` in `core/render.cpp`.

Tests: face on from far away g = sqrt(1 - 3M/r) (0.7071 at 6M), gravity alone at 6M is sqrt(2/3) seen from far away and sqrt(2/3) / sqrt(13/15) seen from 15M, light sent straight along the gas's motion matches the special relativity Doppler shift sqrt((1 + v) / (1 - v)) with the orbital speed v = 1 / sqrt(r - 2M) a hovering observer measures, the approaching side is above 1 and the receding side below, and with both off g = 1. The render test checks the left (approaching) half of an edge on disk is more than 1.5x as bright as the right half, and the halves match with the shifts off.

Interview questions:
- Why is one side of the disk brighter? Doppler beaming. The gas there moves toward us at a decent fraction of light speed, so its light is blueshifted, and brightness goes as g^4.
- Why is even the face on disk redshifted when nothing moves toward or away from you? Gravitational redshift plus time dilation of the orbiting gas. Both make the gas's clock tick slower than mine.
- Why can you just use a blackbody at gT? I_nu / nu^3 is conserved along a ray, and a Planck spectrum at T shifted by g is exactly a Planck spectrum at gT.

## Hitting the disk

Each RK4 step, I check if the ray's y changed sign. If it did, I find where along the step it crossed y = 0 with a straight line between the two points, and if that crossing is between 6M and the outer edge, the ray hit the disk. The step there is 1% of r, so the straight line is a good guess: face on test rays land within 6e-6 of the ring they should.

A ray that starts exactly on the plane (a camera sitting in the disk plane) is leaving it, not hitting it, so that first step doesn't count. Review caught this: a camera at 15M in the plane painted the whole top half of the image as disk.

Code: the crossing check in `trace` in `core/tracer.cpp`.

## Tilting the camera

The camera started on the +z axis, which is inside the disk plane, so everything was edge on. Now it has an elevation angle. I build the ray in the camera's own frame like before, then rotate the position and direction together about the x axis by the elevation. 0 is edge on, pi/2 is looking straight down the spin axis.

Test: from far up the spin axis, rays with the impact parameter of the 10M ring land on the 10M ring whether they leave the screen center going left, right, up or down. A round shadow wouldn't catch a broken tilt, because the hole looks the same from every direction. A second test checks every tilted ray still has energy exactly 1, using (dr/dl)^2 + (1 - 2M/r) h^2 / r^2 = E^2.

## The Interstellar look

I checked what the film team did in their paper (James, von Tunzelmann, Franklin and Thorne 2015). They turned off Doppler and gravitational shifts completely and painted the whole disk one temperature, 4500K, because the real look (one side blazing, the other dim and red) would confuse the audience. Their camera was at 74.1M and 3.44 degrees above the disk. My `uniform` disk option does the same thing. Their hole spins at a/M = 0.6 and mine doesn't, so the shadow and the disk's inner edge are a bit different.

Interview question: is the Interstellar black hole accurate? The lensing is, but they turned off the redshift and beaming on purpose. A real disk at that angle would be much brighter on one side.

## Moving to the GPU: fragment shaders

A fragment shader is a small program the GPU runs once for every pixel, thousands at the same time. Each copy only knows where its own pixel is (`gl_FragCoord`) and a few shared values I hand it, called uniforms: camera distance, angle, field of view, disk settings. It hands back one color.

Ray tracing fits this perfectly because every pixel's light ray is independent. The CPU tracer walks the pixels one at a time. The shader traces all of them at once, which is the only reason this can run in real time.

Code: `web/src/blackhole.frag` is the same math as `core/tracer.cpp`, `core/disk.cpp` and `core/render.cpp`, function for function, so they're easy to compare side by side.

Interview question: why is ray tracing a good fit for the GPU? No pixel needs anything from any other pixel, so nothing waits on anything else.

## The full screen quad

WebGL only runs the fragment shader on pixels covered by a triangle. So I draw two triangles that cover the whole screen (a quad), and the shader runs once per pixel. The four corners come from the vertex number inside the vertex shader (`gl_VertexID`), so there are no vertex buffers at all, just `drawArrays(TRIANGLE_STRIP, 0, 4)`.

Code: `vertSrc` and `draw()` in `web/src/main.ts`.

Interview question: why not three.js? There is no 3D scene, just one rectangle and a shader. Raw WebGL2 is about 40 lines and I can explain every call.

## float32 on the GPU vs double on the CPU

The CPU tracer uses 64 bit doubles, about 16 significant digits. Shaders use 32 bit floats, about 7. The math is the same, but small rounding differences add up over hundreds of RK4 steps. Almost every pixel comes out identical. The ones that don't are knife edge cases: rays balanced on the photon sphere at the rim of the shadow, rays landing right on a checkerboard edge, rays hitting right at the ISCO.

So the GPU can't be held to an exact match. The test allows a small number of wrong pixels and a small average difference, set from what I actually measured (numbers in `results/README.md`).

Interview question: how do you test a GPU renderer? Against a slow, trusted reference, with a tolerance that comes from measuring what float32 really gets wrong, then checking that real bugs land way outside it.

## Orbiting the camera

The CPU camera only tilts up (elevation). The browser camera also swings around the spin axis (azimuth): after the tilt, I rotate the position and direction together about the y axis. Dragging sideways changes the azimuth, dragging up and down changes the elevation, scrolling moves the camera closer or farther (kept between 6M and 200M). The pixel test checks the azimuth with a trick: the GPU camera swings 40 deg, which is exactly two checkerboard squares, and the disk is round, so the picture has to match the CPU one that never swung.

Spinning around the y axis doesn't change a ray's angular momentum about the y axis, so the L the Doppler shift uses still comes straight off the camera ray.

## The starfield

The sky is procedural, made from the direction the ray escapes in, so there are no image files. I chop the sky into small cells, a few cells get a star at a random spot inside them, and the brightness falls off fast from the star's center. Two layers: lots of faint stars and a few bright ones. The random numbers come from a hash of the cell position, so the same direction always gets the same star.

Because each star is looked up by the escape direction, lensing comes for free: stars behind the hole get smeared into arcs around the shadow and show up twice, once on each side.

The checkerboard sky is still there for the pixel test, because the CPU tracer only has the checkerboard.

## The control panel: uniforms

A uniform is a value I hand to the shader that's the same for every pixel in one draw, like the camera distance or whether beaming is on. Uniforms are the only way the page talks to the shader. Every frame where something changed, `draw()` in `web/src/main.ts` sends the whole view as uniforms and runs the shader once more.

The view lives in a tiny store in `web/src/view.ts`. The panel, dragging, pinching and the URL all change the store, and the render loop reads it. React only draws the panel. It never touches WebGL, and the loop never waits on React. When the store changes it marks the frame dirty, and the next animation frame redraws. Nothing redraws when nothing changed, so a still black hole costs nothing.

The address bar follows the store (only the settings that differ from the defaults), so a link opens the same view.

Interview question: why keep React out of the render loop? React is for the panel's buttons and sliders. The picture is one draw call with uniforms, so running it through React's rendering would only add work.

## Exposure in stops

The disk brightness slider (`exposure` in the URL) multiplies the disk's light by 2 to the power of the slider before the soft roll off and the gamma curve. One stop is double the light. It only touches the disk, the stars stay the same. At 0 the shader matches the CPU, so the pixel test still passes.

It's there because of what I found in phase 2: the approaching side carries about 20 times the light of the receding side, but the roll off and gamma squeeze that to about 2.8 times on screen. Turning the exposure down to -3 stops keeps the bright side out of the roll off, and the difference gets much easier to see (left side white blue, right side brown on the edge on preset).

## Step count and quality

These are the two knobs on how much work the GPU does.

- Max steps per ray: how many RK4 steps a ray gets before the shader gives up. The top is 4000, about six laps of the photon sphere. A ray that runs out counts as captured, so fewer steps can only add black pixels, never wrong colors. The ones that run out first are the rays circling near the photon sphere, at the rim of the shadow
- Resolution: how many rays per screen pixel across. At 50% the shader traces a quarter as many rays and the browser scales the picture up

Phase 5 measured what each one costs, see the performance notes below.

## The presets

- Edge on: 50M away, 2 deg above the disk, physical look. The camera I use for the beaming numbers
- Face on: 50M away, straight over the pole. The disk is a ring, and there's almost no beaming because the gas moves across the line of sight
- Interstellar: the film team's camera from James, von Tunzelmann, Franklin and Thorne 2015 (74.1M, 3.44 deg above the disk, field of view 40 deg), one 4500K temperature and no redshift or beaming, because that's what the film did. Their hole spins at a = 0.6 and mine doesn't, so the shadow isn't the same shape

## What makes the shader slow

The shader runs once per pixel and every pixel walks its own light ray, so the work per frame is pixels times steps per ray times the cost of one RK4 step (four gravity calls). A 1080p frame is about 2 million rays. There's a GPU catch too: pixels run in small groups that move in lockstep, so a group is only as fast as its slowest ray. One ray circling the photon sphere holds up its neighbors.

So there are two ways to speed it up: fewer rays (resolution) or fewer steps per ray (early exit and a smarter step size). Before changing anything I measured steps per pixel, and sky rays were the expensive ones. They walked all the way out to 1000M even though they stop bending long before that.

## Early exit

Far from the hole, the sideways pull on a ray falls off as 1 / r^4 (see the ray equation notes), so the bend a ray still has left past radius r is about (3/4) M b^3 / r^4. That's tiny fast: for b = 20 it's under 1e-4 rad by r = 88M. So the shader stops a ray once it's heading out and that leftover bend is under 1e-4 rad, about half a pixel at the narrowest zoom on a 1080p screen, and reads the sky from its direction right there.

Two details:
- b is the ray's angular momentum h = |x cross v|. The pull always points at the hole, so h never changes, and because the camera gives every ray energy 1, h is exactly the impact parameter. The shader works it out once per pixel
- A ray heading out only gets stopped once it's past the disk's outer edge. Heading out, r only grows, so after that it can't hit the disk anymore. Without this a ray could skip the far side of a big disk

I checked the 3/4 b^3 / r^4 formula on the CPU first: trace the same ray once stopping at R and once at 10 million M, and the difference in bend matches it within about 3%.

## Adaptive step size

Steps were always 1% of r. That's already adaptive in a way (small near the hole, big far away), but far from the hole compared to b the path is almost a straight line, and 1% is way more careful than it needs to be. Now the step grows by r / b once the ray is farther out than b, capped at 10% of r.

The reason this is safe: a ray's closest approach r0 is always inside b, because b = r0 / sqrt(1 - 2M / r0) is bigger than r0. So the part of the path that decides whether the ray falls in, gets caught near the photon sphere or not, always runs at the old small steps. The bigger steps only happen on the way in and the way out where the path is nearly straight. Rays that fall in have no closest approach, but their b is under 3 sqrt(3) M, about 5.2M, so near the hole they're inside r = b and keep the small steps too. The cap is there for rays aimed almost straight at the hole, where b is near 0 and the step would blow up and could hop over the horizon.

RK4's error per step goes as the step size to the fifth power, so bigger steps do cost accuracy. I measured it instead of guessing: tracing every pixel both ways, no pixel ended up somewhere different (sky vs disk vs hole), and disk hits moved by about 0.00002M. A stronger version that grew the step from b/2 saved more steps but flipped 2 pixels, so I kept the safer one.

## Dropping resolution while dragging

Time per frame goes with the number of pixels. The page times one full quality frame on the GPU, and if it took longer than 16 ms (one frame at 60 Hz) it scales the resolution by sqrt(16 / ms) while you drag, since scaling width and height both by s cuts the pixels by s^2. When you stop it draws full quality again. On my RTX 3060 at 1080p the default view fits in 16 ms so nothing changes, but Edge on (16.8 ms) and Interstellar (19.1 ms) don't, so it drops a little there too. It matters most on slower GPUs.

Interview questions:
- Why not just lower the step count? Because steps aren't where most rays end. Hardly any ray needs more than 1000 steps, so the slider barely changes the time. The waste was in the steps each ray took, not the cap
- How do you know the speedups didn't change the physics? The CPU tracer in double precision is the reference, the pixel test compares against it, and I measured direction and disk hit errors pixel by pixel
- Why 1e-4 rad? It's under a pixel at the narrowest zoom on a 1080p screen, and the measured errors sit right at it

## A spinning black hole: the Kerr metric

Real black holes spin. A spinning, uncharged hole is described by the Kerr metric, found by Roy Kerr in 1963. Its spin is a number a from 0 (Schwarzschild) up to M (the fastest it can spin), and in my units that's 0 to 1. My slider stops at 0.99.

Three things change compared to the non-spinning hole:

- Frame dragging. The spin drags space around with it, so light and gas going around with the spin (prograde) have an easier time than light going against it (retrograde)
- The horizon shrinks, from 2M down toward 1M: r+ = M + sqrt(M^2 - a^2). At a = 0.6 it's 1.8M, at 0.9 it's 1.436M
- Close in, nothing can hover. Inside the ergosphere (out to 2M in the disk plane) even a rocket can't stay still, it gets dragged around. The page keeps my camera at 6M or more and the render tool at more than 3M, both outside it

The spin axis is +y in my code, the same axis the disk already orbited around, and the disk orbits the same way the hole spins.

Code: `core/kerr.{h,cpp}`.

Interview questions:
- What does the spin do to the picture? The shadow gets flattened on one side and shifts sideways, and the disk's inner edge moves in, so the disk gets hotter and closer to the hole.
- Why does a stop at M? Past that there'd be no horizon, a "naked singularity", which isn't thought to happen in nature.

## Kerr-Schild coordinates

The usual way to write the Kerr metric (Boyer-Lindquist coordinates) breaks at the poles and at the horizon. My face on preset puts the camera right over the pole, so that was out. Instead I use Kerr-Schild coordinates, which are plain x, y, z far away and stay smooth at the poles and through the horizon. In them the metric is flat space plus one extra piece:

```
g = (flat space) + f l l
f = 2 M r^3 / (r^4 + a^2 y^2)
l = (1, (r x - a z) / (r^2 + a^2), y / r, (r z + a x) / (r^2 + a^2))
```

l is the direction light falls in along, swirled around the spin axis, and f says how strong the curving is. The catch is that r isn't the plain distance anymore. It's the solution of (x^2 + z^2) / (r^2 + a^2) + y^2 / r^2 = 1, which is the plain distance on the spin axis and sqrt(x^2 + y^2 + z^2 - a^2) in the disk plane. With a = 0 it's all plain distance again, f is 2M / r and everything reduces to Schwarzschild. (Visser 2007, arXiv:0706.0622, has the derivation.)

Code: `kerr_radius` and `field` in `core/kerr.cpp`.

Test: r is the plain distance on the axis, sqrt(R^2 - a^2) in the plane, and solves the equation anywhere else.

## Tracing light with a Hamiltonian

For Schwarzschild I had one neat equation for the acceleration. Kerr has no symmetry that makes that work, so I switched to Hamilton's equations, the general way to follow a geodesic. The ray carries its position x and its momentum p (the lowered index kind). Its energy is 1, so p_t = -1 the whole way, and

```
H = 1/2 (|p|^2 - 1 - f (1 + l . p)^2)
dx/dlambda = dH/dp      dp/dlambda = -dH/dx
```

H is 0 for light. I worked out dH/dp and dH/dx by hand (the slopes of r, f and l . p in x, y, z) and step them with the same RK4 as before. The step size is the same 1% of r too, except close to the horizon (see below).

Two things never change along a ray, and the tests use both: H stays 0 (it's still light), and the angular momentum about the spin axis, L = z p_x - x p_z, stays the same because the hole looks the same from every angle around its axis.

Code: `kerr_hamiltonian`, `kerr_derivs`, `kerr_step`, `kerr_trace` in `core/kerr.cpp`.

Tests: my hand derived slopes match central differences of H to 1e-6. On a ray that whips around the hole to 1.567M at a = 0.9, H stays under 3.4e-7 and L moves by under 1.6e-7. At a = 0.99, going in to 1.172M, it's 1.5e-5 and 2.6e-6.

## Smaller steps near the horizon

At a = 0.99 the prograde light orbit is at 1.168M and the horizon at 1.141M, only 0.03M apart. Steps of 1% of r (about 0.012M) were too coarse there: on a ray that circles close to that orbit, H drifted to 9e-3. The fix is to step by 1% of the smaller of r and 3 times the gap to the horizon. Past 1.5 times the horizon radius (3M without spin) nothing changes, and with spin 0 nothing in there gets back out anyway. At a = 0.99 the same ray's drift drops to 1.5e-5, under a tenth of a pixel. Stepping by 1 times the gap got it to 2e-7, but with about three times the steps, and the shader only gets 4000.

The first version had a bug: a falling ray's step kept shrinking with the gap, so it crept up on the horizon forever without crossing it and burned through the 100000 step limit (the answer was still "captured", but one 120 x 80 test image took 22 s instead of about 1). The step now never goes under 1e-4.

Code: `kerr_step_size` in `core/kerr.cpp`.

Interview questions:
- Why the Hamiltonian form? It works for any metric without working out the Christoffel symbols, and gives me a built in check (H = 0) for free.
- Why not keep using the old equation with spin 0? I do. Spin 0 runs the old tracer, untouched, and a test checks the new one gives the same rays at spin 0.

## Running time backward flips the spin

I trace rays backward, from the camera out into the scene. Without spin that didn't matter, since the non-spinning hole looks the same with time running backward. A spinning hole doesn't: run the film backward and it spins the other way. So my backward ray is exactly a normal forward light ray around a hole with spin -a. That's why `field` uses -a (so l has r x + a z and r z - a x in the code), and it's also what makes the horizon work: the forward ray falls through the horizon smoothly in these coordinates, while tracing backward in the same coordinates would pile up at the horizon forever.

One side effect: the sky comes out turned around the spin axis by about 2a / r_cam compared to other coordinate choices (about 3 degrees at a = 0.9, r = 30M). That's the same as moving the camera that far around the hole, which doesn't change anything since the hole looks the same from every angle around its axis.

## The camera around a spinning hole

The camera is still a static observer, hovering at Boyer-Lindquist radius r, so "distance 15M" means the same thing the formulas mean. To get its frame I take three seed directions (around the spin axis, at the hole, and up), drop the parts along its clock and along the earlier ones using the curved metric (Gram-Schmidt), and normalize. Around the spin axis has to come first. That direction is exactly the hole's symmetry direction, and taking it out first is what makes "forward" point exactly down the radius.

A pixel's light is then the camera's clock direction plus the pixel direction, lowered with the metric and scaled so the energy is 1.

Code: `kerr_launch` in `core/kerr.cpp`, `kerr_camera_ray` in `core/render.cpp`.

Tests: at spin 0 the start, velocity, final direction and disk hit match the old camera and tracer to 1e-6 or better. Every pixel of a tilted camera at a = 0.9 launches light (H under 1e-12), which only works if the frame is really orthonormal in the curved metric. At a = 0.9 the middle pixel of a camera above the disk plane has no theta push (p_theta under 1e-12), so it really is looking straight down the radius. With the forward seed not leaning in by r / sqrt(r^2 + a^2) that test fails.

## The lopsided shadow

Without spin, light can orbit at 3M and the critical impact parameter is 3 sqrt(3) M in every direction. With spin, light orbiting in the disk plane with the spin can get closer before it's caught, and light against the spin gets caught farther out:

```
r_photon = 2M (1 + cos((2/3) arccos(-+a)))      prograde, retrograde
b = 6M cos((1/3) arccos(-+a)) -+ a
```

At a = 0.9 that's 1.558M with b = 2.844 prograde and 3.910M with b = 6.832 retrograde. At a = 1 it's b = 2 and 7. So the shadow's prograde edge is pulled in (the flattened side) and the retrograde edge pushed out. The flattened side is the same side as the bright half of the disk, since both are where things move toward me.

For a static observer in the disk plane, where on screen each edge sits comes from the metric:

```
sin(alpha) = (b (r - 2M) + 2 a M) / (r sqrt(r^2 - 2 M r + a^2))
```

with b positive going with the spin. At a = 0 that's Synge's formula again.

Tests: walking out from the screen center at a = 0.5 and 0.9, the edge ray's L matches b to 1e-5 on both sides, and the edge angles match the formula to 1e-5 at 6M, 15M and 40M. Far from the hole, prograde light bends less: at b = 300M and a = 0.9 the difference between the two sides matches 8a / b^2 + 20 pi a / b^3 (Sereno and De Luca 2006) within 1%.

Interview questions:
- Why is the shadow flattened on one side? Light going around with the spin can orbit closer (down to 1M at full spin) so the edge on that side moves in.
- How did you test it? The edge rays' angular momentum has to be exactly the critical b on each side, and their angle on screen has to match the formula for a hovering observer.

## ISCO with spin

The innermost stable circular orbit moves in with spin for gas orbiting the same way, from 6M at a = 0 to 4.233M at 0.5, 3.829M at 0.6, 2.321M at 0.9 and 1M at full spin (Bardeen, Press and Teukolsky 1972). So my disk starts closer in.

For the temperature I kept the same thin disk formula and just move its inner edge, T^4 proportional to r^-3 (1 - sqrt(r_isco / r)), hottest at 49/36 of r_isco. The proper relativistic version for a spinning hole (Novikov-Thorne, Page-Thorne) changes the numbers. I picked the one I can explain and test, and this is the simple profile, not the real one.

Code: `isco_r` in `core/kerr.cpp`, `disk_temperature(r, peak, r_in)` in `core/disk.cpp`.

Tests: 6 at 0, 1 at 1, values at 0.5 and 0.9, and the orbit's energy per unit mass is lowest right at r_isco (that's what makes it the last stable one). Looking straight down at a = 0.9, the first disk hit going out from the center lands within 2% of the ISCO, and its radius is the Kerr-Schild r of the crossing point, not the plain distance (7% more there).

## Redshift with spin

The gas orbits at Omega = 1 / (r^1.5 + a), a bit slower than Kepler with spin, and its clock runs at 1 / u^t with

```
u^t = (r^1.5 + a) / (r^0.75 sqrt(r^1.5 - 3 r^0.5 + 2a))
g = 1 / (u^t sqrt(-g_tt at the camera) (1 + Omega L))
```

At a = 0 this is exactly the old g.

The gravity and Doppler toggles needed a new split. Without spin the gravity part was "a hovering emitter at r", but inside 2M nothing can hover and at high spin the disk reaches in there. So the gravity part is now the clock of an observer swept around with space at r (a ZAMO, zero angular momentum observer), alpha = sqrt(Delta r^2 / A), which exists all the way in to the horizon and is sqrt(1 - 2M / r) without spin. The Doppler part is the rest. With both on, g is exact either way.

Code: the `a != 0` branch of `disk_g` in `core/disk.cpp`.

Tests: face on from far away g = 1 / u^t, with u^t worked out from normalizing the orbit in the metric. The gas's Omega, read back out of g, satisfies the circular orbit condition d/dr (g_tt + 2 Omega g_tphi + Omega^2 g_phiphi) = 0. The gravity part matches alpha^2 = -g_tt + g_tphi^2 / g_phiphi, and the camera's clock uses theta measured from the spin axis.

## What the Kerr tests catch

I broke the code on purpose one piece at a time and ran the tests: spin pointing the wrong way, the forward seed not leaning in, a missing part of the momentum slope, the non-spinning lapse in place of the ZAMO one, the retrograde ISCO, the camera at the plain distance r instead of Boyer-Lindquist r, a typo in u^t, the camera's clock ignoring its height, a wrong slope of r, no swirl in l, the camera's up not made perpendicular to forward, Kepler's Omega in the Doppler part, the disk hit measured as plain distance, and no minimum step. Every one fails at least one test. My first tests let three through (the lean, the lapse and the camera's clock) and a review found three more (the up direction, Omega and the hit radius), which is why the theta, lapse, camera clock, every pixel, circular orbit and crossing point tests exist.
