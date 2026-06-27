const { dbPromise } = require('../database/db');
const authService = require('./AuthService');

class PCManagerService {
    /**
     * 注册或更新PC信息
     * @param {string} pcId - PC编号
     * @param {string} pcName - PC名称
     * @param {string} socketId - WebSocket连接ID
     * @returns {Promise<Object>}
     */
    async registerPC(pcId, pcName, socketId) {
        const now = new Date().toISOString();
        
        // 检查PC是否已存在
        const existing = await dbPromise.get(
            'SELECT * FROM pc_list WHERE pc_id = ?',
            [pcId]
        );

        if (existing) {
            // 更新现有PC
            await dbPromise.run(
                `UPDATE pc_list 
                 SET pc_name = ?, status = 'online', socket_id = ?, 
                     last_heartbeat = ?, updated_at = ?
                 WHERE pc_id = ?`,
                [pcName, socketId, now, now, pcId]
            );
        } else {
            // 创建新PC
            await dbPromise.run(
                `INSERT INTO pc_list (pc_id, pc_name, status, socket_id, last_heartbeat)
                 VALUES (?, ?, 'online', ?, ?)`,
                [pcId, pcName, socketId, now]
            );
        }

        return { pcId, pcName, status: 'online', socketId };
    }

    /**
     * 更新PC心跳
     * @param {string} pcId - PC编号
     * @returns {Promise<void>}
     */
    async updateHeartbeat(pcId) {
        const now = new Date().toISOString();
        await dbPromise.run(
            `UPDATE pc_list 
             SET last_heartbeat = ?, status = 'online', updated_at = ?
             WHERE pc_id = ?`,
            [now, now, pcId]
        );
    }

    /**
     * 设置PC为离线状态
     * @param {string} socketId - WebSocket连接ID
     * @returns {Promise<void>}
     */
    async setPCOffline(socketId) {
        await dbPromise.run(
            `UPDATE pc_list 
             SET status = 'offline', socket_id = NULL, updated_at = CURRENT_TIMESTAMP
             WHERE socket_id = ?`,
            [socketId]
        );
    }

    /**
     * 检查PC是否在线（通过username）
     * @param {string} username - 账号
     * @returns {Promise<boolean>}
     */
    async isPCOnline(username) {
        // 从users表查询在线状态
        const user = await dbPromise.get(
            'SELECT status FROM users WHERE username = ?',
            [username]
        );
        return user && user.status === 'online';
    }
    
    /**
     * 获取PC的Socket ID（通过username）
     * @param {string} username - 账号
     * @returns {Promise<string|null>}
     */
    async getPCSocketId(username) {
        const user = await dbPromise.get(
            'SELECT socket_id FROM users WHERE username = ? AND status = ?',
            [username, 'online']
        );
        return user ? user.socket_id : null;
    }
    
    /**
     * 检查PC是否在线（通过pcId，兼容旧接口）
     * @param {string} pcId - PC编号或机器码
     * @returns {Promise<boolean>}
     */
    async isPCOnlineByPCId(pcId) {
        const pc = await dbPromise.get(
            'SELECT status FROM pc_list WHERE pc_id = ?',
            [pcId]
        );
        return pc && pc.status === 'online';
    }
    
    /**
     * 获取PC的Socket ID（通过pcId，兼容旧接口）
     * @param {string} pcId - PC编号或机器码
     * @returns {Promise<string|null>}
     */
    async getPCSocketIdByPCId(pcId) {
        const pc = await dbPromise.get(
            'SELECT socket_id FROM pc_list WHERE pc_id = ? AND status = ?',
            [pcId, 'online']
        );
        return pc ? pc.socket_id : null;
    }

    /**
     * 获取PC信息
     * @param {string} pcId - PC编号
     * @returns {Promise<Object|null>}
     */
    async getPCInfo(pcId) {
        return await dbPromise.get(
            'SELECT * FROM pc_list WHERE pc_id = ?',
            [pcId]
        );
    }

    /**
     * 获取所有在线PC列表
     * @returns {Promise<Array>}
     */
    async getOnlinePCs() {
        return await dbPromise.all(
            'SELECT * FROM pc_list WHERE status = ?',
            ['online']
        );
    }
}

module.exports = new PCManagerService();
