const { dbPromise } = require('../config/database');

class WhitelistService {
    /**
     * 获取客户端白名单
     * @param {string} username - 客户端用户名
     * @returns {Promise<Array>}
     */
    async getClientWhitelist(username) {
        const whitelist = await dbPromise.all(
            `SELECT process_path, is_active, added_by, added_at
             FROM client_whitelist
             WHERE username = ? AND is_active = 1
             ORDER BY added_at DESC`,
            [username]
        );
        
        // 如果数据库中没有白名单，返回驱动默认白名单
        if (whitelist.length === 0) {
            return [
                {
                    processPath: 'Windows\\System32\\notepad.exe',
                    isActive: true,
                    addedBy: 'system',
                    addedAt: null,
                    isDefault: true  // 标记为默认白名单
                },
                {
                    processPath: 'whitelist\\client\\SecureTransferClient.exe',
                    isActive: true,
                    addedBy: 'system',
                    addedAt: null,
                    isDefault: true
                },
                {
                    processPath: 'Windows\\explorer.exe',
                    isActive: true,
                    addedBy: 'system',
                    addedAt: null,
                    isDefault: true
                },
                {
                    processPath: 'Windows\\System32\\svchost.exe',
                    isActive: true,
                    addedBy: 'system',
                    addedAt: null,
                    isDefault: true
                },
                {
                    processPath: 'Windows\\System32\\SearchProtocolHost.exe',
                    isActive: true,
                    addedBy: 'system',
                    addedAt: null,
                    isDefault: true
                }
            ];
        }
        
        return whitelist.map(item => ({
            processPath: item.process_path,
            isActive: item.is_active === 1,
            addedBy: item.added_by,
            addedAt: item.added_at,
            isDefault: false
        }));
    }
    
    /**
     * 添加白名单项
     * @param {string} username - 客户端用户名
     * @param {string} processPath - 进程路径
     * @param {string} addedBy - 添加者（管理员用户名）
     * @returns {Promise<void>}
     */
    async addWhitelistItem(username, processPath, addedBy) {
        try {
            await dbPromise.run(
                `INSERT INTO client_whitelist (username, process_path, added_by)
                 VALUES (?, ?, ?)`,
                [username, processPath, addedBy]
            );
        } catch (err) {
            // 如果已存在，更新为激活状态
            if (err.message && err.message.includes('UNIQUE')) {
                await dbPromise.run(
                    `UPDATE client_whitelist 
                     SET is_active = 1, added_by = ?, added_at = CURRENT_TIMESTAMP
                     WHERE username = ? AND process_path = ?`,
                    [addedBy, username, processPath]
                );
            } else {
                throw err;
            }
        }
    }
    
    /**
     * 移除白名单项
     * @param {string} username - 客户端用户名
     * @param {string} processPath - 进程路径
     * @returns {Promise<void>}
     */
    async removeWhitelistItem(username, processPath) {
        await dbPromise.run(
            `UPDATE client_whitelist 
             SET is_active = 0
             WHERE username = ? AND process_path = ?`,
            [username, processPath]
        );
    }
    
    /**
     * 推送白名单到客户端
     * @param {string} username - 客户端用户名
     * @param {Object} io - Socket.io 实例（主服务器的 io）
     * @returns {Promise<boolean>}
     */
    async pushWhitelistToClient(username, io) {
        const whitelist = await this.getClientWhitelist(username);
        
        const user = await dbPromise.get(
            'SELECT socket_id FROM users WHERE username = ? AND status = ?',
            [username, 'online']
        );
        
        if (!user || !user.socket_id) {
            throw new Error('客户端未上线');
        }
        
        // 通过主服务器的 WebSocket 推送白名单
        io.to(user.socket_id).emit('whitelistUpdate', {
            whitelist: whitelist.map(item => item.processPath)
        });
        
        return true;
    }
    
    /**
     * 批量添加白名单项
     * @param {string} username - 客户端用户名
     * @param {Array<string>} processPaths - 进程路径数组
     * @param {string} addedBy - 添加者
     * @returns {Promise<void>}
     */
    async addWhitelistItems(username, processPaths, addedBy) {
        for (const processPath of processPaths) {
            await this.addWhitelistItem(username, processPath, addedBy);
        }
    }
}

module.exports = new WhitelistService();
