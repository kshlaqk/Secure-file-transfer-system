const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');

const DB_PATH = path.join(__dirname, '../../database/transfers.db');
const DB_DIR = path.dirname(DB_PATH);

// 确保数据库目录存在
if (!fs.existsSync(DB_DIR)) {
    fs.mkdirSync(DB_DIR, { recursive: true });
}

// 创建数据库连接
const db = new sqlite3.Database(DB_PATH, (err) => {
    if (err) {
        console.error('数据库连接失败:', err.message);
    } else {
        console.log('数据库连接成功');
        initializeDatabase();
    }
});

// 初始化数据库表
function initializeDatabase() {
    // PC列表表
    db.run(`
        CREATE TABLE IF NOT EXISTS pc_list (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            pc_id TEXT UNIQUE NOT NULL,
            pc_name TEXT,
            status TEXT DEFAULT 'offline',
            last_heartbeat DATETIME,
            socket_id TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    `, (err) => {
        if (err) {
            console.error('创建pc_list表失败:', err.message);
        } else {
            console.log('pc_list表已就绪');
        }
    });

    // 文件传输任务表
    db.run(`
        CREATE TABLE IF NOT EXISTS file_transfers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id TEXT UNIQUE NOT NULL,
            sender_id TEXT NOT NULL,
            target_id TEXT NOT NULL,
            file_name TEXT NOT NULL,
            file_size INTEGER NOT NULL,
            bytes_received INTEGER DEFAULT 0,
            file_path TEXT,
            status TEXT DEFAULT 'pending',
            start_offset INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            completed_at DATETIME
        )
    `, (err) => {
        if (err) {
            console.error('创建file_transfers表失败:', err.message);
        } else {
            console.log('file_transfers表已就绪');
        }
    });

    // 已完成文件表
    db.run(`
        CREATE TABLE IF NOT EXISTS completed_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id TEXT UNIQUE NOT NULL,
            task_id TEXT,
            sender_id TEXT NOT NULL,
            target_id TEXT NOT NULL,
            file_name TEXT NOT NULL,
            file_size INTEGER NOT NULL,
            file_path TEXT NOT NULL,
            completed_time DATETIME NOT NULL,
            downloaded BOOLEAN DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    `, (err) => {
        if (err) {
            console.error('创建completed_files表失败:', err.message);
        } else {
            console.log('completed_files表已就绪');
        }
    });

    // 用户表
    db.run(`
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            pc_id TEXT UNIQUE NOT NULL,
            pc_name TEXT,
            status TEXT DEFAULT 'offline',
            last_heartbeat DATETIME,
            socket_id TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    `, (err) => {
        if (err) {
            console.error('创建users表失败:', err.message);
        } else {
            console.log('users表已就绪');
        }
    });

    // 驱动日志表（从客户端同步的日志）
    db.run(`
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
    `, (err) => {
        if (err) {
            console.error('创建driver_logs表失败:', err.message);
        } else {
            console.log('driver_logs表已就绪');
        }
    });

    // 创建索引
    db.run(`
        CREATE INDEX IF NOT EXISTS idx_driver_logs_username 
        ON driver_logs(username)
    `, (err) => {
        if (err) {
            console.error('创建索引失败:', err.message);
        }
    });
    
    db.run(`
        CREATE INDEX IF NOT EXISTS idx_driver_logs_timestamp 
        ON driver_logs(timestamp DESC)
    `, (err) => {
        if (err) {
            console.error('创建索引失败:', err.message);
        }
    });
}

// 封装Promise化的数据库操作
const dbPromise = {
    get: (sql, params = []) => {
        return new Promise((resolve, reject) => {
            db.get(sql, params, (err, row) => {
                if (err) reject(err);
                else resolve(row);
            });
        });
    },

    all: (sql, params = []) => {
        return new Promise((resolve, reject) => {
            db.all(sql, params, (err, rows) => {
                if (err) reject(err);
                else resolve(rows);
            });
        });
    },

    run: (sql, params = []) => {
        return new Promise((resolve, reject) => {
            db.run(sql, params, function(err) {
                if (err) reject(err);
                else resolve({ lastID: this.lastID, changes: this.changes });
            });
        });
    }
};

module.exports = { db, dbPromise };
