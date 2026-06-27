# 安全文件传输系统 - 服务端

## 项目简介

这是安全文件传输系统的服务端实现，负责接收客户端发送的文件，管理PC在线状态，并将文件转发给目标PC。

## 功能特性

- ✅ 文件传输管理（接收、存储、转发）
- ✅ PC在线状态管理
- ✅ WebSocket实时通信
- ✅ 断点续传支持
- ✅ 传输任务管理（取消、继续、删除）
- ✅ 已完成文件列表查询
- ✅ SQLite数据库存储

## 技术栈

- **Node.js** - 运行环境
- **Express** - Web框架
- **Socket.io** - WebSocket通信
- **SQLite3** - 数据库
- **UUID** - 唯一ID生成

## 安装依赖

```bash
npm install
```

## 运行服务器

### 开发模式（自动重启）

```bash
npm run dev
```

### 生产模式

```bash
npm start
```

服务器默认运行在 `http://localhost:8080`

## 项目结构

```
SecureTransferServer/
├── src/
│   ├── app.js                 # 应用入口
│   ├── database/
│   │   └── db.js             # 数据库连接和初始化
│   ├── services/
│   │   ├── FileStorageService.js    # 文件存储服务
│   │   ├── PCManagerService.js      # PC管理服务
│   │   └── FileTransferService.js   # 文件传输服务
│   ├── routes/
│   │   ├── file.js           # 文件传输路由
│   │   ├── files.js          # 文件列表路由
│   │   └── pc.js             # PC管理路由
│   └── websocket/
│       └── socketHandler.js  # WebSocket处理器
├── uploads/                   # 临时上传目录
├── completed/                 # 已完成文件目录
├── database/                  # 数据库文件目录
├── package.json
└── README.md
```

## API接口

### 文件传输

**POST /api/file/transfer**

接收文件传输请求，保存文件并转发给目标PC。

**请求体：**
```json
{
    "magic": 1280268881,
    "version": 1,
    "targetId": "李明028",
    "fileName": "document.pdf",
    "fileSize": 1024000,
    "fileData": "base64编码的文件数据"
}
```

### 取消传输

**POST /api/file/cancel**

取消正在进行的文件传输。

**请求体：**
```json
{
    "magic": 1280269614,
    "version": 1,
    "targetId": "李明028",
    "fileName": "document.pdf",
    "cancelTimestamp": "202501011430",
    "interruptOffset": 512000
}
```

### 继续传输

**POST /api/file/resume**

继续之前中断的文件传输。

**请求体：**
```json
{
    "magic": 1280269293,
    "version": 1,
    "targetId": "李明028",
    "fileName": "document.pdf",
    "resumeTimestamp": "202501011435",
    "resumeOffset": 512000
}
```

### 删除任务

**POST /api/file/delete**

删除传输任务。

**请求体：**
```json
{
    "magic": 1280266341,
    "version": 1,
    "targetId": "李明028",
    "fileName": "document.pdf",
    "deleteTimestamp": "202501011440",
    "deleteOffset": 1024000
}
```

### 获取已完成文件列表

**GET /api/files/completed?targetId=李明028**

获取已完成传输的文件列表。

**响应：**
```json
{
    "success": true,
    "files": [
        {
            "fileId": "file_12345",
            "fileName": "document.pdf",
            "senderId": "李明028",
            "fileSize": 1024000,
            "status": "completed",
            "completedTime": "2025-01-01T14:30:00.000Z"
        }
    ]
}
```

### PC管理

**POST /api/pc/register** - 注册PC

**POST /api/pc/heartbeat** - 更新心跳

**GET /api/pc/online** - 获取在线PC列表

## WebSocket事件

### 客户端发送

- `register` - 注册PC
- `heartbeat` - 发送心跳
- `fileReceived` - 文件接收确认

### 服务器发送

- `fileTransfer` - 文件传输通知
- `transferCancelled` - 传输取消通知
- `transferDeleted` - 传输删除通知
- `registered` - 注册成功确认

## 数据库结构

### pc_list
存储PC信息和在线状态

### file_transfers
存储文件传输任务

### completed_files
存储已完成传输的文件信息

## 配置

默认端口：`8080`

可以通过环境变量 `PORT` 修改端口：

```bash
PORT=3000 npm start
```

## 注意事项

1. 确保 `uploads/` 和 `completed/` 目录有写入权限
2. 大文件传输时注意内存使用
3. 定期清理过期文件（可添加定时任务）
4. 生产环境建议使用更强大的数据库（PostgreSQL/MySQL）

## 开发计划

- [ ] 添加文件下载接口
- [ ] 添加文件过期清理机制
- [ ] 添加传输速度限制
- [ ] 添加日志记录
- [ ] 添加身份认证
- [ ] 支持文件分块传输（大文件优化）
