const express = require('express');
const router = express.Router();
const pcManager = require('../services/PCManagerService');
const { dbPromise } = require('../database/db');

// PC注册接口
router.post('/register', async (req, res) => {
    try {
        const { pcId, pcName } = req.body;
        
        if (!pcId) {
            return res.status(400).json({
                success: false,
                error: 'PC编号不能为空'
            });
        }

        // 注意：这里没有socketId，因为HTTP请求无法获取
        // Socket ID应该在WebSocket连接时设置
        const result = await pcManager.registerPC(pcId, pcName || pcId, null);
        
        res.json({
            success: true,
            pc: result
        });
    } catch (error) {
        console.error('PC注册错误:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// PC心跳接口
router.post('/heartbeat', async (req, res) => {
    try {
        const { pcId } = req.body;
        
        if (!pcId) {
            return res.status(400).json({
                success: false,
                error: 'PC编号不能为空'
            });
        }

        await pcManager.updateHeartbeat(pcId);
        
        res.json({
            success: true,
            message: '心跳更新成功'
        });
    } catch (error) {
        console.error('心跳更新错误:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 获取在线PC列表
router.get('/online', async (req, res) => {
    try {
        const pcs = await pcManager.getOnlinePCs();
        res.json({
            success: true,
            pcs: pcs
        });
    } catch (error) {
        console.error('获取PC列表错误:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// PC离线通知接口
router.post('/offline', async (req, res) => {
    try {
        const { username } = req.body;
        
        if (!username) {
            return res.status(400).json({
                success: false,
                error: '用户名不能为空'
            });
        }

        const authService = require('../services/AuthService');
        await dbPromise.run(
            `UPDATE users 
             SET status = 'offline', socket_id = NULL, updated_at = CURRENT_TIMESTAMP
             WHERE username = ?`,
            [username]
        );
        
        // 通知Web管理界面（通过Socket.io）
        const io = req.app.get('io');
        if (io) {
            io.emit('clientOffline', { username });
            console.log(`客户端离线通知: ${username}`);
        }
        
        res.json({
            success: true,
            message: '离线状态已更新'
        });
    } catch (error) {
        console.error('PC离线通知错误:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

module.exports = router;
