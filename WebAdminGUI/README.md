# Web 管理后台

安全文件传输系统的 Web 管理后台，用于管理员管理客户端策略、白名单和查看日志。

## 功能特性

- 🔐 管理员登录认证
- 📋 客户端列表管理
- ⚙️ 策略配置管理（受保护扩展名、路径、加密）
- ✅ 白名单管理（添加/删除进程）
- 📊 驱动日志查看
- 🔄 实时策略推送（客户端上线后自动推送）
- 📡 WebSocket 实时通信

## 项目结构

```
WebAdminGUI/
├── backend/              # 后端 API 服务器
│   ├── app.js           # 主应用入口
│   ├── config/          # 配置文件
│   ├── database/        # 数据库迁移
│   ├── services/        # 业务逻辑服务
│   ├── routes/          # API 路由
│   └── middleware/      # 中间件
├── frontend/             # 前端 GUI
│   ├── index.html       # 主页面
│   ├── css/             # 样式文件
│   └── js/              # JavaScript 文件
└── package.json         # 项目配置
```

## 安装和运行

### 1. 安装依赖

```bash
cd WebAdminGUI
npm install
```

### 2. 配置环境变量（可选）

创建 `.env` 文件：

```
MAIN_SERVER_URL=http://localhost:8080
ADMIN_PORT=3000
```

### 3. 启动服务器

**Windows:**
```bash
start.bat
```

**Linux/Mac:**
```bash
chmod +x start.sh
./start.sh
```

或使用 npm：
```bash
npm start
```

### 4. 访问管理界面

打开浏览器访问：`http://localhost:3000`

## 默认管理员账号

首次运行需要创建管理员账号，可以通过以下方式：

### 方式1：使用 API（推荐）

```bash
# 创建管理员（需要先启动服务器）
curl -X POST http://localhost:3000/api/admin/create \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}'
```

### 方式2：直接访问管理界面

1. 启动服务器后访问 `http://localhost:3000`
2. 如果还没有管理员，系统会提示创建
3. 或者使用 API 创建管理员后再登录

## API 接口

### 管理员认证
- `POST /api/admin/login` - 管理员登录
- `POST /api/admin/create` - 创建管理员（首次使用）

### 客户端管理
- `GET /api/admin/clients` - 获取所有客户端列表
- `GET /api/admin/clients/:username/policy` - 获取客户端策略
- `POST /api/admin/clients/:username/policy` - 更新客户端策略
- `GET /api/admin/clients/:username/whitelist` - 获取客户端白名单
- `POST /api/admin/clients/:username/whitelist` - 添加白名单项
- `DELETE /api/admin/clients/:username/whitelist` - 删除白名单项
- `GET /api/admin/clients/:username/logs` - 获取客户端日志
- `POST /api/admin/clients/:username/logs/sync` - 请求客户端同步日志

## 技术栈

- **后端**: Node.js + Express
- **前端**: HTML + CSS + JavaScript (原生)
- **数据库**: SQLite3（共享 SecureTransferServer 的数据库）
- **实时通信**: Socket.io Client（连接主服务器）

## 注意事项

1. 确保主服务器（SecureTransferServer）已启动
2. 管理后台需要连接到主服务器的 WebSocket（默认端口 8080）
3. 数据库文件位于 `../SecureTransferServer/database/transfers.db`
4. 策略更新会立即推送给在线的客户端，离线客户端会在上线后自动同步
