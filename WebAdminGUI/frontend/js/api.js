// API 基础配置
const API_BASE_URL = window.location.origin;
let currentSessionId = localStorage.getItem('adminSessionId') || '';

// API 请求封装
async function apiRequest(endpoint, options = {}) {
    const url = `${API_BASE_URL}${endpoint}`;
    const config = {
        ...options,
        headers: {
            'Content-Type': 'application/json',
            'X-Session-Id': currentSessionId,
            ...options.headers
        }
    };
    
    if (options.body && typeof options.body === 'object') {
        config.body = JSON.stringify(options.body);
    }
    
    try {
        console.log('发送API请求:', url, config);
        const response = await fetch(url, config);
        
        // 检查响应类型
        const contentType = response.headers.get('content-type');
        let data;
        
        if (contentType && contentType.includes('application/json')) {
            data = await response.json();
        } else {
            const text = await response.text();
            console.error('非JSON响应:', text);
            throw new Error('服务器返回了非JSON格式的响应');
        }
        
        console.log('API响应:', data);
        
        if (!response.ok) {
            throw new Error(data.error || `请求失败: ${response.status}`);
        }
        
        return data;
    } catch (error) {
        console.error('API请求错误:', error);
        console.error('错误详情:', {
            url,
            message: error.message,
            stack: error.stack
        });
        throw error;
    }
}

// API 方法
const API = {
    // 管理员认证
    login: (username, password) => {
        return apiRequest('/api/admin/login', {
            method: 'POST',
            body: { username, password }
        });
    },
    
    createAdmin: (username, password) => {
        return apiRequest('/api/admin/create', {
            method: 'POST',
            body: { username, password }
        });
    },
    
    logout: () => {
        return apiRequest('/api/admin/logout', {
            method: 'POST'
        });
    },
    
    // 客户端管理
    getClients: () => {
        return apiRequest('/api/admin/clients');
    },
    
    // 策略管理
    getClientPolicy: (username) => {
        return apiRequest(`/api/admin/clients/${username}/policy`);
    },
    
    updateClientPolicy: (username, policy) => {
        return apiRequest(`/api/admin/clients/${username}/policy`, {
            method: 'POST',
            body: policy
        });
    },
    
    // 白名单管理
    getClientWhitelist: (username) => {
        return apiRequest(`/api/admin/clients/${username}/whitelist`);
    },
    
    addWhitelistItem: (username, processPath) => {
        return apiRequest(`/api/admin/clients/${username}/whitelist`, {
            method: 'POST',
            body: { processPath }
        });
    },
    
    removeWhitelistItem: (username, processPath) => {
        return apiRequest(`/api/admin/clients/${username}/whitelist`, {
            method: 'DELETE',
            body: { processPath }
        });
    },
    
    // 日志管理
    getClientLogs: (username, limit = 100, offset = 0) => {
        return apiRequest(`/api/admin/clients/${username}/logs?limit=${limit}&offset=${offset}`);
    },
    
    syncClientLogs: (username) => {
        return apiRequest(`/api/admin/clients/${username}/logs/sync`, {
            method: 'POST'
        });
    },
    
    downloadClientLogs: async (username) => {
        const url = `${API_BASE_URL}/api/admin/clients/${username}/logs/download`;
        try {
            const response = await fetch(url, {
                headers: {
                    'X-Session-Id': currentSessionId
                }
            });
            
            if (!response.ok) {
                const error = await response.json();
                throw new Error(error.error || '下载失败');
            }
            
            const blob = await response.blob();
            const downloadUrl = window.URL.createObjectURL(blob);
            const link = document.createElement('a');
            link.href = downloadUrl;
            link.download = `logs_${username}_${Date.now()}.txt`;
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            window.URL.revokeObjectURL(downloadUrl);
        } catch (error) {
            throw error;
        }
    },
    
    // 刷新PC列表
    refreshClients: () => {
        return apiRequest('/api/admin/refresh-clients', {
            method: 'POST'
        });
    },
    
    // 修改密码
    changePassword: (oldPassword, newPassword) => {
        return apiRequest('/api/admin/change-password', {
            method: 'POST',
            body: { oldPassword, newPassword }
        });
    }
};

// 设置会话ID
function setSessionId(sessionId) {
    currentSessionId = sessionId;
    localStorage.setItem('adminSessionId', sessionId);
}

// 清除会话ID
function clearSessionId() {
    currentSessionId = '';
    localStorage.removeItem('adminSessionId');
}
