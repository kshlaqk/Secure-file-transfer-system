// 主应用逻辑
let currentPage = 'pcManagementPage';
let socket = null;

// 初始化应用
function initApp() {
    setupNavigation();
    setupSocketConnection();
    loadPCs();
}

// 设置Socket.io连接
function setupSocketConnection() {
    socket = io();
    
    socket.on('connect', () => {
        console.log('已连接到Web管理后台服务器');
    });
    
    socket.on('disconnect', () => {
        console.log('已断开与Web管理后台服务器的连接');
    });
    
    // 监听客户端离线事件
    socket.on('clientOffline', (data) => {
        console.log('客户端离线:', data);
        // 刷新PC列表以更新状态
        if (typeof loadPCs === 'function') {
            loadPCs();
        }
    });
    
    // 监听客户端上线事件
    socket.on('clientOnline', (data) => {
        console.log('客户端上线:', data);
        // 刷新PC列表以更新状态
        if (typeof loadPCs === 'function') {
            loadPCs();
        }
    });
}

// 设置导航
function setupNavigation() {
    const navBtns = document.querySelectorAll('.nav-btn');
    navBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const targetPage = btn.getAttribute('data-page');
            
            // 移除所有活动状态
            navBtns.forEach(nav => nav.classList.remove('active'));
            document.querySelectorAll('.page').forEach(page => page.classList.remove('active'));
            
            // 设置当前活动项
            btn.classList.add('active');
            const page = document.getElementById(targetPage);
            if (page) {
                page.classList.add('active');
                currentPage = targetPage;
                
                // 页面切换时的初始化逻辑
                if (targetPage === 'policyManagementPage' && typeof loadPCsForPolicy === 'function') {
                    loadPCsForPolicy();
                } else if (targetPage === 'whitelistManagementPage' && typeof loadPCsForWhitelist === 'function') {
                    loadPCsForWhitelist();
                } else if (targetPage === 'pcManagementPage' && typeof loadPCs === 'function') {
                    loadPCs();
                }
            }
        });
    });
}

// 工具函数：关闭模态框
function closeModal(modalId) {
    document.getElementById(modalId).classList.remove('show');
}

// 工具函数：显示模态框
function showModal(modalId) {
    document.getElementById(modalId).classList.add('show');
}

// 工具函数：格式化时间
function formatTimestamp(timestamp) {
    if (!timestamp) return '-';
    const date = new Date(timestamp);
    return date.toLocaleString('zh-CN');
}

// 导出给其他模块使用
window.closeModal = closeModal;
window.showModal = showModal;
window.formatTimestamp = formatTimestamp;
