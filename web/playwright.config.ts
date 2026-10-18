import { defineConfig } from '@playwright/test';

// starts the dev server and runs the browser tests against it in headless chromium.
// never reuses a server already on the port, it could be another project's and the tests would check the wrong page
export default defineConfig({
  testDir: 'tests',
  timeout: 120_000,
  use: { baseURL: 'http://localhost:5183' },
  webServer: { command: 'npx vite --port 5183 --strictPort', url: 'http://localhost:5183', reuseExistingServer: false },
});
