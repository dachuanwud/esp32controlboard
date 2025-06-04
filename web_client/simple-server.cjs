const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 3000;
const ESP32_IP = 'http://192.168.6.109';

// MIME types
const mimeTypes = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.json': 'application/json',
  '.png': 'image/png',
  '.jpg': 'image/jpg',
  '.gif': 'image/gif',
  '.svg': 'image/svg+xml',
  '.wav': 'audio/wav',
  '.mp4': 'video/mp4',
  '.woff': 'application/font-woff',
  '.ttf': 'application/font-ttf',
  '.eot': 'application/vnd.ms-fontobject',
  '.otf': 'application/font-otf',
  '.wasm': 'application/wasm'
};

const server = http.createServer((req, res) => {
  console.log(`${new Date().toISOString()} - ${req.method} ${req.url}`);

  // Handle API proxy
  if (req.url.startsWith('/api')) {
    // Filter and optimize headers for ESP32
    const filteredHeaders = {
      'Content-Type': req.headers['content-type'] || 'application/json',
      'Content-Length': req.headers['content-length'],
      'Accept': 'application/json',
      'Connection': 'close',
      'User-Agent': 'ESP32-WebClient/1.0'
    };

    // Remove undefined headers
    Object.keys(filteredHeaders).forEach(key => {
      if (filteredHeaders[key] === undefined) {
        delete filteredHeaders[key];
      }
    });

    const options = {
      hostname: '192.168.6.109',
      port: 80,
      path: req.url,
      method: req.method,
      headers: filteredHeaders
    };

    const proxyReq = http.request(options, (proxyRes) => {
      // Filter response headers
      const responseHeaders = {
        'Content-Type': proxyRes.headers['content-type'],
        'Content-Length': proxyRes.headers['content-length'],
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type, Authorization'
      };

      // Remove undefined headers
      Object.keys(responseHeaders).forEach(key => {
        if (responseHeaders[key] === undefined) {
          delete responseHeaders[key];
        }
      });

      res.writeHead(proxyRes.statusCode, responseHeaders);
      proxyRes.pipe(res);
    });

    // Set timeout
    proxyReq.setTimeout(10000, () => {
      console.error('Proxy timeout');
      if (!res.headersSent) {
        res.writeHead(408);
        res.end('Request Timeout');
      }
      proxyReq.destroy();
    });

    proxyReq.on('error', (err) => {
      console.error('Proxy error:', err);
      if (!res.headersSent) {
        res.writeHead(500);
        res.end(JSON.stringify({
          error: 'Proxy Error',
          message: 'Failed to connect to ESP32 device'
        }));
      }
    });

    req.pipe(proxyReq);
    return;
  }

  // Serve static files from dist directory
  let filePath = path.join(__dirname, 'dist', req.url === '/' ? 'index.html' : req.url);

  // Security check
  if (!filePath.startsWith(__dirname)) {
    res.writeHead(403);
    res.end('Forbidden');
    return;
  }

  const extname = String(path.extname(filePath)).toLowerCase();
  const contentType = mimeTypes[extname] || 'application/octet-stream';

  fs.readFile(filePath, (error, content) => {
    if (error) {
      if (error.code === 'ENOENT') {
        // File not found, serve index.html for SPA routing
        fs.readFile(path.join(__dirname, 'dist', 'index.html'), (err, indexContent) => {
          if (err) {
            res.writeHead(500);
            res.end('Server Error');
          } else {
            res.writeHead(200, { 'Content-Type': 'text/html' });
            res.end(indexContent, 'utf-8');
          }
        });
      } else {
        res.writeHead(500);
        res.end('Server Error: ' + error.code);
      }
    } else {
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(content, 'utf-8');
    }
  });
});

server.listen(PORT, '0.0.0.0', () => {
  console.log(`🌐 Web server running at:`);
  console.log(`   Local:   http://localhost:${PORT}/`);
  console.log(`   Network: http://0.0.0.0:${PORT}/`);
  console.log(`🔗 Proxying /api requests to: ${ESP32_IP}`);
  console.log(`📱 ESP32 Device IP: ${ESP32_IP.replace('http://', '')}`);
});
