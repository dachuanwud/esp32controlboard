import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    host: '0.0.0.0',
    proxy: {
      '/api': {
        target: 'http://192.168.6.109',  // ESP32的IP地址，已更新为当前IP
        changeOrigin: true,
        secure: false,
        configure: (proxy, options) => {
          proxy.on('proxyReq', (proxyReq, req, res) => {
            // 移除不必要的请求头以减少头部大小
            proxyReq.removeHeader('referer');
            proxyReq.removeHeader('origin');
            proxyReq.removeHeader('user-agent');
            proxyReq.removeHeader('accept-encoding');
            proxyReq.removeHeader('accept-language');
            proxyReq.removeHeader('cache-control');
            proxyReq.removeHeader('pragma');
            proxyReq.removeHeader('sec-fetch-dest');
            proxyReq.removeHeader('sec-fetch-mode');
            proxyReq.removeHeader('sec-fetch-site');
            proxyReq.removeHeader('sec-ch-ua');
            proxyReq.removeHeader('sec-ch-ua-mobile');
            proxyReq.removeHeader('sec-ch-ua-platform');

            // 设置简化的请求头
            proxyReq.setHeader('User-Agent', 'ESP32-WebClient/1.0');
            proxyReq.setHeader('Accept', 'application/json');
            proxyReq.setHeader('Connection', 'close');
          });
        }
      }
    }
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    sourcemap: false,
    minify: 'terser',
    rollupOptions: {
      output: {
        manualChunks: {
          vendor: ['react', 'react-dom'],
          bootstrap: ['react-bootstrap', 'bootstrap']
        }
      }
    }
  }
})
