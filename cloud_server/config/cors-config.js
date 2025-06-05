// CORS配置文件
module.exports = {
  origin: [
    'http://localhost:3000',
    'http://localhost:5173', // Vite开发服务器
    'http://43.167.176.52:3000',
    'http://43.167.176.52',
    'http://www.nagaflow.top',
    'https://www.nagaflow.top',
    'http://nagaflow.top',
    'https://nagaflow.top',
    'http://www.naga.top:3000',
    'http://www.naga.top'
  ],
  credentials: true,
  methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
  allowedHeaders: ['Content-Type', 'Authorization', 'X-Requested-With'],
  optionsSuccessStatus: 200 // 支持旧版浏览器
};
