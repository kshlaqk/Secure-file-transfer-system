# 构建说明

## 环境要求

- **Qt版本**: Qt 6.x (需要 Core, Widgets, Network 模块)
- **CMake版本**: 3.16 或更高
- **编译器**: 
  - Windows: MSVC 2019+ 或 MinGW
  - 需要支持 C++17
- **Windows SDK**: 需要安装 Windows SDK（用于 Minifilter 通信）

## 构建步骤

### 方法1: 使用CMake命令行

```bash
# 1. 创建构建目录
mkdir build
cd build

# 2. 配置CMake（需要指定Qt路径）
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64"  # 根据实际Qt路径修改

# 3. 构建
cmake --build . --config Release

# 4. 运行（构建完成后）
./Release/SecureTransferClient.exe  # 或 Debug/SecureTransferClient.exe
```

### 方法2: 使用Qt Creator

1. 打开Qt Creator
2. 文件 -> 打开文件或项目
3. 选择 `CMakeLists.txt`
4. 配置Kit（选择Qt版本和编译器）
5. 点击"构建"按钮

### 方法3: 使用Visual Studio

```bash
# 1. 创建构建目录
mkdir build
cd build

# 2. 生成Visual Studio项目文件
cmake .. -G "Visual Studio 17 2022" -A x64

# 3. 打开生成的 .sln 文件
# 4. 在Visual Studio中构建和运行
```

## 配置说明

### 首次运行

1. 启动程序后，点击"设置"菜单
2. 输入Web服务URL（例如：`http://localhost:8080`）
3. 程序会自动启动策略同步服务

### 策略同步配置

- 默认轮询间隔：5秒
- API端点：`/api/policy/current`
- 可以通过代码修改轮询间隔和API端点

## 故障排除

### 1. 找不到Qt

**错误**: `Could not find a package configuration file provided by "Qt6"`

**解决**: 使用 `-DCMAKE_PREFIX_PATH` 指定Qt安装路径

### 2. 找不到fltuser.lib

**错误**: `Cannot find fltuser.lib`

**解决**: 
- 确保已安装Windows SDK
- 确保系统PATH中包含Windows SDK的库路径
- 或者在CMakeLists.txt中手动指定路径

### 3. 驱动通信失败

**错误**: `Failed to connect to driver port`

**解决**:
- 确保驱动已加载（使用 `sc query FltDriver` 检查）
- 确保以管理员权限运行客户端
- 检查驱动是否正常运行

### 4. 策略同步失败

**错误**: `无法连接到Web服务`

**解决**:
- 检查Web服务URL是否正确
- 检查网络连接
- 检查Web服务是否运行
- 检查防火墙设置

## 开发提示

1. **驱动通信**: 仅在Windows平台可用，编译其他平台时需要条件编译
2. **字符串转换**: QString到wchar_t的转换在Windows上可以直接使用utf16()
3. **线程安全**: 策略同步线程使用QMutex保证线程安全
4. **错误处理**: 所有网络和驱动通信都有错误处理机制

## 项目依赖

### Qt模块
- Qt6::Core
- Qt6::Widgets  
- Qt6::Network

### Windows库
- fltuser.lib (Minifilter用户态库)

## 下一步开发

1. **文件传输实现**: 当前FileTransferManager只是框架，需要实现实际的网络传输逻辑
2. **远程文件浏览**: 需要实现从服务器获取文件列表的功能
3. **传输进度**: 需要实现真实的传输进度计算
4. **错误恢复**: 可以添加传输失败重试机制
5. **配置文件**: 可以添加配置文件保存设置




