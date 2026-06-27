const { dbPromise } = require('../config/database');

class LogService {
    /**
     * 保存客户端同步的日志
     * @param {string} username - 客户端用户名
     * @param {Array} logs - 日志数组
     * @returns {Promise<void>}
     */
    async saveClientLogs(username, logs) {
        if (!Array.isArray(logs) || logs.length === 0) {
            return;
        }
        
        for (const log of logs) {
            try {
                await dbPromise.run(
                    `INSERT INTO driver_logs (username, timestamp, process_name, file_path, action)
                     VALUES (?, ?, ?, ?, ?)`,
                    [
                        username,
                        log.timestamp || Date.now(),
                        log.processName || '',
                        log.filePath || '',
                        log.action !== undefined ? log.action : 1
                    ]
                );
            } catch (err) {
                // 忽略重复插入错误
                if (!err.message || !err.message.includes('UNIQUE')) {
                    console.error('保存日志失败:', err);
                }
            }
        }
    }
    
    /**
     * 获取客户端日志
     * @param {string} username - 客户端用户名
     * @param {number} limit - 限制条数
     * @param {number} offset - 偏移量
     * @returns {Promise<Array>}
     */
    async getClientLogs(username, limit = 100, offset = 0) {
        const logs = await dbPromise.all(
            `SELECT timestamp, process_name, file_path, action, synced_at
             FROM driver_logs
             WHERE username = ?
             ORDER BY timestamp DESC
             LIMIT ? OFFSET ?`,
            [username, limit, offset]
        );
        
        return logs.map(log => ({
            timestamp: log.timestamp,
            processName: log.process_name,
            filePath: log.file_path,
            action: log.action === 1 ? '允许' : '拒绝',
            actionCode: log.action,
            syncedAt: log.synced_at
        }));
    }
    
    /**
     * 获取客户端日志统计
     * @param {string} username - 客户端用户名
     * @returns {Promise<Object>}
     */
    async getClientLogStats(username) {
        const stats = await dbPromise.get(
            `SELECT 
                COUNT(*) as total,
                SUM(CASE WHEN action = 1 THEN 1 ELSE 0 END) as allowed,
                SUM(CASE WHEN action = 0 THEN 1 ELSE 0 END) as denied
             FROM driver_logs
             WHERE username = ?`,
            [username]
        );
        
        return {
            total: stats.total || 0,
            allowed: stats.allowed || 0,
            denied: stats.denied || 0
        };
    }
    
    /**
     * 请求客户端同步日志
     * @param {string} username - 客户端用户名
     * @param {Object} io - Socket.io 实例（主服务器的 io）
     * @returns {Promise<boolean>}
     */
    async requestLogSync(username, io) {
        const user = await dbPromise.get(
            'SELECT socket_id FROM users WHERE username = ? AND status = ?',
            [username, 'online']
        );
        
        if (!user || !user.socket_id) {
            throw new Error('客户端未上线');
        }
        
        // 通过主服务器的 WebSocket 请求日志同步
        io.to(user.socket_id).emit('requestLogs', {});
        
        return true;
    }
    
    /**
     * 删除客户端日志
     * @param {string} username - 客户端用户名
     * @param {number} days - 保留天数（删除 days 天前的日志）
     * @returns {Promise<number>} 删除的条数
     */
    async deleteOldLogs(username, days = 30) {
        const cutoffTime = Date.now() - (days * 24 * 60 * 60 * 1000);
        
        const result = await dbPromise.run(
            `DELETE FROM driver_logs
             WHERE username = ? AND timestamp < ?`,
            [username, cutoffTime]
        );
        
        return result.changes || 0;
    }
}

module.exports = new LogService();
