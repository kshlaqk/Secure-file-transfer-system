#pragma once
// SecureFilter.h

#ifndef _SECURE_FILTER_H_
#define _SECURE_FILTER_H_

#include <fltKernel.h>
#include <ntddk.h>
#include <ntifs.h>      // 第一次完善：添加 ntifs.h 以支持 PsGetProcessImageFileName
#include <ntstrsafe.h>

// ====================================
// 常量定义
// ====================================
#define POOL_TAG 'SFDT'
#define MAX_PATH_LENGTH 512
#define MAX_PROCESS_NAME 256
#define MAX_LOG_ENTRIES 1000

// ====================================
// 进程信息查询相关定义
// ====================================
// PROCESS_QUERY_INFORMATION 常量定义
#ifndef PROCESS_QUERY_INFORMATION
#define PROCESS_QUERY_INFORMATION (0x0400)
#endif

// ProcessImageFileName 常量值
#define PROCESS_IMAGE_FILE_NAME 27

// ZwQueryInformationProcess 函数声明
// 直接使用 ULONG 作为 ProcessInformationClass 类型，避免枚举类型依赖
// 注意：在内核驱动中，ZwQueryInformationProcess 已经通过 ntddk.h 声明，这里不需要重复声明
// 如果需要自定义声明，可以使用以下格式（但通常不需要）：
NTSTATUS ZwQueryInformationProcess(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

// ====================================
// IOCTL 控制码
// ====================================
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

// ====================================
// 数据结构
// ====================================

// 第一次完善：自定义消息头结构（用于 Minifilter 通信端口）
// 注意：避免与 fltUserStructures.h 中的 FILTER_MESSAGE_HEADER 冲突，使用不同的名称
// PortMessageNotify 回调不使用 FLT_MESSAGE_HEADER（该结构在 fltKernel.h 中不存在）
// 需要自定义消息格式，与用户态 FilterSendMessage 配合使用
typedef struct _SECURE_FILTER_MESSAGE_HEADER {
    ULONG MessageId;      // IOCTL 控制码
    NTSTATUS Status;      // 状态码（仅用于输出响应）
} SECURE_FILTER_MESSAGE_HEADER, *PSECURE_FILTER_MESSAGE_HEADER;

// 白名单条目
typedef struct _WHITELIST_ENTRY {
    LIST_ENTRY ListEntry;
    WCHAR ProcessPath[MAX_PATH_LENGTH];
    BOOLEAN IsActive;
} WHITELIST_ENTRY, * PWHITELIST_ENTRY;

// 日志条目
typedef struct _LOG_ENTRY {
    LARGE_INTEGER Timestamp;
    WCHAR ProcessName[MAX_PROCESS_NAME];
    WCHAR FilePath[MAX_PATH_LENGTH];
    ULONG Action;  // 0=允许, 1=拒绝
} LOG_ENTRY, * PLOG_ENTRY;

// 策略配置
#pragma pack(push, 1)
typedef struct _POLICY_CONFIG {
    WCHAR ProtectedExtensions[256];  // "*.docx;*.xlsx;*.pdf"
    BOOLEAN EnableEncryption;
} POLICY_CONFIG, * PPOLICY_CONFIG;
#pragma pack(pop)

// 全局数据
typedef struct _SECURITY_DATA {
    PFLT_FILTER FilterHandle;
    LIST_ENTRY WhitelistHead;
    KSPIN_LOCK WhitelistLock;
    LOG_ENTRY LogBuffer[MAX_LOG_ENTRIES];
    ULONG LogIndex;
    KSPIN_LOCK LogLock;
    POLICY_CONFIG Policy;
    KSPIN_LOCK PolicyLock;
    ULONG TotalInterceptions;
    ULONG AllowedAccess;
    ULONG DeniedAccess;
    ULONG ProtectedFilesAccessed;
} SECURITY_DATA, * PSECURITY_DATA;

// 统计信息结构
typedef struct _FILTER_STATISTICS {
    ULONG TotalInterceptions;
    ULONG AllowedAccess;
    ULONG DeniedAccess;
    ULONG ProtectedFilesAccessed;
} FILTER_STATISTICS, * PFILTER_STATISTICS;

// ====================================
// 全局变量声明（extern）
// ====================================
extern PFLT_FILTER g_FilterHandle;
extern PSECURITY_DATA g_SecurityData;

// ====================================
// 函数声明
// ====================================

// SecureFilter.c
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);
NTSTATUS FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
);
NTSTATUS InstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

// Callbacks.c
FLT_PREOP_CALLBACK_STATUS PreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS PostCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS PreReadCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS PostReadCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS PreWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS PostWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS PreSetInformationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS PostSetInformationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

// Whitelist.c
// 第一次完善：添加 SAL 批注，修复代码分析警告
VOID InitializeWhitelist(VOID);
VOID CleanupWhitelist(VOID);
NTSTATUS AddToWhitelist(_In_ PWCHAR ProcessPath);
NTSTATUS RemoveFromWhitelist(_In_ PWCHAR ProcessPath);
BOOLEAN IsProcessInWhitelist(_In_ PWCHAR ProcessPath);

// Communication.c
// 第一次完善：重构为使用 Minifilter 标准通信端口
NTSTATUS CreateCommunicationPort(VOID);
VOID DeleteCommunicationPort(VOID);
NTSTATUS HandleIoctl(
    _In_ ULONG ControlCode,
    _In_opt_ PVOID InputBuffer,
    _In_ ULONG InputLength,
    _Out_opt_ PVOID OutputBuffer,
    _In_ ULONG OutputLength,
    _Out_opt_ PULONG BytesReturned
);

// Logger.c
// 第一次完善：添加 SAL 批注，修复代码分析警告
VOID InitializeLogger(VOID);
VOID CleanupLogger(VOID);
VOID LogEvent(_In_ PWCHAR ProcessName, _In_ PWCHAR FilePath, _In_ ULONG Action);
NTSTATUS GetLogs(
    _Out_writes_bytes_(BufferSize) PLOG_ENTRY Buffer,
    _In_ ULONG BufferSize,
    _Out_ PULONG BytesReturned
);

// Utils.c
// 第一次完善：添加 SAL 批注，修复代码分析警告
NTSTATUS GetProcessName(
    _In_ PEPROCESS Process,
    _Out_writes_bytes_(BufferLength) PWCHAR Buffer,
    _In_ ULONG BufferLength
);
BOOLEAN IsProtectedFile(_In_ PUNICODE_STRING FilePath);
BOOLEAN IsProtectedExtension(_In_ PUNICODE_STRING Extension);
BOOLEAN IsProtectedExtensionDefault(_In_ PUNICODE_STRING Extension);
BOOLEAN IsInProtectedDownloadPath(_In_ PUNICODE_STRING FilePath);
VOID ExtractExtension(
    _In_ PUNICODE_STRING FilePath,
    _Out_ PUNICODE_STRING Extension
);
PWCHAR KernelWcsTok(
    _Inout_opt_ PWCHAR String,
    _In_ PCWSTR Delimiters,
    _Inout_ PWCHAR* Context
);

// ====================================
// 为文档化函数手动声明
// ====================================
//NTKERNELAPI
//PUCHAR
//PsGetProcessImageFileName(
//    _In_ PEPROCESS Process
//);

#endif // _SECURE_FILTER_H_