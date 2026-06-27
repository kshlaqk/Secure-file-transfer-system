/**
 * 管理员认证中间件
 * 简单的 session 验证（生产环境建议使用 JWT）
 */

// 简单的内存存储（生产环境应使用 Redis 或数据库）
const sessions = new Map();

/**
 * 创建会话
 * @param {string} username - 管理员用户名
 * @returns {string} sessionId
 */
function createSession(username) {
    const sessionId = require('crypto').randomBytes(32).toString('hex');
    sessions.set(sessionId, {
        username,
        createdAt: Date.now()
    });
    
    // 24小时后过期
    setTimeout(() => {
        sessions.delete(sessionId);
    }, 24 * 60 * 60 * 1000);
    
    return sessionId;
}

/**
 * 验证会话
 * @param {string} sessionId - 会话ID
 * @returns {Object|null} 会话信息
 */
function verifySession(sessionId) {
    return sessions.get(sessionId) || null;
}

/**
 * 删除会话
 * @param {string} sessionId - 会话ID
 */
function deleteSession(sessionId) {
    sessions.delete(sessionId);
}

/**
 * 认证中间件
 */
function requireAuth(req, res, next) {
    const sessionId = req.headers['x-session-id'] || req.query.sessionId;
    
    if (!sessionId) {
        return res.status(401).json({
            success: false,
            error: '未登录，请先登录'
        });
    }
    
    const session = verifySession(sessionId);
    if (!session) {
        return res.status(401).json({
            success: false,
            error: '会话已过期，请重新登录'
        });
    }
    
    req.session = session;
    req.adminUsername = session.username;
    next();
}

module.exports = {
    createSession,
    verifySession,
    deleteSession,
    requireAuth
};
