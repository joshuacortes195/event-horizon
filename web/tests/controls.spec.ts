import { test, expect, type Page } from '@playwright/test';

// headless chromium draws on a software gpu, so a full size frame takes long enough to stall the page.
// these tests are about the panel, not the picture, so they open small and cheap (but wider than the 700 px phone layout)
const CHEAP = 'quality=0.5&steps=500';
test.use({ viewport: { width: 1000, height: 500 } });

// collects console errors and page crashes so every test can check the console stayed clean
function watchErrors(page: Page) {
  const errors: string[] = [];
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  page.on('pageerror', (e) => errors.push(e.message));
  return errors;
}

// the address bar's settings as an object
const urlParams = (page: Page) => Object.fromEntries(new URL(page.url()).searchParams);

// waits until the address bar has caught up with the view (it writes a quarter second after things settle).
// undefined means the setting is back at its default, so it's left out of the url
const expectUrl = (page: Page, want: Record<string, string | undefined>) =>
  expect.poll(() => Object.keys(want).map((k) => urlParams(page)[k])).toEqual(Object.values(want));

test('presets set the camera and the disk, and the url follows', async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto(`/?${CHEAP}`);

  // interstellar: the film team's camera, one temperature, no shifts
  await page.getByRole('button', { name: 'Interstellar' }).click();
  await expectUrl(page, { r: '74.1', fov: '40', elev: '3.44', temp: '4500', redshift: '0', beaming: '0', uniform: '1' });
  await expect(page.getByLabel('Doppler beaming')).not.toBeChecked();

  // edge on and face on put the physics back on
  await page.getByRole('button', { name: 'Edge on' }).click();
  await expectUrl(page, { r: '50', fov: '80', elev: '2' });
  await expect(page.getByLabel('Doppler beaming')).toBeChecked();
  await page.getByRole('button', { name: 'Face on' }).click();
  await expectUrl(page, { elev: '90' });

  expect(errors).toEqual([]);
});

test('the preset matching the view lights up, and reset goes back to the defaults but keeps quality', async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto(`/?${CHEAP}`);
  const pressed = (name: string, on: boolean) => expect(page.getByRole('button', { name })).toHaveAttribute('aria-pressed', String(on));

  // the corner shows a measured frame time once the first still frame is timed
  await expect(page.locator('#stats')).toHaveText(/ms for the last frame/);

  // picking a preset lights up only that one
  await page.getByRole('button', { name: 'Interstellar' }).click();
  await pressed('Interstellar', true);
  await pressed('Edge on', false);
  await pressed('Face on', false);

  // moving the camera off the preset turns it back off
  await page.getByLabel('Distance').focus();
  await page.getByLabel('Distance').press('ArrowRight');
  await pressed('Interstellar', false);

  // reset: camera and disk back to the defaults, the cheap quality settings stay
  await page.getByRole('button', { name: 'Reset' }).click();
  await expect(page.getByLabel('Distance')).toHaveValue('30');
  await expect(page.getByLabel('Doppler beaming')).toBeChecked();
  await expectUrl(page, { r: undefined, temp: undefined, quality: '0.5', steps: '500' });

  expect(errors).toEqual([]);
});

test('every slider reaches its ends and every switch flips', async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto(`/?${CHEAP}`);

  // pushes each slider to one end with the keyboard and checks the url picked it up
  const sliders: [string, string, 'Home' | 'End', string | undefined][] = [
    ['Distance', 'r', 'End', '200'],
    ['Above the disk', 'elev', 'End', '90'],
    ['Around the hole', 'azim', 'End', '180'],
    ['Field of view', 'fov', 'End', '120'],
    ['Outer edge', 'outer', 'End', '60'],
    ['Hottest ring', 'temp', 'End', '20000'],
    ['Disk brightness', 'exposure', 'End', '3'],
    // steps open at 500 here, the top is the default so it drops out of the url
    ['Max steps per ray', 'steps', 'End', undefined],
    ['Resolution', 'quality', 'Home', '0.25'],
  ];
  for (const [label, key, end, value] of sliders) {
    await page.getByLabel(label).focus();
    await page.getByLabel(label).press(end);
    await expectUrl(page, { [key]: value });
  }

  // resolution at 25% really shrinks the canvas, the browser stretches it back over the page
  await expect.poll(() => page.evaluate(() => document.querySelector('canvas')!.width)).toBe(250);

  // every switch, then the disk off grays out the disk controls
  const switches: [string, string][] = [
    ['Gravitational redshift', 'redshift'],
    ['Doppler beaming', 'beaming'],
    ['Starfield', 'stars'],
  ];
  for (const [label, key] of switches) {
    await page.getByLabel(label).uncheck();
    await expectUrl(page, { [key]: '0' });
  }
  await page.getByLabel('One temperature everywhere').check();
  await expectUrl(page, { uniform: '1' });
  await page.getByLabel('Accretion disk').uncheck();
  await expectUrl(page, { disk: '0' });
  await expect(page.getByLabel('Gravitational redshift')).toBeDisabled();
  await expect(page.getByLabel('Outer edge')).toBeDisabled();

  expect(errors).toEqual([]);
});

test('disk brightness actually changes the picture', async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto(`/?${CHEAP}`);
  // a strip through the disk, left of the panel
  const strip = { x: 100, y: 240, width: 500, height: 20 };
  const before = await page.screenshot({ clip: strip });
  await page.getByLabel('Disk brightness').focus();
  await page.getByLabel('Disk brightness').press('Home');
  await expectUrl(page, { exposure: '-3' });
  // the shader got the new value if the pixels changed
  await expect.poll(async () => (await page.screenshot({ clip: strip })).equals(before)).toBe(false);
  expect(errors).toEqual([]);
});

test('a shared link opens the same view in the panel', async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto(`/?${CHEAP}&r=42&elev=12.5&beaming=0&stars=0`);
  await expect(page.getByLabel('Distance')).toHaveValue('42');
  await expect(page.getByLabel('Above the disk')).toHaveValue('12.5');
  await expect(page.getByLabel('Doppler beaming')).not.toBeChecked();
  await expect(page.getByLabel('Starfield')).not.toBeChecked();
  expect(errors).toEqual([]);
});

test('a messy link falls back to sensible values', async ({ page }) => {
  const errors = watchErrors(page);
  // blank values keep the defaults, an angle past 180 wraps around, a 0K disk is no disk
  await page.goto(`/?${CHEAP}&fov=&r=abc&azim=270&temp=0&disk=1`);
  await expect(page.getByLabel('Field of view')).toHaveValue('60');
  await expect(page.getByLabel('Distance')).toHaveValue('30');
  await expect(page.getByLabel('Around the hole')).toHaveValue('-90');
  await expect(page.getByLabel('Accretion disk')).not.toBeChecked();
  expect(errors).toEqual([]);
});

test('dragging orbits the camera and the panel follows', async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto(`/?${CHEAP}`);
  // drag left to right across the black hole, away from the panel on the right
  await page.mouse.move(40, 300);
  await page.mouse.down();
  await page.mouse.move(140, 300, { steps: 5 });
  await page.mouse.up();
  // 100 px at 0.3 deg per px swings the camera 30 deg the other way
  await expect(page.getByLabel('Around the hole')).toHaveValue('-30');
  expect(errors).toEqual([]);
});

test('a slow gpu draws smaller while dragging and goes back to full quality after', async ({ page }) => {
  const errors = watchErrors(page);
  await page.goto(`/?${CHEAP}`);
  // full quality here is half of the 1000 px window. software gpu frames take way over 16 ms, so dragging should drop it
  await expect.poll(() => page.evaluate(() => (document.getElementById('view') as HTMLCanvasElement).width)).toBe(500);
  // the page keeps a list of every canvas width it sees, so slow test timing can't miss the drop
  await page.evaluate(() => {
    const c = document.getElementById('view') as HTMLCanvasElement;
    const w = window as any;
    w.widths = [];
    setInterval(() => w.widths.push(c.width), 5);
  });
  await page.mouse.move(40, 300);
  await page.mouse.down();
  await page.mouse.move(140, 300, { steps: 5 });
  await page.mouse.up();
  // once it settles it's back at full quality, and somewhere in the drag it was smaller
  await expect.poll(() => page.evaluate(() => (document.getElementById('view') as HTMLCanvasElement).width)).toBe(500);
  const smallest = await page.evaluate(() => Math.min(...(window as any).widths));
  expect(smallest).toBeLessThan(500);
  expect(errors).toEqual([]);
});

test('on a phone the panel starts as a closed sheet and opens', async ({ page }) => {
  const errors = watchErrors(page);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto(`/?${CHEAP}`);
  const handle = page.getByRole('button', { name: 'Controls' });
  await expect(handle).toHaveAttribute('aria-expanded', 'false');
  await expect(page.getByLabel('Distance')).toBeHidden();
  await handle.click();
  await expect(page.getByLabel('Distance')).toBeVisible();
  expect(errors).toEqual([]);
});
