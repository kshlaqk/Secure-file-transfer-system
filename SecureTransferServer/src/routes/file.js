const express = require('express');
const router = express.Router();
const fileTransferService = require('../services/FileTransferService');

// 文件传输接口
router.post('/transfer', async (req, res) => {
    try {
        // 从请求头或查询参数获取发送者ID
        // 注意：实际部署时应该从认证信息中获取
        const senderId = req.headers['x-sender-id'] || req.query.senderId || req.body.senderId;
        
        const result = await fileTransferService.handleFileTransfer(
            req.body,
            req.app.get('io'),
            senderId
        );
        res.json(result);
    } catch (error) {
        console.error('文件传输错误:', error);
        res.status(400).json({
            success: false,
            error: error.message
        });
    }
});

// 取消传输接口
router.post('/cancel', async (req, res) => {
    try {
        const result = await fileTransferService.handleCancel(
            req.body,
            req.app.get('io')
        );
        res.json(result);
    } catch (error) {
        console.error('取消传输错误:', error);
        res.status(400).json({
            success: false,
            error: error.message
        });
    }
});

// 继续传输接口
router.post('/resume', async (req, res) => {
    try {
        const result = await fileTransferService.handleResume(req.body);
        res.json(result);
    } catch (error) {
        console.error('继续传输错误:', error);
        res.status(400).json({
            success: false,
            error: error.message
        });
    }
});

// 删除任务接口
router.post('/delete', async (req, res) => {
    try {
        const result = await fileTransferService.handleDelete(
            req.body,
            req.app.get('io')
        );
        res.json(result);
    } catch (error) {
        console.error('删除任务错误:', error);
        res.status(400).json({
            success: false,
            error: error.message
        });
    }
});

module.exports = router;
