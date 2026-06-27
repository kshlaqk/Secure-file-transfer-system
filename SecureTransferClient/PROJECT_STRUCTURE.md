# 安全文件传输客户端项目结构

## 项目概述

基于Qt的文件传输客户端应用程序，支持与Minifilter驱动通信，实现策略自动同步功能。

## 目录结构

```
SecureTransferClient/
├── src/                          # 源代码目录
│   ├── main.cpp                  # 程序入口
│   ├── MainWindow.h/cpp         # 主窗口类
│   ├── DriverTypes.h            # 驱动通信数据类型定义
│   ├── DriverCommunicator.h/cpp # 驱动通信封装类
│   ├── FileTransferManager.h/cpp # 文件传输管理类
│   ├── PolicySyncService.h/cpp  # 策略同步服务类
│   ├── PolicySyncThread.h/cpp   # 策略同步线程类
│   └── WebServiceClient.h/cpp   # Web服务客户端类
├── ui/                           # UI文件目录（.ui文件）
├── resources/                    # 资源文件目录
├── CMakeLists.txt               # CMake构建文件
├── README.md                    # 项目说明
└── .gitignore                   # Git忽略文件

```

## 核心模块说明

### 1. MainWindow (主窗口)
- **功能**: 提供文件传输的用户界面
- **特点**: 
  - 左右分栏显示本地和远程文件
  - 传输任务列表显示
  - 文件操作（上传/下载/删除）
  - 设置对话框（配置Web服务URL）

### 2. DriverCommunicator (驱动通信)
- **功能**: 与Minifilter驱动通信
- **接口**: 
  - `connectToDriver()` - 连接驱动
  - `updatePolicy()` - 更新策略配置
- **依赖**: Windows SDK (fltuser.h)

### 3. PolicySyncService (策略同步服务)
- **功能**: 管理策略同步流程
- **工作流程**:
  1. 启动策略同步线程
  2. 从Web服务获取策略更新
  3. 应用策略到驱动
  4. 发送更新通知

### 4. PolicySyncThread (策略同步线程)
- **功能**: 后台线程，定期轮询Web服务
- **配置**:
  - Web服务URL
  - 轮询间隔
  - API端点
  - 认证令牌

### 5. WebServiceClient (Web服务客户端)
- **功能**: HTTP客户端，与Web后台通信
- **方法**:
  - `getRequest()` - GET请求
  - `postRequest()` - POST请求

### 6. FileTransferManager (文件传输管理)
- **功能**: 管理文件传输任务
- **状态**: 基础框架（待实现具体传输逻辑）

## 构建要求

- Qt 6.x (Core, Widgets, Network)
- CMake 3.16+
- Windows SDK (用于Minifilter通信)
- C++17 编译器

## 使用方法

1. 配置Web服务URL（通过设置菜单）
2. 启动策略同步服务（自动启动）
3. 使用文件传输功能

## Web API接口规范

客户端期望的Web服务API接口：

**GET /api/policy/current**

响应格式（JSON）:
```json
{
    "version": "1.0.3",
    "timestamp": "2025-01-15T10:30:00Z",
    "protectedExtensions": ".docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.exe",
    "enableEncryption": false
}
```

## 注意事项

1. 驱动通信仅在Windows平台支持
2. 需要管理员权限运行（用于驱动通信）
3. 策略同步线程默认每5秒轮询一次
4. Web服务URL需要可访问




