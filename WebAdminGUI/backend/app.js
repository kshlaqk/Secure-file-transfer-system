const express = require('express');
const http = require('http');
const path = require('path');
const cors = require('cors');
const bodyParser = require('body-parser');
const { Server } = require('socket.io');

const adminRoutes = require('./routes/admin');
const { initializeDatabase } = require('./database/migrations');

// 配置
const MAIN_SERVER_URL = process.env.MAIN_SERVER_URL || 'http://localhost:8080';
const ADMIN_PORT = process.env.ADMIN_PORT || 3000;

const app = express();
const server = http.createServer(app);

// 创建本地 Socket.io 服务器（用于前端连接）
const io = new Server(server, {
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});

// 连接到主服务器的 WebSocket（用于推送策略）
// 注意：管理后台需要直接访问主服务器的 io 实例来推送消息
// 这里我们通过 HTTP API 或直接访问主服务器的 io 实例
// 获取主服务器的 io 实例（需要主服务器暴露）
// 方案1：通过 HTTP API 推送（推荐）
// 方案2：直接访问主服务器的 io 实例（如果管理后台和主服务器在同一进程）
// 这里我们使用方案1，通过 HTTP 请求主服务器的推送 API
async function pushToMainServer(event, data) {
    // 这里可以通过 HTTP 请求主服务器的推送 API
    // 或者如果主服务器提供了推送 API，直接调用
    // 暂时先记录日志，实际推送需要在主服务器实现
    console.log(`需要推送事件 ${event} 到主服务器:`, data);
}

// 将 io 附加到 app
app.set('io', io);

// 中间件
app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

// 静态文件服务（前端）
app.use(express.static(path.join(__dirname, '../frontend')));

// API 路由
app.use('/api/admin', adminRoutes);

// 前端 WebSocket 连接
io.on('connection', (socket) => {
    console.log('前端客户端连接:', socket.id);
    
    socket.on('disconnect', () => {
        console.log('前端客户端断开:', socket.id);
    });
});

// 连接到主服务器的Socket.io（用于接收客户端状态更新）
const socketIOClient = require('socket.io-client');
let mainServerSocket = null;

// 尝试连接到主服务器的Socket.io
try {
    const mainServerSocketURL = MAIN_SERVER_URL.replace('http://', 'ws://').replace('https://', 'wss://');
    mainServerSocket = socketIOClient(mainServerSocketURL, {
        reconnection: true,
        reconnectionDelay: 1000,
        reconnectionAttempts: 5
    });
    
    mainServerSocket.on('connect', () => {
        console.log('✓ 已连接到主服务器Socket.io');
    });
    
    mainServerSocket.on('disconnect', () => {
        console.log('✗ 已断开与主服务器Socket.io的连接');
    });
    
    mainServerSocket.on('connect_error', (error) => {
        console.warn('连接主服务器Socket.io失败:', error.message);
    });
    
    // 监听主服务器发送的客户端离线事件
    mainServerSocket.on('clientOffline', (data) => {
        console.log('从主服务器收到客户端离线通知:', data);
        // 转发给所有连接的前端客户端
        io.emit('clientOffline', data);
    });
    
    // 监听主服务器发送的客户端上线事件
    mainServerSocket.on('clientOnline', (data) => {
        console.log('从主服务器收到客户端上线通知:', data);
        // 转发给所有连接的前端客户端
        io.emit('clientOnline', data);
    });
} catch (error) {
    console.warn('无法初始化主服务器Socket.io连接:', error.message);
}

// 初始化数据库
const adminService = require('./services/AdminService');

initializeDatabase().then(async () => {
    console.log('✓ 数据库初始化完成');
    
    // 初始化默认管理员
    await adminService.initializeDefaultAdmin();
    
    // 启动服务器
    server.listen(ADMIN_PORT, '0.0.0.0', () => {
        console.log('========================================');
        console.log('  Web 管理后台已启动');
        console.log('========================================');
        console.log(`管理界面: http://0.0.0.0:${ADMIN_PORT}`);
        console.log(`主服务器: ${MAIN_SERVER_URL}`);
        console.log('========================================');
        console.log('默认账号: admin / admin');
        console.log('========================================');
    });
}).catch(err => {
    console.error('✗ 数据库初始化失败:', err);
    process.exit(1);
});

// 优雅关闭
process.on('SIGTERM', () => {
    console.log('收到SIGTERM信号，正在关闭服务器...');
    server.close(() => {
        process.exit(0);
    });
});

process.on('SIGINT', () => {
    console.log('收到SIGINT信号，正在关闭服务器...');
    server.close(() => {
        process.exit(0);
    });
});
