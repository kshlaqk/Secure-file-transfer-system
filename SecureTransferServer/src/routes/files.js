const express = require('express');
const router = express.Router();
const fileTransferService = require('../services/FileTransferService');
const fileStorageService = require('../services/FileStorageService');
const path = require('path');
const fs = require('fs').promises;

// 获取已完成文件列表
router.get('/completed', async (req, res) => {
    try {
        const targetId = req.query.targetId || null;
        const files = await fileTransferService.getCompletedFiles(targetId);
        
        res.json({
            success: true,
            files: files
        });
    } catch (error) {
        console.error('获取文件列表错误:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 下载已完成文件
router.get('/download/:fileId', async (req, res) => {
    try {
        const { fileId } = req.params;
        const targetId = req.query.targetId || null; // 从查询参数获取目标用户ID（用于权限验证）
        
        // 获取文件信息
        const fileInfo = await fileTransferService.getCompletedFileInfo(fileId, targetId);
        
        if (!fileInfo) {
            return res.status(404).json({
                success: false,
                error: '文件不存在或无权访问'
            });
        }
        
        // 检查文件是否存在
        const fileExists = await fileStorageService.fileExists(fileInfo.filePath);
        if (!fileExists) {
            return res.status(404).json({
                success: false,
                error: '文件不存在'
            });
        }
        
        // 设置响应头
        const fileName = fileInfo.fileName;
        res.setHeader('Content-Type', 'application/octet-stream');
        res.setHeader('Content-Disposition', `attachment; filename="${encodeURIComponent(fileName)}"`);
        res.setHeader('Content-Length', fileInfo.fileSize);
        
        // 读取并发送文件
        const fileBuffer = await fileStorageService.readFile(fileInfo.filePath);
        res.send(fileBuffer);
        
    } catch (error) {
        console.error('下载文件错误:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// 确认下载并删除文件
router.post('/confirm-download', async (req, res) => {
    try {
        const { fileId, targetId } = req.body;
        
        if (!fileId || !targetId) {
            return res.status(400).json({
                success: false,
                error: '缺少必要参数：fileId 和 targetId'
            });
        }
        
        const result = await fileTransferService.confirmDownloadAndDelete(fileId, targetId);
        
        res.json({
            success: true,
            message: result.message
        });
    } catch (error) {
        console.error('确认下载错误:', error);
        
        // 如果是权限错误，返回404；其他错误返回500
        const statusCode = error.message.includes('不存在') || error.message.includes('无权') ? 404 : 500;
        
        res.status(statusCode).json({
            success: false,
            error: error.message
        });
    }
});

module.exports = router;
