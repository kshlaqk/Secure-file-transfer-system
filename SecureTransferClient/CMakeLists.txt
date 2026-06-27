cmake_minimum_required(VERSION 3.16)
project(SecureTransferClient VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebSockets)

qt6_standard_project_setup()

# Windows SDK 依赖（用于 fltuser.h 和 Minifilter 通信）
if(WIN32)
    # 确定架构后缀
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH_SUFFIX "x64")
    else()
        set(ARCH_SUFFIX "x86")
    endif()
    
    # 查找 fltlib.lib（用户模式 Minifilter 库）
    file(GLOB FLTLIB_FILES 
        "C:/Program Files (x86)/Windows Kits/10/Lib/10.*/um/${ARCH_SUFFIX}/fltlib.lib"
        "C:/Program Files/Windows Kits/10/Lib/10.*/um/${ARCH_SUFFIX}/fltlib.lib"
    )
    
    if(FLTLIB_FILES)
        # 选择最新的 SDK 版本
        list(SORT FLTLIB_FILES)
        list(REVERSE FLTLIB_FILES)
        list(GET FLTLIB_FILES 0 FLTUSER_LIB)
        message(STATUS "Found fltlib.lib: ${FLTUSER_LIB}")
    else()
        # 如果找不到，使用库名
        set(FLTUSER_LIB "fltlib")
        message(WARNING "fltlib.lib not found, will try to link by name")
    endif()
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/LoginDialog.cpp
    src/MachineIdHelper.cpp
    src/DriverCommunicator.cpp
    src/FileTransferManager.cpp
    src/PolicySyncService.cpp
    src/PolicySyncThread.cpp
    src/WebServiceClient.cpp
    src/SocketIOClient.cpp
)

set(HEADERS
    src/MainWindow.h
    src/LoginDialog.h
    src/MachineIdHelper.h
    src/DriverCommunicator.h
    src/DriverTypes.h
    src/FileTransferManager.h
    src/PolicySyncService.h
    src/PolicySyncThread.h
    src/WebServiceClient.h
    src/SocketIOClient.h
)

# 创建可执行文件
qt6_add_executable(SecureTransferClient
    ${SOURCES}
    ${HEADERS}
)

# 为 MSVC 添加必要的编译器选项（Qt 6 需要）
if(MSVC)
    target_compile_options(SecureTransferClient PRIVATE 
        /Zc:__cplusplus    # 正确报告 __cplusplus 宏
        /permissive-       # Qt 6 要求的标准一致性模式
        /utf-8             # UTF-8 编码支持（解决中文乱码）
    )
endif()

target_link_libraries(SecureTransferClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

if(WIN32)
    # 链接 fltlib.lib（Minifilter 用户模式库）
    # 如果找到了完整路径，直接使用；如果是库名，链接器会在标准路径中查找
    target_link_libraries(SecureTransferClient PRIVATE ${FLTUSER_LIB})
endif()

# 设置输出目录
set_target_properties(SecureTransferClient PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

