const { dbPromise } = require('../database/db');
const { v4: uuidv4 } = require('uuid');
const pcManager = require('./PCManagerService');
const fileStorageService = require('./FileStorageService');

// 协议Magic值
const PROTOCOL_MAGIC = {
    FILE_TRANSFER: 0x46545251,  // "FTQ"
    CANCEL: 0x4654434E,          // "FTCN"
    RESUME: 0x46545245,          // "FTRE"
    DELETE: 0x46544445           // "FTDE"
};

class FileTransferService {
    /**
     * 处理文件传输请求
     * @param {Object} request - 请求数据
     * @param {Function} io - Socket.io实例
     * @param {string} senderId - 发送者PC编号（可选，从请求头获取）
     * @returns {Promise<Object>}
     */
    async handleFileTransfer(request, io, senderId = null) {
        // 验证协议
        if (request.magic !== PROTOCOL_MAGIC.FILE_TRANSFER) {
            throw new Error('无效的协议标识');
        }

        const { targetId, fileName, fileSize, fileData, startOffset = 0 } = request;
        const taskId = `task_${Date.now()}_${uuidv4().substring(0, 8)}`;
        
        // 解码Base64文件数据
        const fileBuffer = Buffer.from(fileData, 'base64');
        
        // 保存文件
        const filePath = await fileStorageService.saveFile(
            taskId,
            fileName,
            fileBuffer,
            startOffset
        );

        // 获取发送者ID（从参数传入，如果没有则使用'unknown'）
        // 注意：实际部署时应该从JWT token或session中获取
        const finalSenderId = senderId || request.senderId || 'unknown';

        // 创建传输任务记录
        await dbPromise.run(
            `INSERT INTO file_transfers 
             (task_id, sender_id, target_id, file_name, file_size, 
              bytes_received, file_path, status, start_offset)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
            [
                taskId,
                finalSenderId,
                targetId,
                fileName,
                fileSize,
                fileBuffer.length + startOffset,
                filePath,
                'transferring',
                startOffset
            ]
        );

        // 检查目标PC是否在线（targetId现在是username）
        const isOnline = await pcManager.isPCOnline(targetId);
        const socketId = await pcManager.getPCSocketId(targetId);

        if (isOnline && socketId) {
            // 目标PC在线，立即转发
            io.to(socketId).emit('fileTransfer', {
                taskId,
                senderId: finalSenderId,
                fileName,
                fileSize,
                fileData,
                startOffset,
                isComplete: fileBuffer.length + startOffset >= fileSize
            });
        }

        // 检查文件是否传输完成
        const currentSize = fileBuffer.length + startOffset;
        if (currentSize >= fileSize) {
            await this.completeTransfer(taskId);
        }

        return {
            success: true,
            taskId,
            message: isOnline ? '文件已转发给目标PC' : '文件已保存，等待目标PC上线'
        };
    }

    /**
     * 处理取消传输请求
     * @param {Object} request - 请求数据
     * @param {Function} io - Socket.io实例
     * @returns {Promise<Object>}
     */
    async handleCancel(request, io) {
        if (request.magic !== PROTOCOL_MAGIC.CANCEL) {
            throw new Error('无效的协议标识');
        }

        const { targetId, fileName, interruptOffset } = request;

        // 查找传输任务
        const task = await dbPromise.get(
            `SELECT * FROM file_transfers 
             WHERE target_id = ? AND file_name = ? AND status = 'transferring'`,
            [targetId, fileName]
        );

        if (task) {
            // 更新任务状态
            await dbPromise.run(
                `UPDATE file_transfers 
                 SET status = 'cancelled', updated_at = CURRENT_TIMESTAMP
                 WHERE task_id = ?`,
                [task.task_id]
            );

            // 通知目标PC（如果在线）
            const socketId = await pcManager.getPCSocketId(targetId);
            if (socketId) {
                io.to(socketId).emit('transferCancelled', {
                    taskId: task.task_id,
                    fileName,
                    interruptOffset
                });
            }
        }

        return { success: true, message: '传输已取消' };
    }

    /**
     * 处理继续传输请求
     * @param {Object} request - 请求数据
     * @returns {Promise<Object>}
     */
    async handleResume(request) {
        if (request.magic !== PROTOCOL_MAGIC.RESUME) {
            throw new Error('无效的协议标识');
        }

        const { targetId, fileName, resumeOffset } = request;

        // 查找传输任务
        const task = await dbPromise.get(
            `SELECT * FROM file_transfers 
             WHERE target_id = ? AND file_name = ? 
             AND (status = 'cancelled' OR status = 'transferring')`,
            [targetId, fileName]
        );

        if (!task) {
            throw new Error('未找到传输任务');
        }

        // 验证偏移量
        if (resumeOffset < 0 || resumeOffset > task.file_size) {
            throw new Error('无效的偏移量');
        }

        // 更新任务状态
        await dbPromise.run(
            `UPDATE file_transfers 
             SET status = 'transferring', start_offset = ?, updated_at = CURRENT_TIMESTAMP
             WHERE task_id = ?`,
            [resumeOffset, task.task_id]
        );

        return {
            success: true,
            message: '准备继续传输',
            startOffset: resumeOffset
        };
    }

    /**
     * 处理删除任务请求
     * @param {Object} request - 请求数据
     * @param {Function} io - Socket.io实例
     * @returns {Promise<Object>}
     */
    async handleDelete(request, io) {
        if (request.magic !== PROTOCOL_MAGIC.DELETE) {
            throw new Error('无效的协议标识');
        }

        const { targetId, fileName } = request;

        // 查找传输任务或已完成文件
        const task = await dbPromise.get(
            `SELECT * FROM file_transfers 
             WHERE target_id = ? AND file_name = ?`,
            [targetId, fileName]
        );

        if (task) {
            // 更新任务状态
            await dbPromise.run(
                `UPDATE file_transfers 
                 SET status = 'deleted', updated_at = CURRENT_TIMESTAMP
                 WHERE task_id = ?`,
                [task.task_id]
            );

            // 删除文件（可选）
            if (task.file_path) {
                try {
                    await fileStorageService.deleteTaskDirectory(task.task_id);
                } catch (err) {
                    console.error('删除文件失败:', err);
                }
            }

            // 通知目标PC（如果在线）
            const socketId = await pcManager.getPCSocketId(targetId);
            if (socketId) {
                io.to(socketId).emit('transferDeleted', {
                    taskId: task.task_id,
                    fileName
                });
            }
        }

        return { success: true, message: '任务已删除' };
    }

    /**
     * 完成文件传输
     * @param {string} taskId - 任务ID
     * @returns {Promise<void>}
     */
    async completeTransfer(taskId) {
        const task = await dbPromise.get(
            'SELECT * FROM file_transfers WHERE task_id = ?',
            [taskId]
        );

        if (!task) return;

        const fileId = `file_${Date.now()}_${uuidv4().substring(0, 8)}`;
        const now = new Date().toISOString();

        // 移动文件到已完成目录
        const completedPath = await fileStorageService.moveToCompleted(
            taskId,
            task.file_name,
            fileId
        );

        // 添加到已完成文件表
        await dbPromise.run(
            `INSERT INTO completed_files 
             (file_id, task_id, sender_id, target_id, file_name, file_size, file_path, completed_time)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
            [
                fileId,
                taskId,
                task.sender_id,
                task.target_id,
                task.file_name,
                task.file_size,
                completedPath,
                now
            ]
        );

        // 更新传输任务状态
        await dbPromise.run(
            `UPDATE file_transfers 
             SET status = 'completed', completed_at = ?, updated_at = ?
             WHERE task_id = ?`,
            [now, now, taskId]
        );
    }

    /**
     * 获取已完成文件列表
     * @param {string} targetId - 目标PC编号（可选）
     * @returns {Promise<Array>}
     */
    async getCompletedFiles(targetId = null) {
        let sql = `SELECT file_id, file_name, sender_id, file_size, completed_time
                   FROM completed_files
                   WHERE 1=1`;
        const params = [];

        if (targetId) {
            sql += ' AND target_id = ?';
            params.push(targetId);
        }

        sql += ' ORDER BY completed_time DESC';

        const files = await dbPromise.all(sql, params);

        return files.map(file => ({
            fileId: file.file_id,
            fileName: file.file_name,
            senderId: file.sender_id,
            fileSize: file.file_size,
            status: 'completed',
            completedTime: file.completed_time
        }));
    }

    /**
     * 获取已完成文件的路径和元信息
     * @param {string} fileId - 文件ID
     * @param {string} targetId - 目标用户ID（用于权限验证）
     * @returns {Promise<Object|null>} 文件信息，包含 file_path, file_name, file_size
     */
    async getCompletedFileInfo(fileId, targetId = null) {
        let sql = `SELECT file_path, file_name, file_size, target_id, sender_id
                   FROM completed_files
                   WHERE file_id = ?`;
        const params = [fileId];

        if (targetId) {
            sql += ' AND target_id = ?';
            params.push(targetId);
        }

        const file = await dbPromise.get(sql, params);
        
        if (!file) {
            return null;
        }

        return {
            filePath: file.file_path,
            fileName: file.file_name,
            fileSize: file.file_size,
            targetId: file.target_id,
            senderId: file.sender_id
        };
    }

    /**
     * 确认下载并删除文件
     * @param {string} fileId - 文件ID
     * @param {string} targetId - 目标用户ID（用于权限验证）
     * @returns {Promise<Object>}
     */
    async confirmDownloadAndDelete(fileId, targetId) {
        // 1. 获取文件信息并验证权限
        const fileInfo = await this.getCompletedFileInfo(fileId, targetId);
        
        if (!fileInfo) {
            throw new Error('文件不存在或无权访问');
        }
        
        // 2. 删除文件
        await fileStorageService.deleteFile(fileInfo.filePath);
        
        // 3. 删除数据库记录
        await dbPromise.run(
            'DELETE FROM completed_files WHERE file_id = ? AND target_id = ?',
            [fileId, targetId]
        );
        
        console.log(`文件已删除: ${fileInfo.fileName} (${fileId})`);
        
        return {
            success: true,
            message: '文件已删除',
            fileId: fileId,
            fileName: fileInfo.fileName
        };
    }

    /**
     * 检查并转发待转发的文件
     * @param {string} username - 账号
     * @param {string} socketId - Socket ID
     * @param {Function} io - Socket.io实例
     */
    async checkAndForwardPendingFiles(username, socketId, io) {
        // 查找待转发的文件（targetId现在是username）
        const pendingTasks = await dbPromise.all(
            `SELECT * FROM file_transfers 
             WHERE target_id = ? AND status = 'transferring'`,
            [username]
        );

        for (const task of pendingTasks) {
            try {
                // 读取文件
                const fileData = await fileStorageService.readFile(task.file_path);
                const base64Data = fileData.toString('base64');

                // 转发文件
                io.to(socketId).emit('fileTransfer', {
                    taskId: task.task_id,
                    senderId: task.sender_id,
                    fileName: task.file_name,
                    fileSize: task.file_size,
                    fileData: base64Data,
                    startOffset: task.start_offset,
                    isComplete: true
                });
            } catch (err) {
                console.error(`转发文件失败 ${task.task_id}:`, err);
            }
        }
    }
}

module.exports = new FileTransferService();
