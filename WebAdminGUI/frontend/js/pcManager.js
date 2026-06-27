// PC管理
let pcList = [];

// 将pcList暴露到全局
window.pcList = pcList;

// 加载PC列表
async function loadPCs() {
    const container = document.getElementById('pcCardsContainer');
    container.innerHTML = '<div class="loading">加载中...</div>';
    
    try {
        const result = await API.getClients();
        if (result.success) {
            pcList = result.clients || [];
            window.pcList = pcList; // 更新全局pcList
            renderPCCards();
        }
    } catch (error) {
        container.innerHTML = `<div class="error-message show">加载失败: ${error.message}</div>`;
    }
}

// 刷新PC列表
document.getElementById('refreshClientsBtn').addEventListener('click', async () => {
    const btn = document.getElementById('refreshClientsBtn');
    const originalText = btn.innerHTML;
    btn.disabled = true;
    btn.innerHTML = '<span>⏳</span> 刷新中...';
    
    try {
        const result = await API.refreshClients();
        if (result.success) {
            pcList = result.clients || [];
            window.pcList = pcList; // 更新全局pcList
            renderPCCards();
            alert(`刷新成功！已获取 ${pcList.length} 台PC`);
        }
    } catch (error) {
        alert(`刷新失败: ${error.message}`);
    } finally {
        btn.disabled = false;
        btn.innerHTML = originalText;
    }
});

// 渲染PC卡片
async function renderPCCards() {
    const container = document.getElementById('pcCardsContainer');
    
    if (pcList.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <div class="empty-state-icon">🖥️</div>
                <div class="empty-state-text">暂无PC，请点击刷新按钮获取PC列表</div>
            </div>
        `;
        return;
    }
    
    container.innerHTML = '';
    
    for (const pc of pcList) {
        const card = createPCCard(pc);
        container.appendChild(card);
    }
}

// 创建PC卡片
function createPCCard(pc) {
    const card = document.createElement('div');
    card.className = 'pc-card';
    
    // 获取策略和白名单信息（异步加载）
    loadPCInfo(pc.username).then(info => {
        // 格式化策略显示
        let policyHtml = '<div class="policy-details">';
        if (info.policyDetails) {
            if (info.policyDetails.extensions && info.policyDetails.extensions.length > 0) {
                policyHtml += '<div class="policy-section"><strong>受保护扩展名:</strong><ul>';
                info.policyDetails.extensions.forEach(ext => {
                    policyHtml += `<li>${ext || '(空)'}</li>`;
                });
                policyHtml += '</ul></div>';
            } else {
                policyHtml += '<div class="policy-section"><strong>受保护扩展名:</strong> 无</div>';
            }
        } else {
            policyHtml += '<div class="policy-section">未配置</div>';
        }
        policyHtml += '</div>';
        
        // 格式化白名单显示
        let whitelistHtml = '<div class="whitelist-details">';
        if (info.whitelistDetails && info.whitelistDetails.length > 0) {
            whitelistHtml += '<ul>';
            info.whitelistDetails.forEach(item => {
                const defaultTag = item.isDefault ? ' <span style="color: #999; font-size: 11px;">(默认)</span>' : '';
                whitelistHtml += `<li>${item.processPath || '(空)'}${defaultTag}</li>`;
            });
            whitelistHtml += '</ul>';
        } else {
            whitelistHtml += '<div>无白名单项</div>';
        }
        whitelistHtml += '</div>';
        
        card.innerHTML = `
            <div class="pc-card-header">
                <div class="pc-card-title">${pc.pcName || pc.username}</div>
                <span class="pc-status ${pc.status === 'online' ? 'online' : 'offline'}">
                    ${pc.status === 'online' ? '在线' : '离线'}
                </span>
            </div>
            <div class="pc-card-info">
                <div class="pc-info-item">
                    <span class="pc-info-label">使用人:</span>
                    <span class="pc-info-value">${pc.username}</span>
                </div>
                <div class="pc-info-item">
                    <span class="pc-info-label">机器码:</span>
                    <span class="pc-info-value">${pc.machineId || '-'}</span>
                </div>
                <div class="pc-info-item">
                    <span class="pc-info-label">最后心跳:</span>
                    <span class="pc-info-value">${formatTimestamp(pc.lastHeartbeat)}</span>
                </div>
                <div class="pc-info-item pc-info-item-full">
                    <span class="pc-info-label">策略:</span>
                    <div class="pc-info-value">${policyHtml}</div>
                </div>
                <div class="pc-info-item pc-info-item-full">
                    <span class="pc-info-label">白名单:</span>
                    <div class="pc-info-value">${whitelistHtml}</div>
                </div>
            </div>
            <div class="pc-card-actions">
                <button class="btn btn-primary" onclick="downloadLogs('${pc.username}')">
                    📥 下载日志
                </button>
            </div>
        `;
    });
    
    return card;
}

// 加载PC详细信息（策略和白名单）
async function loadPCInfo(username) {
    try {
        const [policyResult, whitelistResult] = await Promise.all([
            API.getClientPolicy(username).catch(() => ({ success: false })),
            API.getClientWhitelist(username).catch(() => ({ success: false }))
        ]);
        
        // 解析策略详细信息
        let policyDetails = null;
        if (policyResult.success && policyResult.policy) {
            const p = policyResult.policy;
            const extensions = p.protectedExtensions 
                ? p.protectedExtensions.split(';').filter(e => e.trim()).map(e => e.trim())
                : [];
            
            policyDetails = {
                extensions: extensions,
                enableEncryption: p.enableEncryption || false,
                policyVersion: p.policyVersion || '1.0.0'
            };
        }
        
        // 解析白名单详细信息
        let whitelistDetails = [];
        if (whitelistResult.success && whitelistResult.whitelist) {
            whitelistDetails = whitelistResult.whitelist.map(item => ({
                processPath: item.processPath || '',
                isActive: item.isActive !== false,
                addedBy: item.addedBy || '',
                addedAt: item.addedAt || ''
            }));
        }
        
        return {
            policyDetails: policyDetails,
            whitelistDetails: whitelistDetails
        };
    } catch (error) {
        console.error('加载PC信息失败:', error);
        return {
            policyDetails: null,
            whitelistDetails: []
        };
    }
}

// 下载日志
async function downloadLogs(username) {
    try {
        // 先同步日志
        await API.syncClientLogs(username);
        // 等待一下让服务器处理
        setTimeout(async () => {
            try {
                await API.downloadClientLogs(username);
            } catch (error) {
                alert(`下载日志失败: ${error.message}`);
            }
        }, 1000);
    } catch (error) {
        alert(`同步日志失败: ${error.message}`);
    }
}

// 导出函数供全局使用
window.downloadLogs = downloadLogs;
window.loadPCs = loadPCs;
