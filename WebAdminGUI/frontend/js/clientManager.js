// 客户端管理
let clients = [];

// 加载客户端列表
async function loadClients() {
    const tbody = document.getElementById('clientsTableBody');
    tbody.innerHTML = '<tr><td colspan="6" class="loading">加载中...</td></tr>';
    
    try {
        const result = await API.getClients();
        if (result.success) {
            clients = result.clients;
            renderClients();
        }
    } catch (error) {
        tbody.innerHTML = `<tr><td colspan="6" class="error">加载失败: ${error.message}</td></tr>`;
    }
}

// 渲染客户端列表
function renderClients() {
    const tbody = document.getElementById('clientsTableBody');
    
    if (clients.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="loading">暂无客户端</td></tr>';
        return;
    }
    
    tbody.innerHTML = clients.map(client => `
        <tr>
            <td>${client.username}</td>
            <td>${client.pcName || '-'}</td>
            <td><code>${client.machineId}</code></td>
            <td>
                <span class="status-badge status-${client.status}">
                    ${client.status === 'online' ? '在线' : '离线'}
                </span>
            </td>
            <td>${formatTimestamp(client.lastHeartbeat)}</td>
            <td>
                <button class="btn btn-primary" onclick="openPolicyModal('${client.username}')">策略</button>
                <button class="btn btn-primary" onclick="openWhitelistModal('${client.username}')">白名单</button>
                <button class="btn btn-primary" onclick="openLogsModal('${client.username}')">日志</button>
            </td>
        </tr>
    `).join('');
}

// 打开策略模态框
function openPolicyModal(username) {
    const client = clients.find(c => c.username === username);
    if (!client) return;
    
    document.getElementById('policyClientName').textContent = client.username;
    showModal('policyModal');
    
    // 加载当前策略
    loadClientPolicy(username);
}

// 打开白名单模态框
function openWhitelistModal(username) {
    const client = clients.find(c => c.username === username);
    if (!client) return;
    
    document.getElementById('whitelistClientName').textContent = client.username;
    showModal('whitelistModal');
    
    // 加载白名单
    loadClientWhitelist(username);
}

// 打开日志模态框
function openLogsModal(username) {
    const client = clients.find(c => c.username === username);
    if (!client) return;
    
    document.getElementById('logsClientName').textContent = client.username;
    showModal('logsModal');
    
    // 加载日志
    loadClientLogs(username);
}

// 刷新客户端列表
document.getElementById('refreshClientsBtn').addEventListener('click', () => {
    loadClients();
});

// 导出函数供全局使用
window.openPolicyModal = openPolicyModal;
window.openWhitelistModal = openWhitelistModal;
window.openLogsModal = openLogsModal;
window.loadClients = loadClients;
