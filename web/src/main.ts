import fragSrc from './blackhole.frag?raw';
import { getView, setView, subscribe, toUrl } from './view';

// corners of a quad covering the whole screen, made from the vertex number so no buffers are needed.
// drawn as a triangle strip, 4 vertices make the two triangles
const vertSrc = `#version 300 es
void main() {
  vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0 - 1.0;
  gl_Position = vec4(p, 0.0, 1.0);
}`;

// the pixel test renders one fixed view from the url, the live page gets the panel and the mouse
const params = new URLSearchParams(location.search);
const testMode = params.has('test');
// ?bench times a fixed list of views and sizes instead of showing the live page
const benchMode = params.has('bench');
// one frame at 60 hz, and how long the view has to sit still before it's drawn at full quality again
const FRAME_MS = 16;
const SETTLE_MS = 150;

// grab webgl2, no antialiasing since every pixel is traced exactly once
const canvas = document.getElementById('view') as HTMLCanvasElement;
const gl = canvas.getContext('webgl2', { antialias: false });
if (!gl) throw new Error('webgl2 is not available in this browser');

// compiles one shader stage and throws with the compiler's message if it fails
function compile(type: number, src: string) {
  const shader = gl!.createShader(type)!;
  gl!.shaderSource(shader, src);
  gl!.compileShader(shader);
  if (!gl!.getShaderParameter(shader, gl!.COMPILE_STATUS)) throw new Error(gl!.getShaderInfoLog(shader) ?? 'shader compile failed');
  return shader;
}

// links the quad and the black hole shader into one program
const program = gl.createProgram();
gl.attachShader(program, compile(gl.VERTEX_SHADER, vertSrc));
gl.attachShader(program, compile(gl.FRAGMENT_SHADER, fragSrc));
gl.linkProgram(program);
if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program) ?? 'link failed');
gl.useProgram(program);
const u = (name: string) => gl.getUniformLocation(program, name);

// sends the whole view to the shader and traces every pixel once
function draw() {
  const v = getView();
  gl!.viewport(0, 0, canvas.width, canvas.height);
  gl!.uniform2f(u('u_resolution'), canvas.width, canvas.height);
  gl!.uniform1f(u('u_r'), v.r);
  gl!.uniform1f(u('u_fov'), (v.fov * Math.PI) / 180);
  gl!.uniform1f(u('u_elev'), (v.elev * Math.PI) / 180);
  gl!.uniform1f(u('u_azim'), (v.azim * Math.PI) / 180);
  gl!.uniform1f(u('u_outer'), v.disk ? v.outer : 0);
  gl!.uniform1f(u('u_peak'), v.temp);
  gl!.uniform1i(u('u_gravity'), v.redshift ? 1 : 0);
  gl!.uniform1i(u('u_doppler'), v.beaming ? 1 : 0);
  gl!.uniform1i(u('u_uniform'), v.uniform ? 1 : 0);
  gl!.uniform1i(u('u_stars'), v.stars ? 1 : 0);
  gl!.uniform1i(u('u_steps'), v.steps);
  // exposure is in stops, each one doubles the disk's brightness
  gl!.uniform1f(u('u_exposure'), 2 ** v.exposure);
  gl!.drawArrays(gl!.TRIANGLE_STRIP, 0, 4);
}

if (testMode) {
  // pixel test: render once at exactly the asked size, read it back top row first like the cpu png
  canvas.width = Number(params.get('w') ?? 320);
  canvas.height = Number(params.get('h') ?? 180);
  draw();
  const w = canvas.width, h = canvas.height;
  const raw = new Uint8Array(w * h * 4);
  gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, raw);
  const pixels = new Uint8Array(w * h * 4);
  for (let y = 0; y < h; y++) pixels.set(raw.subarray((h - 1 - y) * w * 4, (h - y) * w * 4), y * w * 4);
  // which gpu actually ran it, for the results write up
  const info = gl.getExtension('WEBGL_debug_renderer_info');
  Object.assign(window, { pixels, glRenderer: gl.getParameter(info ? info.UNMASKED_RENDERER_WEBGL : gl.RENDERER) });
} else if (benchMode) {
  // benchmark page, loaded on its own so the live page doesn't carry it
  import('./bench').then(({ runBench }) => runBench(canvas, gl, draw));
} else {
  // live page: only redraw when the view or the window changed, nothing moves on its own
  let dirty = true;
  // when the view last changed, and how long the last full quality frame took on the gpu
  let changedAt = -Infinity, fullMs = 0;
  subscribe(() => {
    dirty = true;
    changedAt = performance.now();
  });
  // one pixel read back, it makes the browser wait for the gpu to finish
  const pixel = new Uint8Array(4);
  const waitForGpu = () => gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
  const frame = () => {
    // while moving, drop the resolution if a full frame takes over 16 ms. time goes with pixel count, so scale by sqrt(16 / ms)
    const moving = performance.now() - changedAt < SETTLE_MS;
    const drop = moving && fullMs > FRAME_MS ? Math.max(0.25, Math.sqrt(FRAME_MS / fullMs)) : 1;
    // match the canvas to its size on screen, times the quality slider. the browser scales it back up
    const q = getView().quality * drop;
    const w = Math.max(1, Math.round(canvas.clientWidth * q)), h = Math.max(1, Math.round(canvas.clientHeight * q));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
      dirty = true;
    }
    if (dirty && !moving) {
      // times full quality frames once the view sits still, never mid drag
      if (fullMs === 0) {
        // the very first draw also pays for shader setup, so do one untimed
        draw();
      }
      // finish whatever's still queued so only this frame gets timed
      waitForGpu();
      const t0 = performance.now();
      draw();
      waitForGpu();
      fullMs = performance.now() - t0;
    } else if (dirty) {
      draw();
    }
    dirty = false;
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);

  // keeps the address bar in step with the view so a link opens the same picture.
  // it writes once things settle for a quarter second, not on every pixel of a drag
  let urlTimer = 0;
  subscribe(() => {
    clearTimeout(urlTimer);
    urlTimer = window.setTimeout(() => history.replaceState(null, '', toUrl(getView())), 250);
  });

  // every finger or mouse button that's down, so two fingers can pinch
  const pointers = new Map<number, { x: number; y: number }>();
  const pinchGap = () => {
    const [a, b] = [...pointers.values()];
    return Math.hypot(a.x - b.x, a.y - b.y);
  };
  let gap = 0;

  canvas.addEventListener('pointerdown', (e) => {
    pointers.set(e.pointerId, { x: e.clientX, y: e.clientY });
    canvas.setPointerCapture(e.pointerId);
    if (pointers.size === 2) gap = pinchGap();
  });

  // one finger or the mouse orbits: sideways swings around the hole, up and down tilts over the disk.
  // two fingers pinch to move closer or farther
  canvas.addEventListener('pointermove', (e) => {
    const last = pointers.get(e.pointerId);
    if (!last) return;
    const dx = e.clientX - last.x, dy = e.clientY - last.y;
    pointers.set(e.pointerId, { x: e.clientX, y: e.clientY });
    const v = getView();
    if (pointers.size === 1) {
      setView({ azim: v.azim - dx * 0.3, elev: v.elev + dy * 0.3 });
    } else if (pointers.size === 2) {
      const now = pinchGap();
      if (gap > 0 && now > 0) setView({ r: (v.r * gap) / now });
      gap = now;
    }
  });

  // finger lifted or the browser took the touch away
  const up = (e: PointerEvent) => {
    pointers.delete(e.pointerId);
    gap = pointers.size === 2 ? pinchGap() : 0;
  };
  canvas.addEventListener('pointerup', up);
  canvas.addEventListener('pointercancel', up);

  // scroll to zoom in and out, the store keeps it between 6M and 200M
  canvas.addEventListener(
    'wheel',
    (e) => {
      e.preventDefault();
      setView({ r: getView().r * Math.exp(e.deltaY * 0.001) });
    },
    { passive: false },
  );

  // the control panel is react, loaded on its own so the black hole draws before the panel is ready
  import('./panel').then(({ mountPanel }) => mountPanel(document.getElementById('panel')!));
}
