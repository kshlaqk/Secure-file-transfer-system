// 认证管理
let currentAdmin = null;

// 检查登录状态
function checkAuth() {
    const sessionId = localStorage.getItem('adminSessionId');
    if (sessionId) {
        // 可以在这里验证会话是否有效
        return true;
    }
    return false;
}

// 显示登录界面
function showLogin() {
    document.getElementById('loginPage').style.display = 'flex';
    document.getElementById('mainApp').style.display = 'none';
}

// 隐藏登录界面
function hideLogin() {
    document.getElementById('loginPage').style.display = 'none';
    document.getElementById('mainApp').style.display = 'flex';
}

// 登录处理
function setupLoginForm() {
    const loginForm = document.getElementById('loginForm');
    if (!loginForm) {
        console.error('登录表单未找到');
        return;
    }
    
    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const username = document.getElementById('loginUsername').value;
        const password = document.getElementById('loginPassword').value;
        const errorDiv = document.getElementById('loginError');
        
        errorDiv.textContent = '';
        errorDiv.classList.remove('show');
        
        console.log('开始登录，用户名:', username);
        
        try {
            const result = await API.login(username, password);
            console.log('登录响应:', result);
            
            if (result && result.success) {
                if (typeof setSessionId === 'function') {
                    setSessionId(result.sessionId);
                } else {
                    console.error('setSessionId函数未定义');
                }
                currentAdmin = result.admin;
                
                const adminNameEl = document.getElementById('adminName');
                if (adminNameEl) {
                    adminNameEl.textContent = `管理员: ${currentAdmin.username}`;
                }
                
                hideLogin();
                
                // 初始化应用
                if (typeof initApp === 'function') {
                    initApp();
                } else {
                    console.warn('initApp函数未定义');
                }
            } else {
                const errorMsg = (result && result.error) || '登录失败';
                errorDiv.textContent = errorMsg;
                errorDiv.classList.add('show');
            }
        } catch (error) {
            console.error('登录错误:', error);
            errorDiv.textContent = error.message || '登录失败';
            errorDiv.classList.add('show');
        }
    });
}

// 登出处理
function setupLogout() {
    const logoutBtn = document.getElementById('logoutBtn');
    if (logoutBtn) {
        logoutBtn.addEventListener('click', async () => {
            try {
                await API.logout();
            } catch (error) {
                console.error('登出失败:', error);
            } finally {
                clearSessionId();
                currentAdmin = null;
                showLogin();
            }
        });
    }
}

// 修改密码处理
function setupChangePassword() {
    const changePasswordBtn = document.getElementById('changePasswordBtn');
    if (changePasswordBtn) {
        changePasswordBtn.addEventListener('click', () => {
            showModal('changePasswordModal');
        });
    }
    
    const changePasswordForm = document.getElementById('changePasswordForm');
    if (changePasswordForm) {
        changePasswordForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const oldPassword = document.getElementById('oldPassword').value;
            const newPassword = document.getElementById('newPassword').value;
            const confirmPassword = document.getElementById('confirmPassword').value;
            const errorDiv = document.getElementById('changePasswordError');
            
            errorDiv.textContent = '';
            errorDiv.classList.remove('show');
            
            if (newPassword !== confirmPassword) {
                errorDiv.textContent = '新密码和确认密码不一致';
                errorDiv.classList.add('show');
                return;
            }
            
            try {
                const result = await API.changePassword(oldPassword, newPassword);
                if (result.success) {
                    alert('密码修改成功');
                    closeModal('changePasswordModal');
                    document.getElementById('changePasswordForm').reset();
                }
            } catch (error) {
                errorDiv.textContent = error.message || '修改密码失败';
                errorDiv.classList.add('show');
            }
        });
    }
}

// 页面加载完成后初始化
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initAuth);
} else {
    // DOM已经加载完成
    initAuth();
}

function initAuth() {
    console.log('初始化登录功能');
    console.log('API对象:', typeof API);
    console.log('setSessionId函数:', typeof setSessionId);
    
    setupLoginForm();
    setupLogout();
    setupChangePassword();
    
    // 检查登录状态
    if (checkAuth()) {
        console.log('检测到已登录，隐藏登录页面');
        hideLogin();
    } else {
        console.log('未登录，显示登录页面');
        showLogin();
    }
}
