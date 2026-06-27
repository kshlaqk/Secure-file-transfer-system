const { dbPromise } = require('../../../SecureTransferServer/src/database/db');

async function initializeDatabase() {
    try {
        // 管理员表
        await dbPromise.run(`
            CREATE TABLE IF NOT EXISTS admins (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                role TEXT DEFAULT 'admin',
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        `);
        console.log('admins 表已就绪');

        // 客户端策略表
        await dbPromise.run(`
            CREATE TABLE IF NOT EXISTS client_policies (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                protected_extensions TEXT DEFAULT '.docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.temp;.txt',
                protected_paths TEXT DEFAULT '',
                enable_encryption BOOLEAN DEFAULT 0,
                policy_version TEXT NOT NULL,
                updated_by TEXT,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (username) REFERENCES users(username)
            )
        `);
        console.log('client_policies 表已就绪');

        // 客户端白名单表
        await dbPromise.run(`
            CREATE TABLE IF NOT EXISTS client_whitelist (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL,
                process_path TEXT NOT NULL,
                is_active BOOLEAN DEFAULT 1,
                added_by TEXT,
                added_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(username, process_path),
                FOREIGN KEY (username) REFERENCES users(username)
            )
        `);
        console.log('client_whitelist 表已就绪');

        // 驱动日志表
        await dbPromise.run(`
            CREATE TABLE IF NOT EXISTS driver_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                process_name TEXT NOT NULL,
                file_path TEXT NOT NULL,
                action INTEGER NOT NULL,
                synced_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (username) REFERENCES users(username)
            )
        `);
        console.log('driver_logs 表已就绪');

        // 创建索引以提高查询性能
        await dbPromise.run(`
            CREATE INDEX IF NOT EXISTS idx_driver_logs_username 
            ON driver_logs(username)
        `);
        
        await dbPromise.run(`
            CREATE INDEX IF NOT EXISTS idx_driver_logs_timestamp 
            ON driver_logs(timestamp DESC)
        `);

        return true;
    } catch (error) {
        console.error('数据库初始化失败:', error);
        throw error;
    }
}

module.exports = {
    initializeDatabase
};
