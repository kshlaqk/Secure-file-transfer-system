// 策略管理
let currentPolicyUsername = null;

// 初始化策略管理页面
document.addEventListener('DOMContentLoaded', () => {
    // 保存策略
    const saveBtn = document.getElementById('policySaveBtn');
    if (saveBtn) {
        saveBtn.addEventListener('click', async () => {
            await savePolicy();
        });
    }

    // 取消按钮
    const cancelBtn = document.getElementById('policyCancelBtn');
    if (cancelBtn) {
        cancelBtn.addEventListener('click', () => {
            document.getElementById('policyPcInput').value = '';
            document.getElementById('policyExtensions').value = '';
            document.getElementById('policyEncryption').checked = false;
            currentPolicyUsername = null;
            clearMessages('policyError', 'policySuccess');
        });
    }
});

// 验证PC是否存在
function validatePCExists(username) {
    // 从pcManager.js中获取pcList
    if (typeof window.pcList === 'undefined' || !Array.isArray(window.pcList)) {
        // 如果pcList不可用，尝试通过API获取
        return null; // 返回null表示需要异步验证
    }
    
    return window.pcList.some(pc => pc.username === username);
}

// 异步验证PC是否存在
async function validatePCExistsAsync(username) {
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

// 保存策略
async function savePolicy() {
    const pcInput = document.getElementById('policyPcInput');
    const username = pcInput.value.trim();
    
    if (!username) {
        showError('policyError', '请输入PC使用人名称');
        return;
    }
    
    // 验证PC是否存在
    let pcExists = validatePCExists(username);
    if (pcExists === null) {
        // 需要异步验证
        pcExists = await validatePCExistsAsync(username);
    }
    
    if (!pcExists) {
        showError('policyError', `PC "${username}" 不存在，请检查PC名称是否正确`);
        return;
    }
    
    const protectedExtensions = document.getElementById('policyExtensions').value.trim();
    const enableEncryption = document.getElementById('policyEncryption').checked;
    
    clearMessages('policyError', 'policySuccess');
    
    try {
        const result = await API.updateClientPolicy(username, {
            protectedExtensions,
            enableEncryption
        });
        
        if (result.success) {
            showSuccess('policySuccess', '策略已保存！' + (result.message || ''));
            // 刷新PC列表以更新PC卡片
            if (typeof loadPCs === 'function') {
                await loadPCs();
            }
            // 清空表单
            document.getElementById('policyExtensions').value = '';
            document.getElementById('policyEncryption').checked = false;
            currentPolicyUsername = null;
        }
    } catch (error) {
        showError('policyError', '保存策略失败: ' + error.message);
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

// 导出函数供全局使用
window.validatePCExists = validatePCExists;
window.validatePCExistsAsync = validatePCExistsAsync;
