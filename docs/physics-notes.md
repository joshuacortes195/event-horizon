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
- What does "non-spinning" leave out? A spinning hole is the Kerr metric. It drags space around with it and makes the shadow lopsided. I left it out of scope.

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
