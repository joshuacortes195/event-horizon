import { defineConfig } from 'vite';

export default defineConfig({
  // relative paths, so the same build works on github pages under /event-horizon/ and locally
  base: './',
  // playwright writes its results in here mid test, which would make the dev server reload the page
  server: { watch: { ignored: ['**/test-results/**', '**/playwright-report/**'] } },
});
