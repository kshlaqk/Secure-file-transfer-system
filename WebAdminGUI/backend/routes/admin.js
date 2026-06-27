const express = require('express');
const router = express.Router();
const http = require('http');
const adminService = require('../services/AdminService');
const policyService = require('../services/PolicyService');
const whitelistService = require('../services/WhitelistService');
const logService = require('../services/LogService');
const { dbPromise } = require('../config/database');
const { createSession, deleteSession, requireAuth } = require('../middleware/auth');

const MAIN_SERVER_URL = process.env.MAIN_SERVER_URL || 'http://localhost:8080';

// 管理员登录（不需要认证）
router.post('/login', async (req, res) => {
    try {
        const { username, password } = req.body;
        
        if (!username || !password) {
            return res.status(400).json({
                success: false,
                error: '用户名和密码不能为空'
            });
        }
        
        const admin = await adminService.login(username, password);
        const sessionId = createSession(admin.username);
        
        res.json({
            success: true,
            admin: {
                id: admin.id,
                username: admin.username,
                role: admin.role
            },
            sessionId
        });
    } catch (error) {
        res.status(401).json({
            success: false,
            error: error.message
        });
    }
});

// 创建管理员（首次使用，生产环境应移除或添加权限控制）
router.post('/create', async (req, res) => {
    try {
        const { username, password } = req.body;
        
        if (!username || !password) {
            return res.status(400).json({
                success: false,
                error: '用户名和密码不能为空'
            });
        }
        
        const result = await adminService.createAdmin(username, password);
        res.json({
            success: true,
            message: result.message
        });
    } catch (error) {
        res.status(400).json({
            success: false,
            error: error.message
        });
    }
});

// 登出
router.post('/logout', requireAuth, (req, res) => {
    const sessionId = req.headers['x-session-id'] || req.query.sessionId;
    if (sessionId) {
        deleteSession(sessionId);
    }
    res.json({ success: true, message: '已登出' });
});

// 修改密码
router.post('/change-password', requireAuth, async (req, res) => {
    try {
        const { oldPassword, newPassword } = req.body;
        
        if (!oldPassword || !newPassword) {
            return res.status(400).json({
                success: false,
                error: '旧密码和新密码不能为空'
            });
        }
        
        const result = await adminService.changePassword(
            req.adminUsername,
            oldPassword,
            newPassword
        );
        
        res.json({
            success: true,
            message: result.message
        });
    } catch (error) {
        res.status(400).json({
            success: false,
            error: error.message
        });
    }
});

// 从主服务器获取PC列表
router.post('/refresh-clients', requireAuth, async (req, res) => {
    try {
        // 直接查询主服务器数据库获取所有用户信息
        const clients = await dbPromise.all(
            `SELECT username, pc_id, pc_name, status, last_heartbeat, created_at
             FROM users
             ORDER BY username`
        );
        
        // 保存到本地数据库（用于下次登录时加载）
        // 这里我们创建一个本地缓存表
        await dbPromise.run(`
            CREATE TABLE IF NOT EXISTS cached_clients (
                username TEXT PRIMARY KEY,
                pc_id TEXT,
                pc_name TEXT,
                status TEXT,
                last_heartbeat TEXT,
                cached_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        `);
        
        // 清空旧数据
        await dbPromise.run('DELETE FROM cached_clients');
        
        // 插入新数据
        for (const client of clients) {
            await dbPromise.run(
                `INSERT OR REPLACE INTO cached_clients 
                 (username, pc_id, pc_name, status, last_heartbeat) 
                 VALUES (?, ?, ?, ?, ?)`,
                [
                    client.username,
                    client.pc_id,
                    client.pc_name,
                    client.status || 'offline',
                    client.last_heartbeat
                ]
            );
        }
        
        res.json({
            success: true,
            clients: clients.map(client => ({
                username: client.username,
                machineId: client.pc_id,
                pcName: client.pc_name,
                status: client.status,
                lastHeartbeat: client.last_heartbeat,
                createdAt: client.created_at
            })),
            message: `已刷新 ${clients.length} 台PC`
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 获取PC列表（直接从主服务器数据库读取，实时数据）
router.get('/clients', requireAuth, async (req, res) => {
    try {
        // 直接从主服务器的 users 表读取（实时数据）
        const clients = await dbPromise.all(
            `SELECT username, pc_id, pc_name, status, last_heartbeat, created_at
             FROM users
             ORDER BY username`
        );
        
        res.json({
            success: true,
            clients: clients.map(client => ({
                username: client.username,
                machineId: client.pc_id,
                pcName: client.pc_name,
                status: client.status || 'offline',
                lastHeartbeat: client.last_heartbeat,
                createdAt: client.created_at
            }))
        });
    } catch (error) {
        console.error('获取PC列表错误:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 获取客户端策略
router.get('/clients/:username/policy', requireAuth, async (req, res) => {
    try {
        const policy = await policyService.getClientPolicy(req.params.username);
        res.json({ success: true, policy });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 更新客户端策略
router.post('/clients/:username/policy', requireAuth, async (req, res) => {
    try {
        const { protectedExtensions, enableEncryption } = req.body;
        const updatedBy = req.adminUsername;
        
        const version = await policyService.updateClientPolicy(
            req.params.username,
            { protectedExtensions, enableEncryption },
            updatedBy
        );
        
        // 如果客户端在线，通过主服务器推送策略
        let pushStatus = false;  // 跟踪推送状态
        try {
            const user = await dbPromise.get(
                'SELECT socket_id FROM users WHERE username = ? AND status = ?',
                [req.params.username, 'online']
            );
            
            if (user && user.socket_id) {
                // 通过 HTTP 请求主服务器的推送 API
                pushToMainServer('pushPolicy', {
                    socketId: user.socket_id,
                    policy: {
                        version,
                        protectedExtensions: protectedExtensions || '',
                        enableEncryption: enableEncryption || false
                    }
                });
                pushStatus = true;  // 标记为已推送
            } else {
                console.log(`客户端 ${req.params.username} 未上线，策略已保存，上线后会自动同步`);
            }
        } catch (err) {
            console.error('推送策略失败:', err);
            // 策略已保存，客户端上线后会同步
        }
        
        res.json({
            success: true,
            version,
            message: '策略已更新' + (pushStatus ? '，已推送给在线客户端' : '，客户端上线后会自动同步')
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 获取客户端白名单
router.get('/clients/:username/whitelist', requireAuth, async (req, res) => {
    try {
        const whitelist = await whitelistService.getClientWhitelist(req.params.username);
        res.json({ success: true, whitelist });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 添加白名单项
router.post('/clients/:username/whitelist', requireAuth, async (req, res) => {
    try {
        console.log(`[WebAdminGUI] ===== 收到添加白名单请求 =====`);
        console.log(`[WebAdminGUI] 用户名: ${req.params.username}`);
        console.log(`[WebAdminGUI] 请求体:`, JSON.stringify(req.body, null, 2));
        
        const { processPath } = req.body;
        
        if (!processPath) {
            console.error(`[WebAdminGUI] ✗ 进程路径为空`);
            return res.status(400).json({
                success: false,
                error: '进程路径不能为空'
            });
        }
        
        console.log(`[WebAdminGUI] 进程路径: ${processPath}`);
        
        const addedBy = req.adminUsername;
        console.log(`[WebAdminGUI] 添加者: ${addedBy}`);
        
        await whitelistService.addWhitelistItem(
            req.params.username,
            processPath,
            addedBy
        );
        
        console.log(`[WebAdminGUI] ✓ 白名单项已添加到数据库`);
        
        // 推送白名单更新
        try {
            console.log(`[WebAdminGUI] 开始查询用户在线状态...`);
            console.log(`[WebAdminGUI] 查询SQL: SELECT socket_id FROM users WHERE username = ? AND status = ?`);
            console.log(`[WebAdminGUI] 查询参数: [${req.params.username}, 'online']`);
            
            const user = await dbPromise.get(
                'SELECT socket_id FROM users WHERE username = ? AND status = ?',
                [req.params.username, 'online']
            );
            
            console.log(`[WebAdminGUI] 查询结果:`, user);
            
            if (user && user.socket_id) {
                console.log(`[WebAdminGUI] ✓ 用户在线，socket_id: ${user.socket_id}`);
                console.log(`[WebAdminGUI] 获取完整白名单列表...`);
                
                const whitelist = await whitelistService.getClientWhitelist(req.params.username);
                console.log(`[WebAdminGUI] 白名单列表:`, JSON.stringify(whitelist, null, 2));
                
                // 通过 HTTP 请求主服务器推送
                console.log(`[WebAdminGUI] 准备调用 pushToMainServer...`);
                pushToMainServer('whitelistUpdate', {
                    socketId: user.socket_id,
                    whitelist: whitelist.map(item => item.processPath)
                });
                console.log(`[WebAdminGUI] pushToMainServer 调用完成`);
            } else {
                console.log(`[WebAdminGUI] ⚠ 用户不在线或没有socket_id，跳过推送`);
                console.log(`[WebAdminGUI] user对象:`, user);
            }
        } catch (err) {
            console.error(`[WebAdminGUI] ✗ 推送白名单失败:`, err);
            console.error(`[WebAdminGUI] 错误堆栈:`, err.stack);
        }
        
        console.log(`[WebAdminGUI] ===== 添加白名单请求处理完成 =====`);
        res.json({ success: true, message: '白名单项已添加' });
    } catch (error) {
        console.error(`[WebAdminGUI] ✗ 添加白名单请求处理异常:`, error);
        console.error(`[WebAdminGUI] 错误堆栈:`, error.stack);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 删除白名单项
router.delete('/clients/:username/whitelist', requireAuth, async (req, res) => {
    try {
        const { processPath } = req.body;
        
        if (!processPath) {
            return res.status(400).json({
                success: false,
                error: '进程路径不能为空'
            });
        }
        
        await whitelistService.removeWhitelistItem(req.params.username, processPath);
        
        // 推送更新
        try {
            const user = await dbPromise.get(
                'SELECT socket_id FROM users WHERE username = ? AND status = ?',
                [req.params.username, 'online']
            );
            
            if (user && user.socket_id) {
                const whitelist = await whitelistService.getClientWhitelist(req.params.username);
                pushToMainServer('whitelistUpdate', {
                    socketId: user.socket_id,
                    whitelist: whitelist.map(item => item.processPath)
                });
            }
        } catch (err) {
            console.error('推送白名单失败:', err);
        }
        
        res.json({ success: true, message: '白名单项已删除' });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 获取客户端日志
router.get('/clients/:username/logs', requireAuth, async (req, res) => {
    try {
        const limit = parseInt(req.query.limit) || 100;
        const offset = parseInt(req.query.offset) || 0;
        
        const logs = await logService.getClientLogs(req.params.username, limit, offset);
        const stats = await logService.getClientLogStats(req.params.username);
        
        res.json({
            success: true,
            logs,
            stats,
            pagination: {
                limit,
                offset,
                total: stats.total
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 请求客户端同步日志
router.post('/clients/:username/logs/sync', requireAuth, async (req, res) => {
    try {
        const user = await dbPromise.get(
            'SELECT socket_id FROM users WHERE username = ? AND status = ?',
            [req.params.username, 'online']
        );
        
        if (!user || !user.socket_id) {
            return res.status(400).json({
                success: false,
                error: '客户端未上线'
            });
        }
        
        // 通过 HTTP 请求主服务器推送日志同步请求
        pushToMainServer('requestLogs', {
            socketId: user.socket_id
        });
        
        res.json({
            success: true,
            message: '已请求客户端同步日志'
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 下载客户端日志（返回文本文件）
router.get('/clients/:username/logs/download', requireAuth, async (req, res) => {
    try {
        const limit = parseInt(req.query.limit) || 10000; // 默认最多10000条
        const logs = await logService.getClientLogs(req.params.username, limit, 0);
        
        // 格式化为文本格式
        let logText = `驱动日志 - 用户: ${req.params.username}\n`;
        logText += `导出时间: ${new Date().toLocaleString('zh-CN')}\n`;
        logText += `日志条数: ${logs.length}\n`;
        logText += '='.repeat(80) + '\n\n';
        
        for (const log of logs) {
            const date = new Date(log.timestamp);
            const actionText = log.actionCode === 1 ? '允许' : '拒绝';
            logText += `[${date.toLocaleString('zh-CN')}] ${actionText} | 进程: ${log.processName} | 文件: ${log.filePath}\n`;
        }
        
        // 生成安全文件名（过滤掉 HTTP 头不允许的字符），使用 Express 自带的 attachment 设置 Content-Disposition
        const rawUsername = String(req.params.username || '');
        const safeUser = rawUsername.replace(/[^a-zA-Z0-9_\-]/g, '_');
        const filename = `logs_${safeUser}_${Date.now()}.txt`;

        res.type('text/plain; charset=utf-8');
        res.attachment(filename);
        res.send(logText);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 辅助函数：推送消息到主服务器
function pushToMainServer(event, data) {
    console.log(`[WebAdminGUI] ===== 开始推送事件到主服务器 =====`);
    console.log(`[WebAdminGUI] 事件类型: ${event}`);
    console.log(`[WebAdminGUI] 数据内容:`, JSON.stringify(data, null, 2));
    
    const pushUrl = new URL('/api/admin/push', MAIN_SERVER_URL);
    console.log(`[WebAdminGUI] 目标URL: ${pushUrl.toString()}`);
    console.log(`[WebAdminGUI] 主服务器地址: ${MAIN_SERVER_URL}`);
    
    const postData = JSON.stringify({
        event,
        ...data
    });
    
    console.log(`[WebAdminGUI] 请求数据: ${postData}`);
    
    const options = {
        hostname: pushUrl.hostname,
        port: pushUrl.port || 8080,
        path: pushUrl.pathname,
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(postData)
        },
        timeout: 5000
    };
    
    console.log(`[WebAdminGUI] HTTP请求选项:`, {
        hostname: options.hostname,
        port: options.port,
        path: options.path,
        method: options.method
    });
    
    const req = http.request(options, (res) => {
        let responseData = '';
        console.log(`[WebAdminGUI] 收到响应，状态码: ${res.statusCode}`);
        
        res.on('data', (chunk) => {
            responseData += chunk;
        });
        
        res.on('end', () => {
            if (res.statusCode === 200) {
                console.log(`[WebAdminGUI] ✓ 推送成功 ${event}`);
                console.log(`[WebAdminGUI] 响应内容:`, responseData);
            } else {
                console.error(`[WebAdminGUI] ✗ 推送失败 ${event}，状态码: ${res.statusCode}`);
                console.error(`[WebAdminGUI] 响应内容:`, responseData);
            }
            console.log(`[WebAdminGUI] ===== 推送完成 =====`);
        });
    });
    
    req.on('error', (err) => {
        console.error(`[WebAdminGUI] ✗ 推送 ${event} 失败，错误:`, err.message);
        console.error(`[WebAdminGUI] 错误堆栈:`, err.stack);
        console.log(`[WebAdminGUI] ===== 推送失败 =====`);
    });
    
    req.on('timeout', () => {
        req.destroy();
        console.error(`[WebAdminGUI] ✗ 推送 ${event} 超时（5秒）`);
        console.log(`[WebAdminGUI] ===== 推送超时 =====`);
    });
    
    console.log(`[WebAdminGUI] 发送HTTP请求...`);
    req.write(postData);
    req.end();
}

module.exports = router;
