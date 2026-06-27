const { dbPromise } = require('../config/database');
const bcrypt = require('bcrypt');

class AdminService {
    /**
     * 管理员登录
     * @param {string} username - 用户名
     * @param {string} password - 密码
     * @returns {Promise<Object>}
     */
    async login(username, password) {
        const admin = await dbPromise.get(
            'SELECT * FROM admins WHERE username = ?',
            [username]
        );
        
        if (!admin) {
            throw new Error('用户名或密码错误');
        }
        
        const passwordMatch = await bcrypt.compare(password, admin.password_hash);
        if (!passwordMatch) {
            throw new Error('用户名或密码错误');
        }
        
        return {
            id: admin.id,
            username: admin.username,
            role: admin.role
        };
    }
    
    /**
     * 创建管理员
     * @param {string} username - 用户名
     * @param {string} password - 密码
     * @returns {Promise<Object>}
     */
    async createAdmin(username, password) {
        // 检查是否已存在
        const existing = await dbPromise.get(
            'SELECT * FROM admins WHERE username = ?',
            [username]
        );
        
        if (existing) {
            throw new Error('管理员用户名已存在');
        }
        
        // 密码长度验证
        if (password.length < 6) {
            throw new Error('密码长度至少6位');
        }
        
        const saltRounds = 10;
        const passwordHash = await bcrypt.hash(password, saltRounds);
        
        await dbPromise.run(
            `INSERT INTO admins (username, password_hash)
             VALUES (?, ?)`,
            [username, passwordHash]
        );
        
        return {
            username,
            message: '管理员创建成功'
        };
    }
    
    /**
     * 获取管理员信息
     * @param {string} username - 用户名
     * @returns {Promise<Object|null>}
     */
    async getAdminByUsername(username) {
        return await dbPromise.get(
            'SELECT * FROM admins WHERE username = ?',
            [username]
        );
    }
    
    /**
     * 修改管理员密码
     * @param {string} username - 用户名
     * @param {string} oldPassword - 旧密码
     * @param {string} newPassword - 新密码
     * @returns {Promise<Object>}
     */
    async changePassword(username, oldPassword, newPassword) {
        const admin = await dbPromise.get(
            'SELECT * FROM admins WHERE username = ?',
            [username]
        );
        
        if (!admin) {
            throw new Error('管理员不存在');
        }
        
        // 验证旧密码
        const passwordMatch = await bcrypt.compare(oldPassword, admin.password_hash);
        if (!passwordMatch) {
            throw new Error('旧密码错误');
        }
        
        // 密码长度验证
        if (newPassword.length < 6) {
            throw new Error('新密码长度至少6位');
        }
        
        // 更新密码
        const saltRounds = 10;
        const passwordHash = await bcrypt.hash(newPassword, saltRounds);
        
        await dbPromise.run(
            `UPDATE admins SET password_hash = ?, updated_at = CURRENT_TIMESTAMP WHERE username = ?`,
            [passwordHash, username]
        );
        
        return {
            message: '密码修改成功'
        };
    }
    
    /**
     * 初始化默认管理员（如果不存在）
     */
    async initializeDefaultAdmin() {
        const existing = await dbPromise.get(
            'SELECT * FROM admins WHERE username = ?',
            ['admin']
        );
        
        if (!existing) {
            const saltRounds = 10;
            const passwordHash = await bcrypt.hash('admin', saltRounds);
            await dbPromise.run(
                `INSERT INTO admins (username, password_hash) VALUES (?, ?)`,
                ['admin', passwordHash]
            );
            console.log('默认管理员已创建: admin/admin');
        }
    }
}

module.exports = new AdminService();
