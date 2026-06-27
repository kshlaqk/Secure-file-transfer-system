const { dbPromise } = require('../database/db');
const bcrypt = require('bcrypt');

class AuthService {
    /**
     * 注册用户
     * @param {string} username - 账号（名字+工号）
     * @param {string} password - 密码
     * @param {string} machineId - 机器码
     * @returns {Promise<Object>}
     */
    async register(username, password, machineId) {
        // 检查用户名是否已存在
        const existingUser = await dbPromise.get(
            'SELECT * FROM users WHERE username = ?',
            [username]
        );
        
        if (existingUser) {
            throw new Error('用户名已存在');
        }
        
        // 检查机器码是否已存在
        const existingMachine = await dbPromise.get(
            'SELECT * FROM users WHERE pc_id = ?',
            [machineId]
        );
        
        if (existingMachine) {
            throw new Error('该机器码已被其他账号绑定');
        }
        
        // 加密密码
        const saltRounds = 10;
        const passwordHash = await bcrypt.hash(password, saltRounds);
        
        // 创建用户（pc_id存储机器码，pc_name使用username）
        await dbPromise.run(
            `INSERT INTO users (username, password_hash, pc_id, pc_name, status)
             VALUES (?, ?, ?, ?, 'offline')`,
            [username, passwordHash, machineId, username]
        );
        
        return {
            username,
            machineId
        };
    }
    
    /**
     * 用户登录
     * @param {string} username - 账号
     * @param {string} password - 密码
     * @returns {Promise<Object>}
     */
    async login(username, password) {
        // 查找用户
        const user = await dbPromise.get(
            'SELECT * FROM users WHERE username = ?',
            [username]
        );
        
        if (!user) {
            throw new Error('用户名或密码错误');
        }
        
        // 验证密码
        const passwordMatch = await bcrypt.compare(password, user.password_hash);
        if (!passwordMatch) {
            throw new Error('用户名或密码错误');
        }
        
        // 更新最后登录时间和状态（可选）
        const now = new Date().toISOString();
        await dbPromise.run(
            `UPDATE users 
             SET last_heartbeat = ?, updated_at = ?
             WHERE username = ?`,
            [now, now, username]
        );
        
        return {
            username: user.username,
            machineId: user.pc_id,  // pc_id存储的是机器码
            status: user.status
        };
    }
    
    /**
     * 根据用户名获取用户信息
     * @param {string} username - 账号
     * @returns {Promise<Object|null>}
     */
    async getUserByUsername(username) {
        return await dbPromise.get(
            'SELECT * FROM users WHERE username = ?',
            [username]
        );
    }
    
    /**
     * 根据机器码获取用户信息
     * @param {string} machineId - 机器码
     * @returns {Promise<Object|null>}
     */
    async getUserByMachineId(machineId) {
        return await dbPromise.get(
            'SELECT * FROM users WHERE pc_id = ?',
            [machineId]
        );
    }
    
    /**
     * 根据用户名获取机器码
     * @param {string} username - 账号
     * @returns {Promise<string|null>}
     */
    async getMachineIdByUsername(username) {
        const user = await dbPromise.get(
            'SELECT pc_id FROM users WHERE username = ?',
            [username]
        );
        return user ? user.pc_id : null;
    }
    
    /**
     * 更新用户PC在线状态（通过username）
     * @param {string} username - 账号
     * @param {string} socketId - WebSocket连接ID（可选）
     * @returns {Promise<void>}
     */
    async updatePCStatus(username, socketId = null) {
        const now = new Date().toISOString();
        
        if (socketId) {
            await dbPromise.run(
                `UPDATE users 
                 SET status = 'online', socket_id = ?, 
                     last_heartbeat = ?, updated_at = ?
                 WHERE username = ?`,
                [socketId, now, now, username]
            );
        } else {
            await dbPromise.run(
                `UPDATE users 
                 SET status = 'online', last_heartbeat = ?, updated_at = ?
                 WHERE username = ?`,
                [now, now, username]
            );
        }
    }
}

module.exports = new AuthService();