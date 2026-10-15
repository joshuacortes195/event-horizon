import fragSrc from './blackhole.frag?raw';

// corners of a quad covering the whole screen, made from the vertex number so no buffers are needed.
// drawn as a triangle strip, 4 vertices make the two triangles
const vertSrc = `#version 300 es
void main() {
  vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0 - 1.0;
  gl_Position = vec4(p, 0.0, 1.0);
}`;

// the view, from the url if it's there (the pixel test sets every one), otherwise the defaults
const params = new URLSearchParams(location.search);
const num = (key: string, fallback: number) => (params.has(key) ? Number(params.get(key)) : fallback);
const look = params.get('look') ?? 'physical';
const view = {
  r: num('r', 30),
  fov: num('fov', 60),
  elev: num('elev', 8),
  azim: num('azim', 0),
  outer: num('outer', 20),
  temp: num('temp', 6000),
};
const testMode = params.has('test');

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

// the settings that don't change while dragging: disk, look and sky
gl.uniform1f(u('u_outer'), view.outer);
gl.uniform1f(u('u_peak'), view.temp);
gl.uniform1i(u('u_gravity'), look === 'physical' ? 1 : 0);
gl.uniform1i(u('u_doppler'), look === 'physical' ? 1 : 0);
gl.uniform1i(u('u_uniform'), look === 'uniform' ? 1 : 0);
gl.uniform1i(u('u_stars'), params.get('sky') === 'checker' ? 0 : 1);

// traces every pixel once for the current camera
function draw() {
  gl!.viewport(0, 0, canvas.width, canvas.height);
  gl!.uniform2f(u('u_resolution'), canvas.width, canvas.height);
  gl!.uniform1f(u('u_r'), view.r);
  gl!.uniform1f(u('u_fov'), (view.fov * Math.PI) / 180);
  gl!.uniform1f(u('u_elev'), (view.elev * Math.PI) / 180);
  gl!.uniform1f(u('u_azim'), (view.azim * Math.PI) / 180);
  gl!.drawArrays(gl!.TRIANGLE_STRIP, 0, 4);
}

if (testMode) {
  // pixel test: render once at exactly the asked size, read it back top row first like the cpu png
  canvas.width = num('w', 320);
  canvas.height = num('h', 180);
  draw();
  const w = canvas.width, h = canvas.height;
  const raw = new Uint8Array(w * h * 4);
  gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, raw);
  const pixels = new Uint8Array(w * h * 4);
  for (let y = 0; y < h; y++) pixels.set(raw.subarray((h - 1 - y) * w * 4, (h - y) * w * 4), y * w * 4);
  // which gpu actually ran it, for the results write up
  const info = gl.getExtension('WEBGL_debug_renderer_info');
  Object.assign(window, { pixels, glRenderer: gl.getParameter(info ? info.UNMASKED_RENDERER_WEBGL : gl.RENDERER) });
} else {
  // live page: only redraw when the camera or the window changed, nothing moves on its own
  let dirty = true;
  const frame = () => {
    // match the canvas to its size on screen.
    // one traced ray per css pixel, retina gets upscaled. phase 5 adds resolution scaling
    if (canvas.width !== canvas.clientWidth || canvas.height !== canvas.clientHeight) {
      canvas.width = canvas.clientWidth;
      canvas.height = canvas.clientHeight;
      dirty = true;
    }
    if (dirty) draw();
    dirty = false;
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);

  // drag to orbit: sideways swings around the hole, up and down tilts over the disk
  let last: PointerEvent | null = null;
  canvas.addEventListener('pointerdown', (e) => {
    last = e;
    canvas.setPointerCapture(e.pointerId);
  });
  canvas.addEventListener('pointermove', (e) => {
    if (!last) return;
    view.azim -= (e.clientX - last.clientX) * 0.3;
    view.elev = Math.min(90, Math.max(-90, view.elev + (e.clientY - last.clientY) * 0.3));
    last = e;
    dirty = true;
  });
  canvas.addEventListener('pointerup', () => (last = null));

  // scroll to zoom in and out, never closer than the isco at 6M or farther than 200M
  canvas.addEventListener(
    'wheel',
    (e) => {
      e.preventDefault();
      view.r = Math.min(200, Math.max(6, view.r * Math.exp(e.deltaY * 0.001)));
      dirty = true;
    },
    { passive: false },
  );
}
