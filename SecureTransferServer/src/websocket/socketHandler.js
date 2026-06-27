const pcManager = require('../services/PCManagerService');
const fileTransferService = require('../services/FileTransferService');
const { dbPromise } = require('../database/db');

function setupSocketHandlers(io) {
    io.on('connection', (socket) => {
        console.log('========================================');
        console.log(`[WebSocket] ===== 客户端连接 =====`);
        console.log(`[WebSocket] Socket ID: ${socket.id}`);
        console.log(`[WebSocket] 客户端IP: ${socket.handshake.address}`);
        console.log(`[WebSocket] 连接时间: ${new Date().toISOString()}`);
        console.log(`[WebSocket] 请求头:`, JSON.stringify(socket.handshake.headers, null, 2));
        console.log('========================================');

        // 设置超时检测：如果30秒内没有收到 register 事件，记录警告
        const registerTimeout = setTimeout(() => {
            console.warn(`[WebSocket] ⚠ 警告: 客户端 ${socket.id} 连接后30秒内未发送 register 事件`);
            console.warn(`[WebSocket] 这可能表示客户端连接成功但未发送注册信息`);
        }, 30000);

        // PC注册（通过username）
        socket.on('register', async (data) => {
            try {
                clearTimeout(registerTimeout); // 清除超时检测
                
                console.log('========================================');
                console.log(`[WebSocket] ===== 收到 register 事件 =====`);
                console.log(`[WebSocket] Socket ID: ${socket.id}`);
                console.log(`[WebSocket] Register 数据:`, JSON.stringify(data, null, 2));
                console.log('========================================');
                
                const { username, machineId } = data;
                
                if (!username) {
                    console.error(`[WebSocket] ✗ register 事件缺少 username`);
                    socket.emit('error', { message: '账号不能为空' });
                    return;
                }
                
                if (!machineId) {
                    console.error(`[WebSocket] ✗ register 事件缺少 machineId`);
                    socket.emit('error', { message: '机器码不能为空' });
                    return;
                }

                console.log(`[WebSocket] 验证用户信息...`);
                console.log(`[WebSocket] 用户名: ${username}`);
                console.log(`[WebSocket] 机器码: ${machineId}`);

                // 验证username和machineId是否匹配
                const authService = require('../services/AuthService');
                const user = await authService.getUserByUsername(username);
                if (!user) {
                    console.error(`[WebSocket] ✗ 用户不存在: ${username}`);
                    socket.emit('error', { message: '账号不存在' });
                    return;
                }
                
                console.log(`[WebSocket] 数据库中的用户信息:`, {
                    username: user.username,
                    pc_id: user.pc_id,
                    status: user.status
                });
                
                if (user.pc_id !== machineId) {
                    console.error(`[WebSocket] ✗ 机器码不匹配`);
                    console.error(`[WebSocket] 期望的机器码: ${user.pc_id}`);
                    console.error(`[WebSocket] 实际的机器码: ${machineId}`);
                    socket.emit('error', { message: '机器码不匹配' });
                    return;
                }

                console.log(`[WebSocket] ✓ 验证通过，开始更新用户状态...`);

                // 更新用户在线状态
                await authService.updatePCStatus(username, socket.id);
                console.log(`[WebSocket] ✓ PC注册成功: ${username} (socket_id: ${socket.id})`);

                // 验证 socket_id 是否已保存
                const updatedUser = await authService.getUserByUsername(username);
                console.log(`[WebSocket] 更新后的用户信息:`, {
                    username: updatedUser.username,
                    socket_id: updatedUser.socket_id,
                    status: updatedUser.status
                });

                // 广播客户端上线事件（供Web管理后台监听）
                io.emit('clientOnline', { username: username });
                console.log(`[WebSocket] ✓ 客户端 ${username} 上线，已广播通知`);

                // 检查并转发待转发的文件
                await fileTransferService.checkAndForwardPendingFiles(
                    username,
                    socket.id,
                    io
                );

                // 获取并推送白名单（实现持久化）
                try {
                    const whitelist = await dbPromise.all(
                        `SELECT process_path 
                         FROM client_whitelist 
                         WHERE username = ? AND is_active = 1 
                         ORDER BY added_at DESC`,
                        [username]
                    );
                    
                    // 如果数据库中没有白名单，使用默认白名单
                    let whitelistPaths = [];
                    if (whitelist.length === 0) {
                        // 返回驱动默认白名单
                        whitelistPaths = [
                            'Windows\\System32\\notepad.exe',
                            'whitelist\\client\\SecureTransferClient.exe',
                            'Windows\\explorer.exe',
                            'Windows\\System32\\svchost.exe',
                            'Windows\\System32\\SearchProtocolHost.exe'
                        ];
                    } else {
                        whitelistPaths = whitelist.map(item => item.process_path);
                    }
                    
                    // 推送白名单到客户端
                    socket.emit('whitelistUpdate', { whitelist: whitelistPaths });
                    console.log(`[WebSocket] ✓ 已推送白名单给客户端 ${username}，共 ${whitelistPaths.length} 项`);
                } catch (whitelistError) {
                    // 如果表不存在或其他错误，只记录日志，不影响注册流程
                    console.warn(`[WebSocket] ⚠ 获取白名单失败:`, whitelistError.message);
                    // 如果表不存在，尝试推送默认白名单
                    try {
                        const defaultWhitelist = [
                            'Windows\\System32\\notepad.exe',
                            'whitelist\\client\\SecureTransferClient.exe',
                            'Windows\\explorer.exe',
                            'Windows\\System32\\svchost.exe',
                            'Windows\\System32\\SearchProtocolHost.exe'
                        ];
                        socket.emit('whitelistUpdate', { whitelist: defaultWhitelist });
                        console.log(`[WebSocket] ✓ 已推送默认白名单给客户端 ${username}`);
                    } catch (err) {
                        console.error(`[WebSocket] ✗ 推送默认白名单失败:`, err);
                    }
                }

                // 获取并推送策略（实现持久化）
                try {
                    const policy = await dbPromise.get(
                        `SELECT protected_extensions, enable_encryption, policy_version 
                         FROM client_policies 
                         WHERE username = ?`,
                        [username]
                    );
                    
                    if (policy) {
                        // 推送策略到客户端
                        socket.emit('policyUpdate', {
                            version: policy.policy_version || '1.0.0',
                            protectedExtensions: policy.protected_extensions || '.docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.temp;.txt',
                            enableEncryption: policy.enable_encryption === 1
                        });
                        console.log(`[WebSocket] ✓ 已推送策略给客户端 ${username}，版本: ${policy.policy_version || '1.0.0'}`);
                    } else {
                        // 如果数据库中没有策略，推送默认策略
                        const defaultPolicy = {
                            version: '1.0.0',
                            protectedExtensions: '.docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.temp;.txt',
                            enableEncryption: false
                        };
                        socket.emit('policyUpdate', defaultPolicy);
                        console.log(`[WebSocket] ✓ 已推送默认策略给客户端 ${username}`);
                    }
                } catch (policyError) {
                    console.warn(`[WebSocket] ⚠ 获取并推送策略失败:`, policyError.message);
                    // 如果获取失败，尝试推送默认策略
                    try {
                        const defaultPolicy = {
                            version: '1.0.0',
                            protectedExtensions: '.docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.temp;.txt',
                            enableEncryption: false
                        };
                        socket.emit('policyUpdate', defaultPolicy);
                        console.log(`[WebSocket] ✓ 已推送默认策略给客户端 ${username}`);
                    } catch (err) {
                        console.error(`[WebSocket] ✗ 推送默认策略失败:`, err);
                    }
                }

                socket.emit('registered', {
                    success: true,
                    username: username
                });
                console.log(`[WebSocket] ✓ 已发送 registered 确认给客户端 ${username}`);
                console.log('========================================');
            } catch (error) {
                console.error(`[WebSocket] ✗ PC注册错误:`, error);
                console.error(`[WebSocket] 错误堆栈:`, error.stack);
                socket.emit('error', { message: error.message });
            }
        });

        // 心跳
        socket.on('heartbeat', async (data) => {
            try {
                const { username } = data;
                if (username) {
                    const authService = require('../services/AuthService');
                    await authService.updatePCStatus(username);
                    console.log(`[WebSocket] 收到心跳: ${username} (${socket.id})`);
                }
            } catch (error) {
                console.error(`[WebSocket] ✗ 心跳更新错误:`, error);
            }
        });

        // 文件接收确认
        socket.on('fileReceived', async (data) => {
            try {
                const { taskId } = data;
                
                // 完成传输任务
                await fileTransferService.completeTransfer(taskId);
                
                console.log(`[WebSocket] 文件接收确认: ${taskId} (${socket.id})`);
            } catch (error) {
                console.error(`[WebSocket] ✗ 文件接收确认错误:`, error);
            }
        });

        // 接收客户端同步的日志
        socket.on('syncLogs', async (data) => {
            try {
                const { username, logs } = data;
                if (username && Array.isArray(logs)) {
                    const logService = require('../services/LogService');
                    await logService.saveClientLogs(username, logs);
                    console.log(`[WebSocket] 收到客户端 ${username} 的日志同步，共 ${logs.length} 条`);
                }
            } catch (error) {
                console.error(`[WebSocket] ✗ 处理日志同步错误:`, error);
            }
        });

        // 客户端确认策略更新
        socket.on('policyUpdated', async (data) => {
            try {
                const { username, version } = data;
                console.log(`[WebSocket] 客户端 ${username} 已更新策略，版本: ${version}`);
            } catch (error) {
                console.error(`[WebSocket] ✗ 处理策略更新确认错误:`, error);
            }
        });

        // 客户端确认白名单更新
        socket.on('whitelistUpdated', async (data) => {
            try {
                const { username } = data;
                console.log(`[WebSocket] 客户端 ${username} 已更新白名单`);
            } catch (error) {
                console.error(`[WebSocket] ✗ 处理白名单更新确认错误:`, error);
            }
        });

        // 断开连接
        socket.on('disconnect', async (reason) => {
            try {
                console.log('========================================');
                console.log(`[WebSocket] ===== 客户端断开连接 =====`);
                console.log(`[WebSocket] Socket ID: ${socket.id}`);
                console.log(`[WebSocket] 断开原因: ${reason}`);
                console.log(`[WebSocket] 断开时间: ${new Date().toISOString()}`);
                
                // 通过socketId查找username并设置为离线
                const authService = require('../services/AuthService');
                const user = await dbPromise.get(
                    'SELECT username FROM users WHERE socket_id = ?',
                    [socket.id]
                );
                if (user) {
                    console.log(`[WebSocket] 找到用户: ${user.username}`);
                    await dbPromise.run(
                        `UPDATE users 
                         SET status = 'offline', socket_id = NULL, updated_at = CURRENT_TIMESTAMP
                         WHERE username = ?`,
                        [user.username]
                    );
                    
                    // 广播客户端离线事件（供Web管理后台监听）
                    io.emit('clientOffline', { username: user.username });
                    console.log(`[WebSocket] ✓ 客户端 ${user.username} 离线，已广播通知`);
                } else {
                    console.log(`[WebSocket] ⚠ 断开连接的客户端 ${socket.id} 未找到对应的用户记录`);
                }
                console.log('========================================');
            } catch (error) {
                console.error(`[WebSocket] ✗ 处理断开连接错误:`, error);
                console.error(`[WebSocket] 错误堆栈:`, error.stack);
            }
        });

        // 错误处理
        socket.on('error', (error) => {
            console.error(`[WebSocket] ✗ Socket错误 (${socket.id}):`, error);
            console.error(`[WebSocket] 错误堆栈:`, error.stack);
        });
    });

    // 定期清理离线PC（可选）
    setInterval(async () => {
        // 可以添加清理逻辑，比如超过5分钟没有心跳的PC标记为离线
    }, 5 * 60 * 1000);
}

module.exports = setupSocketHandlers;
