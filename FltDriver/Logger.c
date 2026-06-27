// Logger.c
#include "SecureFilter.h"

VOID InitializeLogger(VOID)
{
    RtlZeroMemory(g_SecurityData->LogBuffer, sizeof(g_SecurityData->LogBuffer));
    g_SecurityData->LogIndex = 0;
    KeInitializeSpinLock(&g_SecurityData->LogLock);
}

// 第一次完善：添加 SAL 批注，与头文件声明保持一致
VOID LogEvent(_In_ PWCHAR ProcessName, _In_ PWCHAR FilePath, _In_ ULONG Action)
{
    KIRQL oldIrql;
    PLOG_ENTRY entry;

    KeAcquireSpinLock(&g_SecurityData->LogLock, &oldIrql);

    // 循环缓冲区
    entry = &g_SecurityData->LogBuffer[g_SecurityData->LogIndex % MAX_LOG_ENTRIES];
    g_SecurityData->LogIndex++;

    // 记录信息
    KeQuerySystemTime(&entry->Timestamp);
    RtlStringCbCopyW(entry->ProcessName, sizeof(entry->ProcessName), ProcessName);
    RtlStringCbCopyW(entry->FilePath, sizeof(entry->FilePath), FilePath);
    entry->Action = Action;

    KeReleaseSpinLock(&g_SecurityData->LogLock, oldIrql);
}

// ====================================
// 获取日志（供用户态调用）
// ====================================
// 第一次完善：添加 SAL 批注，与头文件声明保持一致
NTSTATUS GetLogs(
    _Out_writes_bytes_(BufferSize) PLOG_ENTRY Buffer,
    _In_ ULONG BufferSize,
    _Out_ PULONG BytesReturned
)
{
    KIRQL oldIrql;
    ULONG entriesToCopy;
    ULONG bytesNeeded;
    ULONG startIndex;
    ULONG i, j;

    // 参数检查
    if (Buffer == NULL || BytesReturned == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    // 第一次完善：初始化 Buffer 以消除代码分析警告
    if (BufferSize > 0) {
        RtlZeroMemory(Buffer, BufferSize);
    }

    if (BufferSize == 0) {
        *BytesReturned = 0;
        return STATUS_INVALID_PARAMETER;
    }

    *BytesReturned = 0;

    // 获取日志锁
    KeAcquireSpinLock(&g_SecurityData->LogLock, &oldIrql);

    // 计算要复制的日志条目数量
    // 如果日志未满，只复制已有的；如果满了，复制最多 MAX_LOG_ENTRIES 条
    if (g_SecurityData->LogIndex < MAX_LOG_ENTRIES) {
        entriesToCopy = g_SecurityData->LogIndex;
        startIndex = 0;
    }
    else {
        // 日志已满，是循环缓冲区
        entriesToCopy = MAX_LOG_ENTRIES;
        // 计算最早的日志位置
        startIndex = g_SecurityData->LogIndex % MAX_LOG_ENTRIES;
    }

    // 计算需要的字节数
    bytesNeeded = entriesToCopy * sizeof(LOG_ENTRY);

    // 检查缓冲区是否足够大
    if (BufferSize < bytesNeeded) {
        KeReleaseSpinLock(&g_SecurityData->LogLock, oldIrql);

        // 返回需要的大小（让调用者知道需要多大的缓冲区）
        *BytesReturned = bytesNeeded;

        DbgPrint("[+]SecureFilter: GetLogs - Buffer too small. Need %lu bytes, got %lu bytes\n",
            bytesNeeded, BufferSize);

        return STATUS_BUFFER_TOO_SMALL;
    }

    // 如果没有日志
    if (entriesToCopy == 0) {
        KeReleaseSpinLock(&g_SecurityData->LogLock, oldIrql);
        *BytesReturned = 0;
        DbgPrint("[+]SecureFilter: GetLogs - No logs available\n");
        return STATUS_SUCCESS;
    }

    // 复制日志（按时间顺序，从最早的开始）
    j = 0;
    for (i = 0; i < entriesToCopy; i++) {
        ULONG index = (startIndex + i) % MAX_LOG_ENTRIES;

        // 复制日志条目
        RtlCopyMemory(&Buffer[j], &g_SecurityData->LogBuffer[index], sizeof(LOG_ENTRY));
        j++;
    }

    KeReleaseSpinLock(&g_SecurityData->LogLock, oldIrql);

    // 设置返回的字节数
    *BytesReturned = entriesToCopy * sizeof(LOG_ENTRY);

    DbgPrint("[+]SecureFilter: GetLogs - Retrieved %lu log entries (%lu bytes)\n",
        entriesToCopy, *BytesReturned);

    return STATUS_SUCCESS;
}

// ====================================
// 清理日志系统
// ====================================
VOID CleanupLogger(VOID)
{
    KIRQL oldIrql;

    // 第一次完善：实现 CleanupLogger 函数
    KeAcquireSpinLock(&g_SecurityData->LogLock, &oldIrql);

    // 清空日志缓冲区
    RtlZeroMemory(g_SecurityData->LogBuffer, sizeof(g_SecurityData->LogBuffer));
    g_SecurityData->LogIndex = 0;

    KeReleaseSpinLock(&g_SecurityData->LogLock, oldIrql);

    DbgPrint("[+]SecureFilter: Logger cleaned up\n");
}