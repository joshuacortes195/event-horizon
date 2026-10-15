import { defineConfig } from '@playwright/test';

// starts the dev server and runs the browser tests against it in headless chromium
export default defineConfig({
  testDir: 'tests',
  timeout: 120_000,
  use: { baseURL: 'http://localhost:5179' },
  webServer: { command: 'npx vite --port 5179 --strictPort', url: 'http://localhost:5179', reuseExistingServer: true },
});
