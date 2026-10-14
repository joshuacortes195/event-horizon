# Event Horizon

A real-time black hole renderer for the browser. Light rays bend around a non-spinning black hole, so you see its black shadow, the stars lensed around it, and an accretion disk that is brighter on the side spinning toward you.

Work in progress. The plan is in [docs/plan.md](docs/plan.md) and my notes on the physics are in [docs/physics-notes.md](docs/physics-notes.md).

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```
