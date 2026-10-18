import { test, expect } from '@playwright/test';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

// the native render tool from the cmake build, the cpu side of the comparison
const RENDER = join(import.meta.dirname, '../../build/render' + (process.platform === 'win32' ? '.exe' : ''));

// a channel off by more than this counts the pixel as wrong.
// small differences are just float32 rounding in the color math
const CHANNEL_TOLERANCE = 16;
// most pixels any view is allowed to get wrong, and the most the average channel may be off.
// both about 5x the worst real float32 mismatch measured, see results/README.md for why
const MAX_WRONG_FRACTION = 0.002;
const MAX_MEAN_DIFF = 0.3;

// views that cover the shadow, the lensed sky, the physical disk and the flat one temperature disk.
// azim only goes to the gpu: 40 degrees swings the sky exactly two checker squares and the disk is
// round, so the picture has to come out the same as the cpu's azim 0 one
const VIEWS = [
  { name: 'shadow', w: 320, h: 180, r: 15, fov: 60, elev: 0, outer: 0, temp: 0, look: 'flat' },
  { name: 'physical disk', w: 320, h: 180, r: 30, fov: 60, elev: 10, outer: 20, temp: 6000, look: 'physical' },
  { name: 'interstellar uniform disk', w: 320, h: 180, r: 74.1, fov: 40, elev: 3.44, outer: 20, temp: 4500, look: 'uniform' },
  { name: 'face on physical disk', w: 320, h: 180, r: 30, fov: 60, elev: 90, outer: 20, temp: 6000, look: 'physical' },
  { name: 'near edge on physical disk, swung around', w: 320, h: 180, r: 50, fov: 50, elev: 2, outer: 20, temp: 6000, look: 'physical', azim: 40 },
  // biggest disk the slider allows seen nearly edge on, where rays the shader stops early could still cross the far edge
  { name: 'widest disk nearly edge on', w: 320, h: 180, r: 40, fov: 120, elev: 0.5, outer: 60, temp: 6000, look: 'physical' },
];

for (const v of VIEWS) {
  test(`gpu matches the cpu tracer: ${v.name}`, async ({ page }) => {
    // fail on any console error or shader problem
    const errors: string[] = [];
    page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
    page.on('pageerror', (e) => errors.push(e.message));

    // render the view on the cpu
    const dir = mkdtempSync(join(tmpdir(), 'eh-'));
    const png = join(dir, 'cpu.png');
    const args = [v.w, v.h, v.r, v.fov, v.elev, v.outer, v.temp, v.look].map(String);
    execFileSync(RENDER, [png, ...args]);
    const cpuPng = readFileSync(png).toString('base64');
    // clean up the temp folder once the png is read
    rmSync(dir, { recursive: true, force: true });

    // render the same view on the gpu, the page puts the finished pixels on window.pixels
    const q = new URLSearchParams({ test: '', sky: 'checker', ...Object.fromEntries(Object.entries(v).map(([k, x]) => [k, String(x)])) });
    await page.goto(`/?${q}`);
    await page.waitForFunction(() => (window as any).pixels, null, { timeout: 60_000 });

    // decode the cpu png in the browser with no color management, and grab both images as rgba
    const { gpu, cpu, renderer } = await page.evaluate(async (b64) => {
      const blob = await (await fetch(`data:image/png;base64,${b64}`)).blob();
      const bmp = await createImageBitmap(blob, { colorSpaceConversion: 'none', premultiplyAlpha: 'none' });
      const ctx = new OffscreenCanvas(bmp.width, bmp.height).getContext('2d')!;
      ctx.drawImage(bmp, 0, 0);
      const w = window as any;
      return { gpu: Array.from(w.pixels as Uint8Array), cpu: Array.from(ctx.getImageData(0, 0, bmp.width, bmp.height).data), renderer: w.glRenderer as string };
    }, cpuPng);
    expect(gpu.length).toBe(cpu.length);

    // count pixels where any channel is off by more than the tolerance
    let wrong = 0, sum = 0;
    for (let i = 0; i < cpu.length; i += 4) {
      let worst = 0;
      for (let c = 0; c < 3; c++) {
        const d = Math.abs(gpu[i + c] - cpu[i + c]);
        worst = Math.max(worst, d);
        sum += d;
      }
      if (worst > CHANNEL_TOLERANCE) wrong++;
    }
    const pixels = cpu.length / 4;
    const mean = sum / (3 * pixels);
    console.log(`${v.name}: ${wrong} of ${pixels} pixels wrong (${((100 * wrong) / pixels).toFixed(3)}%), mean channel diff ${mean.toFixed(3)}, on ${renderer}`);

    expect(errors).toEqual([]);
    expect(wrong / pixels).toBeLessThan(MAX_WRONG_FRACTION);
    expect(mean).toBeLessThan(MAX_MEAN_DIFF);
  });
}
