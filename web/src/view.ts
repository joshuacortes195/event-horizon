// everything that decides what the black hole looks like. the panel, the mouse and the url all
// change this, and the render loop reads it every frame
export type View = {
  r: number; // camera distance from the hole, in M
  fov: number; // field of view across the width, degrees
  elev: number; // camera angle above the disk plane, degrees
  azim: number; // camera angle around the spin axis, degrees
  disk: boolean;
  outer: number; // disk outer edge, in M
  temp: number; // hottest ring temperature, kelvin
  redshift: boolean;
  beaming: boolean;
  uniform: boolean; // whole disk one temperature, the interstellar look
  stars: boolean; // starfield, or the checkerboard when off
  exposure: number; // disk brightness in stops, 0 matches the cpu
  steps: number; // most steps a ray gets
  quality: number; // traced rays per css pixel across, 1 is full resolution
};

// slider limits, shared by the panel and the url reader so a link can't go out of range
export const LIMITS = {
  r: [6, 200],
  fov: [20, 120],
  elev: [-90, 90],
  azim: [-180, 180],
  outer: [7, 60],
  temp: [2000, 20000],
  exposure: [-3, 3],
  steps: [500, 4000],
  quality: [0.25, 1],
} as const;

// what the page opens with
export const DEFAULTS: View = {
  r: 30, fov: 60, elev: 8, azim: 0,
  disk: true, outer: 20, temp: 6000,
  redshift: true, beaming: true, uniform: false,
  stars: true, exposure: 0, steps: 4000, quality: 1,
};

// one click camera and disk setups. the interstellar one copies the film team's paper:
// 74.1M away, 3.44 deg above the disk, one 4500K temperature, no redshift or beaming.
// it's an approximation, their hole spins at a = 0.6 and mine doesn't
export const PRESETS: Record<string, Partial<View>> = {
  'Edge on': { r: 50, fov: 80, elev: 2, azim: 0, disk: true, outer: 20, temp: 6000, redshift: true, beaming: true, uniform: false },
  'Face on': { r: 50, fov: 80, elev: 90, azim: 0, disk: true, outer: 20, temp: 6000, redshift: true, beaming: true, uniform: false },
  Interstellar: { r: 74.1, fov: 40, elev: 3.44, azim: 0, disk: true, outer: 20, temp: 4500, redshift: false, beaming: false, uniform: true },
};

// keeps a number inside its slider range
const clamp = (key: keyof typeof LIMITS, x: number) => Math.min(LIMITS[key][1], Math.max(LIMITS[key][0], x));

// wraps an angle back into -180 to 180 so dragging round and round never runs off the slider.
// angles already in range stay put, so the slider's own ends don't jump to the other side
export const wrapAngle = (a: number) => (a >= -180 && a <= 180 ? a : ((((a + 180) % 360) + 360) % 360) - 180);

// reads a view from the url. the pixel test uses outer=0 for no disk and look=physical|flat|uniform,
// the panel writes the plain keys
export function readUrl(search: string): View {
  const p = new URLSearchParams(search);
  const v = { ...DEFAULTS };
  for (const key of Object.keys(LIMITS) as (keyof typeof LIMITS)[]) {
    // blank or junk values keep the default instead of snapping to the slider's end
    const raw = p.get(key)?.trim();
    const x = Number(raw);
    if (raw && Number.isFinite(x)) v[key] = key === 'azim' ? wrapAngle(x) : clamp(key, x);
  }
  // outer=0 from the pixel test means no disk, keep the default size for when it's turned back on
  if (p.has('outer') && Number(p.get('outer')) <= 0) {
    v.disk = false;
    v.outer = DEFAULTS.outer;
  }
  const look = p.get('look');
  if (look) Object.assign(v, { redshift: look === 'physical', beaming: look === 'physical', uniform: look === 'uniform' });
  for (const key of ['disk', 'redshift', 'beaming', 'uniform', 'stars'] as const) if (p.has(key)) v[key] = p.get(key) !== '0';
  // a 0K disk is no disk, same as the cpu, whatever disk= says
  if (p.has('temp') && Number(p.get('temp')) <= 0) {
    v.disk = false;
    v.temp = DEFAULTS.temp;
  }
  if (p.get('sky') === 'checker') v.stars = false;
  return v;
}

// turns a view into a short url, only the settings that differ from the defaults
export function toUrl(v: View): string {
  const p = new URLSearchParams();
  for (const [key, x] of Object.entries(v) as [keyof View, View[keyof View]][]) {
    if (x === DEFAULTS[key]) continue;
    p.set(key, typeof x === 'boolean' ? (x ? '1' : '0') : String(Math.round(x * 100) / 100));
  }
  const q = p.toString();
  return q ? `?${q}` : location.pathname;
}

// tiny store: the current view plus whoever wants to hear when it changes
let current = readUrl(location.search);
const listeners = new Set<() => void>();

export const getView = () => current;

// changes some settings and tells everyone, angles and ranges kept tidy
export function setView(patch: Partial<View>) {
  const next = { ...current, ...patch };
  next.azim = wrapAngle(next.azim);
  for (const key of Object.keys(LIMITS) as (keyof typeof LIMITS)[]) if (key !== 'azim') next[key] = clamp(key, next[key]);
  current = next;
  listeners.forEach((f) => f());
}

// signs up for changes, hands back a function to sign off again
export function subscribe(f: () => void) {
  listeners.add(f);
  return () => listeners.delete(f);
}
