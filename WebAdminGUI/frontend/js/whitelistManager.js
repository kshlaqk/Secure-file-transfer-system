// 白名单管理
let currentWhitelistUsername = null;

// 初始化白名单管理页面
document.addEventListener('DOMContentLoaded', () => {
    // 添加白名单项
    const addBtn = document.getElementById('addWhitelistBtn');
    if (addBtn) {
        addBtn.addEventListener('click', async () => {
            await addWhitelistItem();
        });
    }

    // 删除白名单项按钮
    const removeBtn = document.getElementById('removeWhitelistBtn');
    if (removeBtn) {
        removeBtn.addEventListener('click', async () => {
            await removeWhitelistItemFromInput();
        });
    }

    // 回车键删除
    const removeInput = document.getElementById('removeWhitelistPath');
    if (removeInput) {
        removeInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                removeWhitelistItemFromInput();
            }
        });
    }
});

// 验证PC是否存在
function validateWhitelistPCExists(username) {
    // 从pcManager.js中获取pcList
    if (typeof window.pcList === 'undefined' || !Array.isArray(window.pcList)) {
        // 如果pcList不可用，尝试通过API获取
        return null; // 返回null表示需要异步验证
    }
    
    return window.pcList.some(pc => pc.username === username);
}

// 异步验证PC是否存在
async function validateWhitelistPCExistsAsync(username) {
    try {
        const result = await API.getClients();
        if (result.success && result.clients) {
            return result.clients.some(pc => pc.username === username);
        }
    } catch (error) {
        console.error('验证PC存在性失败:', error);
    }
    return false;
}

// 注意：白名单列表不再在此页面显示，只通过PC卡片查看

// 添加白名单项
async function addWhitelistItem() {
    const pcInput = document.getElementById('whitelistPcInput');
    const username = pcInput.value.trim();
    
    if (!username) {
        showError('whitelistError', '请输入PC使用人名称');
        return;
    }
    
    // 验证PC是否存在
    let pcExists = validateWhitelistPCExists(username);
    if (pcExists === null) {
        // 需要异步验证
        pcExists = await validateWhitelistPCExistsAsync(username);
    }
    
    if (!pcExists) {
        showError('whitelistError', `PC "${username}" 不存在，请检查PC名称是否正确`);
        return;
    }
    
    const input = document.getElementById('newWhitelistPath');
    if (!input) return;
    
    const processPath = input.value.trim();
    if (!processPath) {
        showError('whitelistError', '请输入进程路径');
        return;
    }
    
    clearMessages('whitelistError', 'whitelistSuccess');
    
    try {
        const result = await API.addWhitelistItem(username, processPath);
        if (result.success) {
            input.value = '';
            showSuccess('whitelistSuccess', '白名单项已添加');
            // 刷新PC列表以更新PC卡片
            if (typeof loadPCs === 'function') {
                await loadPCs();
            }
        }
    } catch (error) {
        showError('whitelistError', '添加失败: ' + error.message);
    }
}

// 从输入框删除白名单项
async function removeWhitelistItemFromInput() {
    const pcInput = document.getElementById('whitelistPcInput');
    const username = pcInput.value.trim();
    
    if (!username) {
        showError('whitelistError', '请输入PC使用人名称');
        return;
    }
    
    // 验证PC是否存在
    let pcExists = validateWhitelistPCExists(username);
    if (pcExists === null) {
        // 需要异步验证
        pcExists = await validateWhitelistPCExistsAsync(username);
    }
    
    if (!pcExists) {
        showError('whitelistError', `PC "${username}" 不存在，请检查PC名称是否正确`);
        return;
    }
    
    const removeInput = document.getElementById('removeWhitelistPath');
    if (!removeInput) return;
    
    const processPath = removeInput.value.trim();
    if (!processPath) {
        showError('whitelistError', '请输入要删除的进程路径');
        return;
    }
    
    if (!confirm('确定要删除这个白名单项吗？\n' + processPath)) {
        return;
    }
    
    clearMessages('whitelistError', 'whitelistSuccess');
    
    try {
        const result = await API.removeWhitelistItem(username, processPath);
        if (result.success) {
            removeInput.value = '';
            showSuccess('whitelistSuccess', '白名单项已删除');
            // 刷新PC列表以更新PC卡片
            if (typeof loadPCs === 'function') {
                await loadPCs();
            }
        }
    } catch (error) {
        showError('whitelistError', '删除失败: ' + error.message);
    }
}

// 删除白名单项（保留旧接口以兼容）
async function removeWhitelistItem(username, processPath) {
    const pcInput = document.getElementById('whitelistPcInput');
    const inputUsername = pcInput ? pcInput.value.trim() : '';
    
    // 如果传入了username参数，使用参数；否则使用输入框的值
    const targetUsername = username || inputUsername;
    
    if (!targetUsername) {
        showError('whitelistError', '请输入PC使用人名称');
        return;
    }
    
    // 验证PC是否存在
    let pcExists = validateWhitelistPCExists(targetUsername);
    if (pcExists === null) {
        // 需要异步验证
        pcExists = await validateWhitelistPCExistsAsync(targetUsername);
    }
    
    if (!pcExists) {
        showError('whitelistError', `PC "${targetUsername}" 不存在，请检查PC名称是否正确`);
        return;
    }
    
    if (!confirm('确定要删除这个白名单项吗？\n' + processPath)) {
        return;
    }
    
    clearMessages('whitelistError', 'whitelistSuccess');
    
    try {
        const result = await API.removeWhitelistItem(targetUsername, processPath);
        if (result.success) {
            showSuccess('whitelistSuccess', '白名单项已删除');
            // 刷新PC列表以更新PC卡片
            if (typeof loadPCs === 'function') {
                await loadPCs();
            }
        }
    } catch (error) {
        showError('whitelistError', '删除失败: ' + error.message);
    }
}

// 显示错误
function showError(elementId, message) {
    const errorDiv = document.getElementById(elementId);
    if (errorDiv) {
        errorDiv.textContent = message;
        errorDiv.classList.add('show');
    }
}

// 显示成功消息
function showSuccess(elementId, message) {
    const successDiv = document.getElementById(elementId);
    if (successDiv) {
        successDiv.textContent = message;
        successDiv.classList.add('show');
    }
}

// 清除消息
function clearMessages(errorId, successId) {
    const errorDiv = document.getElementById(errorId);
    const successDiv = document.getElementById(successId);
    if (errorDiv) {
        errorDiv.textContent = '';
        errorDiv.classList.remove('show');
    }
    if (successDiv) {
        successDiv.textContent = '';
        successDiv.classList.remove('show');
    }
}

// 格式化时间戳
function formatTimestamp(timestamp) {
    if (!timestamp) return '-';
    try {
        const date = new Date(timestamp);
        return date.toLocaleString('zh-CN');
    } catch {
        return timestamp;
    }
}

// 导出函数供全局使用
window.removeWhitelistItem = removeWhitelistItem;
window.validateWhitelistPCExists = validateWhitelistPCExists;
window.validateWhitelistPCExistsAsync = validateWhitelistPCExistsAsync;
