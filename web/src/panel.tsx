import { useState, useSyncExternalStore } from 'react';
import { createRoot } from 'react-dom/client';
import { getView, setView, subscribe, LIMITS, PRESETS, type View } from './view';

type NumKey = keyof typeof LIMITS;
type BoolKey = 'disk' | 'redshift' | 'beaming' | 'uniform' | 'stars';

// one labeled slider tied to a number in the view, shows its value with units
function Slider({ k, label, step, unit, fmt, view, disabled }: { k: NumKey; label: string; step: number; unit: string; fmt?: (x: number) => string; view: View; disabled?: boolean }) {
  const [min, max] = LIMITS[k];
  return (
    <label className={disabled ? 'row off' : 'row'}>
      <span className="name">{label}</span>
      <span className="value">{fmt ? fmt(view[k]) : view[k]}{unit}</span>
      <input type="range" min={min} max={max} step={step} value={view[k]} disabled={disabled} onChange={(e) => setView({ [k]: Number(e.target.value) })} />
    </label>
  );
}

// one on/off switch tied to a true/false setting in the view
function Toggle({ k, label, view, disabled }: { k: BoolKey; label: string; view: View; disabled?: boolean }) {
  return (
    <label className={disabled ? 'toggle off' : 'toggle'}>
      <input type="checkbox" checked={view[k]} disabled={disabled} onChange={(e) => setView({ [k]: e.target.checked })} />
      {label}
    </label>
  );
}

// rounds slider readouts so they don't show a pile of decimals
const one = (x: number) => x.toFixed(1);
const signed = (x: number) => (x > 0 ? '+' : '') + x.toFixed(1);

// the whole control panel: presets, camera, disk, look and quality
function Panel() {
  // re-renders whenever the view changes, from here, the mouse or a preset
  const view = useSyncExternalStore(subscribe, getView);
  // on phones the panel is a sheet that starts closed so the black hole gets the whole screen
  const [open, setOpen] = useState(() => !matchMedia('(max-width: 700px)').matches);
  const noDisk = !view.disk;

  return (
    <aside className={open ? 'panel open' : 'panel'}>
      <button className="handle" aria-expanded={open} onClick={() => setOpen(!open)}>
        {open ? 'Hide controls' : 'Controls'}
      </button>

      <div className="body">
        <section>
          <h2>Presets</h2>
          <div className="presets">
            {Object.entries(PRESETS).map(([name, preset]) => (
              <button key={name} onClick={() => setView(preset)}>{name}</button>
            ))}
          </div>
        </section>

        <section>
          <h2>Camera</h2>
          <Slider k="r" label="Distance" step={0.1} unit=" M" fmt={one} view={view} />
          <Slider k="elev" label="Above the disk" step={0.1} unit="°" fmt={one} view={view} />
          <Slider k="azim" label="Around the hole" step={1} unit="°" fmt={(x) => x.toFixed(0)} view={view} />
          <Slider k="fov" label="Field of view" step={1} unit="°" fmt={(x) => x.toFixed(0)} view={view} />
        </section>

        <section>
          <h2>Disk</h2>
          <Toggle k="disk" label="Accretion disk" view={view} />
          <Slider k="outer" label="Outer edge" step={1} unit=" M" fmt={(x) => x.toFixed(0)} view={view} disabled={noDisk} />
          <Slider k="temp" label="Hottest ring" step={100} unit=" K" fmt={(x) => x.toFixed(0)} view={view} disabled={noDisk} />
          <Toggle k="redshift" label="Gravitational redshift" view={view} disabled={noDisk} />
          <Toggle k="beaming" label="Doppler beaming" view={view} disabled={noDisk} />
          <Toggle k="uniform" label="One temperature everywhere" view={view} disabled={noDisk} />
        </section>

        <section>
          <h2>Look</h2>
          <Slider k="exposure" label="Disk brightness" step={0.1} unit=" stops" fmt={signed} view={view} />
          <Toggle k="stars" label="Starfield (off shows the checkerboard)" view={view} />
        </section>

        <section>
          <h2>Quality</h2>
          <Slider k="steps" label="Max steps per ray" step={100} unit="" fmt={(x) => x.toFixed(0)} view={view} />
          <Slider k="quality" label="Resolution" step={0.25} unit="%" fmt={(x) => (x * 100).toFixed(0)} view={view} />
        </section>
      </div>
    </aside>
  );
}

// puts the panel on the page
export function mountPanel(el: HTMLElement) {
  createRoot(el).render(<Panel />);
}
