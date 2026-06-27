// 日志查看器
let currentLogsUsername = null;

// 加载客户端日志
async function loadClientLogs(username) {
    currentLogsUsername = username;
    const tbody = document.getElementById('logsTableBody');
    const statsDiv = document.getElementById('logsStats');
    
    tbody.innerHTML = '<tr><td colspan="4" class="loading">加载中...</td></tr>';
    statsDiv.innerHTML = '';
    
    try {
        const result = await API.getClientLogs(username, 100, 0);
        if (result.success) {
            renderLogs(result.logs);
            renderLogStats(result.stats);
        }
    } catch (error) {
        tbody.innerHTML = `<tr><td colspan="4" class="error">加载失败: ${error.message}</td></tr>`;
    }
}

// 渲染日志
function renderLogs(logs) {
    const tbody = document.getElementById('logsTableBody');
    
    if (logs.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" class="loading">暂无日志</td></tr>';
        return;
    }
    
    tbody.innerHTML = logs.map(log => {
        const actionClass = log.actionCode === 0 ? 'log-entry-allowed' : 'log-entry-denied';
        return `
            <tr>
                <td>${formatTimestamp(log.timestamp)}</td>
                <td><code>${log.processName}</code></td>
                <td>${log.filePath}</td>
                <td class="${actionClass}">${log.action}</td>
            </tr>
        `;
    }).join('');
}

// 渲染日志统计
function renderLogStats(stats) {
    const statsDiv = document.getElementById('logsStats');
    
    statsDiv.innerHTML = `
        <div class="stats-item">
            <div class="stats-label">总日志数</div>
            <div class="stats-value">${stats.total || 0}</div>
        </div>
        <div class="stats-item">
            <div class="stats-label">允许</div>
            <div class="stats-value" style="color: #27ae60;">${stats.allowed || 0}</div>
        </div>
        <div class="stats-item">
            <div class="stats-label">拒绝</div>
            <div class="stats-value" style="color: #e74c3c;">${stats.denied || 0}</div>
        </div>
    `;
}

// 同步日志
document.getElementById('syncLogsBtn').addEventListener('click', async () => {
    if (!currentLogsUsername) return;
    
    try {
        const result = await API.syncClientLogs(currentLogsUsername);
        if (result.success) {
            alert('已请求客户端同步日志，请稍后刷新查看');
            setTimeout(() => {
                loadClientLogs(currentLogsUsername);
            }, 2000);
        }
    } catch (error) {
        alert('同步失败: ' + error.message);
    }
});

// 刷新日志
document.getElementById('refreshLogsBtn').addEventListener('click', () => {
    if (currentLogsUsername) {
        loadClientLogs(currentLogsUsername);
    }
});
