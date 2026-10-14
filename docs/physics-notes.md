# Physics notes

My notes on the physics in this project, written as I learn it. Each section says what the idea is, where the code for it lives, and what I would expect to be asked about it.

## What I am rendering

A black hole bends light. A ray that passes far away bends a little, a ray that passes close bends a lot, and a ray that gets too close falls in and never comes out. To draw what a camera would see, I run that backwards: for every pixel I send a ray out of the camera, follow its bent path, and color the pixel by whatever the ray hits. That might be the disk, a star far away, or nothing at all if it falls in, which is where the black shadow comes from.

The rest of the project is about getting each path right (the physics), checking it against numbers that are known exactly (the tests), and doing it for every pixel 60 times a second (the shader).

Sections get added below as I build each part.
