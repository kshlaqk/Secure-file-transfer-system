# 安全文件传输客户端

基于Qt的文件传输客户端，用于安全文件传输系统的客户端应用程序。

## 功能特性

- 文件传输功能（上传/下载）
- 实时传输进度显示
- 与Minifilter驱动通信
- 策略自动同步（从Web服务端）
- 多任务传输管理

## 构建要求

- Qt 6.x
- CMake 3.16+
- Windows SDK（用于Minifilter通信）
- Visual Studio 2019+ 或 MinGW

## 构建步骤

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 配置说明

客户端通过Web服务获取策略配置，需要在设置中配置Web服务URL。

## 目录结构

```
SecureTransferClient/
├── src/          # 源代码
├── ui/           # UI文件（.ui）
├── resources/    # 资源文件
└── CMakeLists.txt
```




