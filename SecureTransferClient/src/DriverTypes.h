#pragma once

#ifndef DRIVERTYPES_H
#define DRIVERTYPES_H

#include <QtGlobal>
#include <QString>

// Windows API 定义（需要包含windows.h）
#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#else
// 非Windows平台的定义（用于编译，但实际运行需要Windows）
#define FILE_DEVICE_UNKNOWN 0x00000022
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif

// 与驱动共享的常量定义
#define MAX_PATH_LENGTH 512
#define MAX_PROCESS_NAME 256
#define MAX_LOG_ENTRIES 1000

// IOCTL 控制码（与驱动中定义相同）
#define IOCTL_ADD_WHITELIST \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_REMOVE_WHITELIST \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_GET_LOGS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_UPDATE_POLICY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_GET_WHITELIST \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_GET_STATISTICS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_CLEAR_LOGS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

// 消息头结构（与驱动中定义一致）
#pragma pack(push, 1)
struct SecureFilterMessageHeader {
    quint32 MessageId;      // IOCTL 控制码
    quint32 Status;         // NTSTATUS 状态码
};
#pragma pack(pop)

// 策略配置结构（与驱动中定义一致）
#pragma pack(push, 1)
struct PolicyConfig {
    wchar_t ProtectedExtensions[256];
    quint8 EnableEncryption;  // BOOLEAN
};
#pragma pack(pop)

// 日志条目结构
#pragma pack(push, 1)
struct LogEntry {
    qint64 Timestamp;  // LARGE_INTEGER
    wchar_t ProcessName[MAX_PROCESS_NAME];
    wchar_t FilePath[MAX_PATH_LENGTH];
    quint32 Action;  // 0=允许, 1=拒绝
};
#pragma pack(pop)

// 统计信息结构
#pragma pack(push, 1)
struct FilterStatistics {
    quint32 TotalInterceptions;
    quint32 AllowedAccess;
    quint32 DeniedAccess;
    quint32 ProtectedFilesAccessed;
};
#pragma pack(pop)

#endif // DRIVERTYPES_H

