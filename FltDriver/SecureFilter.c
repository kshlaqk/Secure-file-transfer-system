// SecureFilter.c
#pragma warning(disable: 4996)
#include <fltKernel.h>
#include "SecureFilter.h"

// ====================================
// 全局变量定义
// ====================================
PFLT_FILTER g_FilterHandle = NULL;
PSECURITY_DATA g_SecurityData = NULL;

// ====================================
// 回调数组定义
// ====================================
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    {
        IRP_MJ_CREATE,                   // 文件创建
        0,
        PreCreateCallback,
        PostCreateCallback
    },
    {
        IRP_MJ_READ,                     // 文件读取
        0,
        PreReadCallback,
        PostReadCallback
    },
    {
        IRP_MJ_WRITE,                    // 文件写入
        0,
        PreWriteCallback,
        PostWriteCallback
    },
    {
        IRP_MJ_SET_INFORMATION,         // 文件重命名/移动
        0,
        PreSetInformationCallback,
        PostSetInformationCallback
    },
    { IRP_MJ_OPERATION_END }            // 结束标记
};

// ====================================
// 过滤器注册结构
// ====================================
CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),            // Size
    FLT_REGISTRATION_VERSION,            // Version
    0,                                   // Flags
    NULL,                                // Context
    Callbacks,                           // 回调数组
    FilterUnload,                        // Unload
    InstanceSetup,                       // InstanceSetup
    NULL,                                // InstanceQueryTeardown
    NULL,                                // InstanceTeardownStart
    NULL,                                // InstanceTeardownComplete
    NULL, NULL, NULL                     // 其他可选回调
};

// ====================================
// 驱动入口点
// ====================================
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
  NTSTATUS status = STATUS_SUCCESS;

  UNREFERENCED_PARAMETER(RegistryPath);

  // 第一次完善：分配并初始化全局数据结构
  // 1. 分配全局数据结构
 g_SecurityData = (PSECURITY_DATA)ExAllocatePoolWithTag(
     NonPagedPool,
     sizeof(SECURITY_DATA),
     POOL_TAG
 );

 if (g_SecurityData == NULL) {
     DbgPrint("[+]SecureFilter: Failed to allocate security data\n");
     return STATUS_INSUFFICIENT_RESOURCES;
 }

 // 清零内存
 RtlZeroMemory(g_SecurityData, sizeof(SECURITY_DATA));

 // 2. 初始化锁
 KeInitializeSpinLock(&g_SecurityData->WhitelistLock);
 KeInitializeSpinLock(&g_SecurityData->LogLock);
 KeInitializeSpinLock(&g_SecurityData->PolicyLock);

 // 3. 初始化统计信息
 g_SecurityData->TotalInterceptions = 0;
 g_SecurityData->AllowedAccess = 0;
 g_SecurityData->DeniedAccess = 0;
 g_SecurityData->ProtectedFilesAccessed = 0;

 // 4. 初始化策略配置（默认值）
 RtlStringCbCopyW(
     g_SecurityData->Policy.ProtectedExtensions,
     sizeof(g_SecurityData->Policy.ProtectedExtensions),
     L".docx;.xlsx;.pptx;.pdf;.doc;.xls;.ppt;.temp;.txt"
 );

 g_SecurityData->Policy.EnableEncryption = FALSE;

 DbgPrint("[+]Initialization successful\n");

 //5. 初始化白名单
 InitializeWhitelist();

 DbgPrint("[+]InitializeWhitelist OK\n");

 //6. 初始化日志系统
 InitializeLogger();

 DbgPrint("[+]InitializeLogger OK\n");

 //7. 注册 Minifilter（在创建通信端口之前）
 status = FltRegisterFilter(
     DriverObject,
     &FilterRegistration,
     &g_FilterHandle
 );

 if (!NT_SUCCESS(status)) {
     DbgPrint("[+]SecureFilter: Failed to register filter: 0x%X\n", status);
     goto Cleanup;
 }

 // 8. 创建 Minifilter 通信端口（使用 FltCreateCommunicationPort）
 status = CreateCommunicationPort();
 
 if (!NT_SUCCESS(status)) {
     DbgPrint("[+]SecureFilter: Failed to create communication port: 0x%X\n", status);
     goto Cleanup;
 }
 
 // 9. 启动过滤
 status = FltStartFiltering(g_FilterHandle);
 
 if (!NT_SUCCESS(status)) {
     DbgPrint("[+]SecureFilter: Failed to start filtering: 0x%X\n", status);
     goto Cleanup;
 }
 
 DbgPrint("[+]SecureFilter: Driver loaded successfully\n");
 return status;
 
Cleanup:
    if (g_FilterHandle) {
        FltUnregisterFilter(g_FilterHandle);
    }
    if (g_SecurityData) {
        ExFreePoolWithTag(g_SecurityData, POOL_TAG);
    }
  return status;
}

// ====================================
// 驱动卸载
// ====================================
NTSTATUS FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
{
    DbgPrint("[+]SecureFilter: Unloading driver\n");

    UNREFERENCED_PARAMETER(Flags);

    // 清理资源
    CleanupWhitelist();
    CleanupLogger();
    DeleteCommunicationPort();

    FltUnregisterFilter(g_FilterHandle);

    if (g_SecurityData) {
        ExFreePoolWithTag(g_SecurityData, POOL_TAG);
    }

    DbgPrint("[+]Free OK\n");

    return STATUS_SUCCESS;
}

// ====================================
// 实例设置
// ====================================
NTSTATUS InstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(FltObjects);

    // 只附加到 NTFS 文件系统
    if (VolumeFilesystemType != FLT_FSTYPE_NTFS) {
        return STATUS_FLT_DO_NOT_ATTACH;
    }

    return STATUS_SUCCESS;
}