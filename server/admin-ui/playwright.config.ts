import { defineConfig } from '@playwright/test';
import { fileURLToPath } from 'url';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export default defineConfig({
  testDir: './tests',
  timeout: 45000,
  fullyParallel: false,
  workers: 1,
  reporter: 'list',
  use: {
    baseURL: 'http://127.0.0.1:8000',
    channel: 'msedge',
    headless: true,
    screenshot: 'only-on-failure',
  },
  webServer: {
    command: 'cmd /c ".venv\\Scripts\\python.exe -m uvicorn app.main:app --host 127.0.0.1 --port 8000"',
    cwd: path.resolve(__dirname, '..'),
    url: 'http://127.0.0.1:8000/admin',
    reuseExistingServer: true,
    timeout: 30000,
  },
});
