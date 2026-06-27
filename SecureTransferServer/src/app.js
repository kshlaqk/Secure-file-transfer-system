const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');
const bodyParser = require('body-parser');
const path = require('path');

// 导入路由
const fileRoutes = require('./routes/file');
const filesRoutes = require('./routes/files');
const pcRoutes = require('./routes/pc');
const authRoutes = require('./routes/auth');
const adminPushRoutes = require('./routes/admin');

// 导入WebSocket处理器
const setupSocketHandlers = require('./websocket/socketHandler');

// 导入文件清理服务
const fileCleanupService = require('./services/FileCleanupService');

// 创建Express应用
const app = express();
const server = http.createServer(app);

// 创建Socket.io实例
const io = socketIo(server, {
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});

// 将io实例附加到app，以便在路由中使用
app.set('io', io);

// 中间件
app.use(cors());
app.use(bodyParser.json({ limit: '100mb' })); // 支持大文件
app.use(bodyParser.urlencoded({ extended: true, limit: '100mb' }));

// 请求日志中间件
app.use((req, res, next) => {
    console.log(`${new Date().toISOString()} - ${req.method} ${req.path}`);
    next();
});

// API路由
app.use('/api/file', fileRoutes);
app.use('/api/files', filesRoutes);
app.use('/api/pc', pcRoutes);
app.use('/api/auth', authRoutes);
app.use('/api/admin', adminPushRoutes);  // 管理后台推送 API

// 健康检查接口
app.get('/health', (req, res) => {
    res.json({
        status: 'ok',
        timestamp: new Date().toISOString()
    });
});

// 根路径
app.get('/', (req, res) => {
    res.json({
        name: 'Secure Transfer Server',
        version: '1.0.0',
        endpoints: {
            authRegister: 'POST /api/auth/register',
            authLogin: 'POST /api/auth/login',
            fileTransfer: 'POST /api/file/transfer',
            cancel: 'POST /api/file/cancel',
            resume: 'POST /api/file/resume',
            delete: 'POST /api/file/delete',
            completedFiles: 'GET /api/files/completed',
            downloadFile: 'GET /api/files/download/:fileId',
            confirmDownload: 'POST /api/files/confirm-download',
            pcRegister: 'POST /api/pc/register',
            pcHeartbeat: 'POST /api/pc/heartbeat',
            onlinePCs: 'GET /api/pc/online'
        }
    });
});

// 设置WebSocket处理器
setupSocketHandlers(io);
console.log('[Server] WebSocket 处理器已设置');

// 错误处理中间件
app.use((err, req, res, next) => {
    console.error('服务器错误:', err);
    res.status(500).json({
        success: false,
        error: err.message || '服务器内部错误'
    });
});

// 404处理
app.use((req, res) => {
    res.status(404).json({
        success: false,
        error: '接口不存在'
    });
});

// 启动服务器
const PORT = process.env.PORT || 8080;
server.listen(PORT, '0.0.0.0', () => {
    console.log('========================================');
    console.log('安全文件传输服务器已启动');
    console.log(`HTTP服务器: http://0.0.0.0:${PORT}`);
    console.log(`WebSocket服务器: ws://0.0.0.0:${PORT}`);
    console.log(`Socket.io 路径: /socket.io/`);
    console.log(`等待客户端连接...`);
    console.log('========================================');
    
    // 启动定期文件清理服务（每24小时执行一次，保留7天）
    fileCleanupService.startPeriodicCleanup(24, 7);
});

// 优雅关闭
process.on('SIGTERM', () => {
    console.log('收到SIGTERM信号，正在关闭服务器...');
    fileCleanupService.stopPeriodicCleanup();
    server.close(() => {
        console.log('服务器已关闭');
        process.exit(0);
    });
});

process.on('SIGINT', () => {
    console.log('收到SIGINT信号，正在关闭服务器...');
    fileCleanupService.stopPeriodicCleanup();
    server.close(() => {
        console.log('服务器已关闭');
        process.exit(0);
    });
});
