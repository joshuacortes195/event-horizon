import { DEFAULTS, PRESETS, setView, type View } from './view';

// the views, sizes and step counts every benchmark run goes through, same list on every machine
const VIEWS: Record<string, Partial<View>> = { Default: DEFAULTS, 'Edge on': PRESETS['Edge on'], Interstellar: PRESETS.Interstellar };
const SIZES = [[640, 360], [1280, 720], [1920, 1080]];
const STEPS = [500, 1000, 4000];
const WARMUP = 5;
const FRAMES = 30;

type Row = { view: string; w: number; h: number; steps: number; ms: number; fps: number };

// waits for the browser to breathe so the tab doesn't freeze and the page can show progress
const tick = () => new Promise((done) => setTimeout(done, 0));

// times every view, size and step count. each frame waits for the gpu to finish by reading one pixel
// back, so the time is the real trace time and not capped by the screen's 60 hz
export async function runBench(canvas: HTMLCanvasElement, gl: WebGL2RenderingContext, draw: () => void) {
  // full screen box the results table gets written into
  const out = document.createElement('pre');
  out.style.cssText = 'position:fixed;inset:0;margin:0;padding:16px;overflow:auto;color:#e8e6e3;background:#000;font:13px/1.4 monospace';
  document.body.append(out);
  const pixel = new Uint8Array(4);

  // which gpu and browser ran it, for the results write up
  const info = gl.getExtension('WEBGL_debug_renderer_info');
  const renderer = gl.getParameter(info ? info.UNMASKED_RENDERER_WEBGL : gl.RENDERER);
  const header = `gpu: ${renderer}\nbrowser: ${navigator.userAgent}\n\n| view | size | steps | median ms | fps |\n| --- | --- | --- | --- | --- |\n`;
  out.textContent = header + 'running...';

  const rows: Row[] = [];
  // every view at every size and step count, set up fresh each time
  for (const [name, view] of Object.entries(VIEWS)) {
    for (const [w, h] of SIZES) {
      for (const steps of STEPS) {
        setView({ ...DEFAULTS, ...view, steps, quality: 1 });
        canvas.width = w;
        canvas.height = h;
        // a few untimed frames first so shader compiling and clock ramp up don't count
        const times: number[] = [];
        for (let i = 0; i < WARMUP + FRAMES; i++) {
          const t0 = performance.now();
          draw();
          gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
          if (i >= WARMUP) times.push(performance.now() - t0);
          await tick();
        }
        // median frame, so one hiccup from the os doesn't move the number
        times.sort((a, b) => a - b);
        const ms = times[FRAMES >> 1];
        rows.push({ view: name, w, h, steps, ms, fps: 1000 / ms });
        // redraws the table so far as a markdown table, ready to paste
        out.textContent = header + rows.map((r) => `| ${r.view} | ${r.w}x${r.h} | ${r.steps} | ${r.ms.toFixed(2)} | ${r.fps.toFixed(1)} |`).join('\n') + '\n';
      }
    }
  }
  out.textContent += '\ndone';
  // so a script can pick the numbers up when the run ends
  Object.assign(window, { benchResults: { renderer, userAgent: navigator.userAgent, rows } });
}
