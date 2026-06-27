const express = require('express');
const router = express.Router();
const authService = require('../services/AuthService');

// 用户注册
router.post('/register', async (req, res) => {
    try {
        const { username, password, machineId } = req.body;
        
        if (!username || !password || !machineId) {
            return res.status(400).json({
                success: false,
                error: '账号、密码和机器码不能为空'
            });
        }
        
        const user = await authService.register(username, password, machineId);
        
        // 更新PC状态为在线（但不设置 socket_id，因为此时还没有 WebSocket 连接）
        // socket_id 会在 WebSocket 的 'register' 事件中设置
        await authService.updatePCStatus(user.username);
        
        // 获取Socket.io实例并广播客户端上线事件（供WebGUI实时更新）
        const io = req.app.get('io');
        if (io) {
            io.emit('clientOnline', { username: user.username });
            console.log(`客户端 ${user.username} 注册成功，已广播上线通知`);
        }
        
        res.json({
            success: true,
            message: '注册成功',
            user: {
                username: user.username,
                machineId: user.machineId,
                status: 'online'  // 返回在线状态
            }
        });
    } catch (error) {
        console.error('注册错误:', error);
        res.status(400).json({
            success: false,
            error: error.message
        });
    }
});

// 用户登录
router.post('/login', async (req, res) => {
    try {
        const { username, password } = req.body;
        
        if (!username || !password) {
            return res.status(400).json({
                success: false,
                error: '账号和密码不能为空'
            });
        }
        
        const user = await authService.login(username, password);
        
        // 更新PC状态为在线（但不设置 socket_id，因为此时还没有 WebSocket 连接）
        // socket_id 会在 WebSocket 的 'register' 事件中设置
        await authService.updatePCStatus(user.username);
        
        // 获取Socket.io实例并广播客户端上线事件（供WebGUI实时更新）
        const io = req.app.get('io');
        if (io) {
            io.emit('clientOnline', { username: user.username });
            console.log(`客户端 ${user.username} 登录成功，已广播上线通知`);
        }
        
        res.json({
            success: true,
            message: '登录成功',
            user: {
                username: user.username,
                machineId: user.machineId,
                status: 'online'  // 返回在线状态
            }
        });
    } catch (error) {
        console.error('登录错误:', error);
        res.status(401).json({
            success: false,
            error: error.message
        });
    }
});

module.exports = router;
