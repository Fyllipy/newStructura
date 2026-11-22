import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  base: '/admin/',
  server: {
    port: 5173,
    proxy: {
      '/admin/auth': 'http://127.0.0.1:8000',
      '/admin/licenses': 'http://127.0.0.1:8000',
      '/admin/devices': 'http://127.0.0.1:8000',
      '/admin/abuse': 'http://127.0.0.1:8000',
      '/admin/vm-allowlist': 'http://127.0.0.1:8000',
      '/admin/metrics': 'http://127.0.0.1:8000',
      '/admin/users': 'http://127.0.0.1:8000',
    }
  }
})
