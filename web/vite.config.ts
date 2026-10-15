import { defineConfig } from 'vite';

export default defineConfig({
  // playwright writes its results in here mid test, which would make the dev server reload the page
  server: { watch: { ignored: ['**/test-results/**', '**/playwright-report/**'] } },
});
