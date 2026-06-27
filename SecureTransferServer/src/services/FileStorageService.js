const fs = require('fs').promises;
const path = require('path');
const { v4: uuidv4 } = require('uuid');

const UPLOADS_DIR = path.join(__dirname, '../../uploads');
const COMPLETED_DIR = path.join(__dirname, '../../completed');

// 确保目录存在
async function ensureDirectories() {
    await fs.mkdir(UPLOADS_DIR, { recursive: true });
    await fs.mkdir(COMPLETED_DIR, { recursive: true });
}

// 初始化时创建目录
ensureDirectories();

class FileStorageService {
    /**
     * 保存文件到临时目录
     * @param {string} taskId - 任务ID
     * @param {string} fileName - 文件名
     * @param {Buffer} fileData - 文件数据（已解码的Buffer）
     * @param {number} startOffset - 起始偏移量（用于断点续传）
     * @returns {Promise<string>} 文件路径
     */
    async saveFile(taskId, fileName, fileData, startOffset = 0) {
        await ensureDirectories();
        
        const taskDir = path.join(UPLOADS_DIR, taskId);
        await fs.mkdir(taskDir, { recursive: true });
        
        const filePath = path.join(taskDir, fileName);
        
        if (startOffset === 0) {
            // 新文件，直接写入
            await fs.writeFile(filePath, fileData);
        } else {
            // 断点续传，追加写入
            const fileHandle = await fs.open(filePath, 'a');
            await fileHandle.writeFile(fileData);
            await fileHandle.close();
        }
        
        return filePath;
    }

    /**
     * 读取文件
     * @param {string} filePath - 文件路径
     * @returns {Promise<Buffer>} 文件数据
     */
    async readFile(filePath) {
        return await fs.readFile(filePath);
    }

    /**
     * 删除文件
     * @param {string} filePath - 文件路径
     */
    async deleteFile(filePath) {
        try {
            await fs.unlink(filePath);
        } catch (err) {
            if (err.code !== 'ENOENT') {
                throw err;
            }
        }
    }

    /**
     * 删除任务目录
     * @param {string} taskId - 任务ID
     */
    async deleteTaskDirectory(taskId) {
        const taskDir = path.join(UPLOADS_DIR, taskId);
        try {
            await fs.rm(taskDir, { recursive: true, force: true });
        } catch (err) {
            if (err.code !== 'ENOENT') {
                throw err;
            }
        }
    }

    /**
     * 移动文件到已完成目录
     * @param {string} taskId - 任务ID
     * @param {string} fileName - 文件名
     * @param {string} fileId - 文件ID
     * @returns {Promise<string>} 新文件路径
     */
    async moveToCompleted(taskId, fileName, fileId) {
        await ensureDirectories();
        
        const sourcePath = path.join(UPLOADS_DIR, taskId, fileName);
        // 保留原始文件名，使用 fileId 作为前缀避免冲突
        const newFileName = `${fileId}_${fileName}`;
        const destPath = path.join(COMPLETED_DIR, newFileName);
        
        await fs.rename(sourcePath, destPath);
        
        // 删除任务目录（如果为空）
        const taskDir = path.join(UPLOADS_DIR, taskId);
        try {
            const files = await fs.readdir(taskDir);
            if (files.length === 0) {
                await fs.rmdir(taskDir);
            }
        } catch (err) {
            // 忽略错误
        }
        
        return destPath;
    }

    /**
     * 获取文件大小
     * @param {string} filePath - 文件路径
     * @returns {Promise<number>} 文件大小（字节）
     */
    async getFileSize(filePath) {
        const stats = await fs.stat(filePath);
        return stats.size;
    }

    /**
     * 检查文件是否存在
     * @param {string} filePath - 文件路径
     * @returns {Promise<boolean>}
     */
    async fileExists(filePath) {
        try {
            await fs.access(filePath);
            return true;
        } catch {
            return false;
        }
    }
}

module.exports = new FileStorageService();
