const express = require('express');
const router = express.Router();

// 推送策略到客户端（供管理后台调用）
router.post('/push', (req, res) => {
    try {
        console.log(`[SecureTransferServer] ===== 收到推送请求 =====`);
        console.log(`[SecureTransferServer] 请求体:`, JSON.stringify(req.body, null, 2));
        
        const { event, socketId, policy, whitelist } = req.body;
        console.log(`[SecureTransferServer] 解析参数:`);
        console.log(`[SecureTransferServer]   - event: ${event}`);
        console.log(`[SecureTransferServer]   - socketId: ${socketId}`);
        console.log(`[SecureTransferServer]   - hasPolicy: ${!!policy}`);
        console.log(`[SecureTransferServer]   - hasWhitelist: ${!!whitelist}`);
        if (whitelist && Array.isArray(whitelist)) {
            console.log(`[SecureTransferServer]   - whitelist数组长度: ${whitelist.length}`);
            console.log(`[SecureTransferServer]   - whitelist内容:`, JSON.stringify(whitelist, null, 2));
        }
        
        const io = req.app.get('io');
        
        if (!io) {
            console.error(`[SecureTransferServer] ✗ WebSocket 服务器未初始化`);
            return res.status(500).json({
                success: false,
                error: 'WebSocket 服务器未初始化'
            });
        }
        
        console.log(`[SecureTransferServer] ✓ WebSocket 服务器已初始化`);
        
        if (event === 'pushPolicy' && socketId && policy) {
            // 推送策略更新
            console.log(`[SecureTransferServer] 处理策略推送请求`);
            console.log(`[SecureTransferServer] 发送 policyUpdate 事件到 socketId: ${socketId}`);
            io.to(socketId).emit('policyUpdate', policy);
            console.log(`[SecureTransferServer] ✓ 策略已推送到客户端: ${socketId}`);
            
            return res.json({
                success: true,
                message: '策略已推送'
            });
        }
        
        if (event === 'whitelistUpdate' && socketId && whitelist) {
            // 推送白名单更新
            console.log(`[SecureTransferServer] 处理白名单推送请求`);
            console.log(`[SecureTransferServer] 发送 whitelistUpdate 事件到 socketId: ${socketId}`);
            console.log(`[SecureTransferServer] 白名单数据:`, JSON.stringify({ whitelist }, null, 2));
            
            io.to(socketId).emit('whitelistUpdate', { whitelist });
            
            console.log(`[SecureTransferServer] ✓ 白名单已推送到客户端: ${socketId}`);
            console.log(`[SecureTransferServer] ===== 推送完成 =====`);
            
            return res.json({
                success: true,
                message: '白名单已推送'
            });
        }
        
        if (event === 'requestLogs' && socketId) {
            // 请求日志同步
            console.log(`[SecureTransferServer] 处理日志请求`);
            console.log(`[SecureTransferServer] 发送 requestLogs 事件到 socketId: ${socketId}`);
            io.to(socketId).emit('requestLogs', {});
            console.log(`[SecureTransferServer] ✓ 已请求客户端同步日志: ${socketId}`);
            
            return res.json({
                success: true,
                message: '已请求日志同步'
            });
        }
        
        console.error(`[SecureTransferServer] ✗ 无效的推送请求`);
        console.error(`[SecureTransferServer] 参数检查:`);
        console.error(`[SecureTransferServer]   - event === 'whitelistUpdate': ${event === 'whitelistUpdate'}`);
        console.error(`[SecureTransferServer]   - socketId存在: ${!!socketId}`);
        console.error(`[SecureTransferServer]   - whitelist存在: ${!!whitelist}`);
        console.log(`[SecureTransferServer] ===== 推送失败 =====`);
        
        res.status(400).json({
            success: false,
            error: '无效的推送请求'
        });
    } catch (error) {
        console.error(`[SecureTransferServer] ✗ 推送错误:`, error);
        console.error(`[SecureTransferServer] 错误堆栈:`, error.stack);
        console.log(`[SecureTransferServer] ===== 推送异常 =====`);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

module.exports = router;
