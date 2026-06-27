const { dbPromise } = require('../config/database');
const { v4: uuidv4 } = require('uuid');

class PolicyService {
    /**
     * 获取客户端策略
     * @param {string} username - 客户端用户名
     * @returns {Promise<Object>}
     */
    async getClientPolicy(username) {
        const policy = await dbPromise.get(
            'SELECT * FROM client_policies WHERE username = ?',
            [username]
        );
        
        if (!policy) {
            // 返回默认策略
            return {
                username,
                protectedExtensions: '.docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.temp;.txt',
                enableEncryption: false,
                policyVersion: '1.0.0'
            };
        }
        
        return {
            username: policy.username,
            protectedExtensions: policy.protected_extensions,
            enableEncryption: policy.enable_encryption === 1,
            policyVersion: policy.policy_version,
            updatedBy: policy.updated_by,
            updatedAt: policy.updated_at
        };
    }
    
    /**
     * 更新客户端策略
     * @param {string} username - 客户端用户名
     * @param {Object} policyData - 策略数据
     * @param {string} updatedBy - 更新者（管理员用户名）
     * @returns {Promise<string>} 新的策略版本号
     */
    async updateClientPolicy(username, policyData, updatedBy) {
        const version = Date.now().toString(36) + '-' + uuidv4().substring(0, 8);
        
        const existing = await dbPromise.get(
            'SELECT * FROM client_policies WHERE username = ?',
            [username]
        );
        
        if (existing) {
            await dbPromise.run(
                `UPDATE client_policies 
                 SET protected_extensions = ?, protected_paths = '', 
                     enable_encryption = ?, policy_version = ?, 
                     updated_by = ?, updated_at = CURRENT_TIMESTAMP
                 WHERE username = ?`,
                [
                    policyData.protectedExtensions || '',
                    policyData.enableEncryption ? 1 : 0,
                    version,
                    updatedBy,
                    username
                ]
            );
        } else {
            await dbPromise.run(
                `INSERT INTO client_policies 
                 (username, protected_extensions, protected_paths, 
                  enable_encryption, policy_version, updated_by)
                 VALUES (?, ?, '', ?, ?, ?)`,
                [
                    username,
                    policyData.protectedExtensions || '',
                    policyData.enableEncryption ? 1 : 0,
                    version,
                    updatedBy
                ]
            );
        }
        
        return version;
    }
    
    /**
     * 推送策略到客户端（通过WebSocket）
     * @param {string} username - 客户端用户名
     * @param {Object} io - Socket.io 实例（主服务器的 io）
     * @returns {Promise<boolean>}
     */
    async pushPolicyToClient(username, io) {
        const policy = await this.getClientPolicy(username);
        
        // 获取客户端的socket_id（从主服务器数据库）
        const user = await dbPromise.get(
            'SELECT socket_id FROM users WHERE username = ? AND status = ?',
            [username, 'online']
        );
        
        if (!user || !user.socket_id) {
            throw new Error('客户端未上线');
        }
        
        // 通过主服务器的 WebSocket 推送策略
        io.to(user.socket_id).emit('policyUpdate', {
            version: policy.policyVersion,
            protectedExtensions: policy.protectedExtensions,
            enableEncryption: policy.enableEncryption
        });
        
        return true;
    }
    
    /**
     * 获取所有客户端的策略列表
     * @returns {Promise<Array>}
     */
    async getAllClientPolicies() {
        const policies = await dbPromise.all(
            `SELECT cp.*, u.pc_name, u.status
             FROM client_policies cp
             RIGHT JOIN users u ON cp.username = u.username
             ORDER BY u.username`
        );
        
        return policies.map(p => ({
            username: p.username || u.username,
            pcName: p.pc_name,
            status: p.status,
            protectedExtensions: p.protected_extensions || '.docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.temp;.txt',
            enableEncryption: p.enable_encryption === 1,
            policyVersion: p.policy_version || '1.0.0',
            updatedBy: p.updated_by,
            updatedAt: p.updated_at
        }));
    }
}

module.exports = new PolicyService();
