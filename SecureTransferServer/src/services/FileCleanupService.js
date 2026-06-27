const { dbPromise } = require('../database/db');
const fileStorageService = require('./FileStorageService');
const path = require('path');

class FileCleanupService {
    /**
     * 清理超过指定天数未下载的文件
     * @param {number} days - 保留天数，默认7天
     * @returns {Promise<Object>} 清理结果
     */
    async cleanupOldFiles(days = 7) {
        const cutoffDate = new Date();
        cutoffDate.setDate(cutoffDate.getDate() - days);
        const cutoffTime = cutoffDate.toISOString();

        console.log(`[清理服务] 开始清理 ${cutoffTime} 之前未下载的文件...`);

        try {
            // 查找需要清理的文件（未下载且超过指定天数）
            const filesToClean = await dbPromise.all(
                `SELECT file_id, file_path, file_name, completed_time, target_id 
                 FROM completed_files 
                 WHERE downloaded = 0 
                 AND (completed_time < ? OR created_at < ?)`,
                [cutoffTime, cutoffTime]
            );

            if (filesToClean.length === 0) {
                console.log('[清理服务] 没有需要清理的文件');
                return {
                    success: true,
                    cleanedCount: 0,
                    message: '没有需要清理的文件'
                };
            }

            console.log(`[清理服务] 找到 ${filesToClean.length} 个需要清理的文件`);

            let successCount = 0;
            let failCount = 0;
            const errors = [];

            // 逐个删除文件和数据库记录
            for (const file of filesToClean) {
                try {
                    // 删除物理文件
                    if (file.file_path) {
                        await fileStorageService.deleteFile(file.file_path);
                    }

                    // 删除数据库记录
                    await dbPromise.run(
                        'DELETE FROM completed_files WHERE file_id = ?',
                        [file.file_id]
                    );

                    successCount++;
                    console.log(`[清理服务] 已清理文件: ${file.file_name} (${file.file_id})`);
                } catch (error) {
                    failCount++;
                    const errorMsg = `清理文件失败 ${file.file_name} (${file.file_id}): ${error.message}`;
                    errors.push(errorMsg);
                    console.error(`[清理服务] ${errorMsg}`);
                }
            }

            console.log(`[清理服务] 清理完成: 成功 ${successCount} 个, 失败 ${failCount} 个`);

            return {
                success: true,
                cleanedCount: successCount,
                failedCount: failCount,
                totalFound: filesToClean.length,
                errors: errors.length > 0 ? errors : undefined,
                message: `清理完成: 成功 ${successCount} 个, 失败 ${failCount} 个`
            };
        } catch (error) {
            console.error('[清理服务] 清理过程出错:', error);
            throw error;
        }
    }

    /**
     * 启动定期清理任务
     * @param {number} intervalHours - 清理间隔（小时），默认24小时
     * @param {number} retentionDays - 文件保留天数，默认7天
     */
    startPeriodicCleanup(intervalHours = 24, retentionDays = 7) {
        console.log(`[清理服务] 启动定期清理任务: 每 ${intervalHours} 小时执行一次, 保留 ${retentionDays} 天`);

        // 立即执行一次清理
        this.cleanupOldFiles(retentionDays).catch(err => {
            console.error('[清理服务] 首次清理失败:', err);
        });

        // 设置定期清理
        const intervalMs = intervalHours * 60 * 60 * 1000;
        this.cleanupInterval = setInterval(() => {
            this.cleanupOldFiles(retentionDays).catch(err => {
                console.error('[清理服务] 定期清理失败:', err);
            });
        }, intervalMs);

        console.log(`[清理服务] 定期清理任务已启动`);
    }

    /**
     * 停止定期清理任务
     */
    stopPeriodicCleanup() {
        if (this.cleanupInterval) {
            clearInterval(this.cleanupInterval);
            this.cleanupInterval = null;
            console.log('[清理服务] 定期清理任务已停止');
        }
    }
}

module.exports = new FileCleanupService();
