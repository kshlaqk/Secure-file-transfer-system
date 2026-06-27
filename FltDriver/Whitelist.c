// Whitelist.c
#pragma warning(disable: 4996)
#include "SecureFilter.h"

// ====================================
// 初始化白名单
// ====================================
VOID InitializeWhitelist(VOID)
{
    InitializeListHead(&g_SecurityData->WhitelistHead);

    // 添加默认白名单（使用路径后缀格式，包含目录结构）
    // 路径后缀匹配：匹配从系统目录开始的后缀部分
    // 例如：\Device\HarddiskVolume4\Windows\System32\notepad.exe 的后缀是 Windows\System32\notepad.exe
    AddToWhitelist(L"Windows\\System32\\notepad.exe");
    AddToWhitelist(L"whitelist\\client\\SecureTransferClient.exe");
    AddToWhitelist(L"Windows\\explorer.exe");
    AddToWhitelist(L"Windows\\System32\\svchost.exe"); 
    AddToWhitelist(L"Windows\\System32\\SearchProtocolHost.exe"); 
}

// ====================================
// 添加到白名单
// ====================================
NTSTATUS AddToWhitelist(_In_ PWCHAR ProcessPath)
{
    PWHITELIST_ENTRY entry;
    KIRQL oldIrql;

    // 分配内存
    entry = (PWHITELIST_ENTRY)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(WHITELIST_ENTRY),
        POOL_TAG
    );

    if (entry == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // 复制路径
    RtlStringCbCopyW(entry->ProcessPath, sizeof(entry->ProcessPath), ProcessPath);
    entry->IsActive = TRUE;

    // 加入链表
    KeAcquireSpinLock(&g_SecurityData->WhitelistLock, &oldIrql);
    InsertTailList(&g_SecurityData->WhitelistHead, &entry->ListEntry);
    KeReleaseSpinLock(&g_SecurityData->WhitelistLock, oldIrql);

    DbgPrint("[+]SecureFilter: Added to whitelist: %ws\n", ProcessPath);

    return STATUS_SUCCESS;
}

// ====================================
// 提取路径后缀（去掉设备路径前缀）
// ====================================
// 统一处理：去掉 \Device\HarddiskVolumeX\ 前缀，提取实际文件系统路径
// 例如：\Device\HarddiskVolume4\whitelist\client\SecureTransferClient.exe -> whitelist\client\SecureTransferClient.exe
//       \Device\HarddiskVolume4\Windows\System32\notepad.exe -> Windows\System32\notepad.exe
static PWCHAR ExtractPathSuffix(_In_ PWCHAR FullPath)
{
    if (FullPath == NULL) {
        return NULL;
    }

    // 查找设备路径前缀 \Device\HarddiskVolume
    PWCHAR devicePos = wcsstr(FullPath, L"\\Device\\HarddiskVolume");
    if (devicePos != NULL) {
        // 跳过 \Device\HarddiskVolume 部分
        PWCHAR afterDevice = devicePos + wcslen(L"\\Device\\HarddiskVolume");
        
        // 跳过卷号（数字）和反斜杠
        while (*afterDevice >= L'0' && *afterDevice <= L'9') {
            afterDevice++;
        }
        if (*afterDevice == L'\\') {
            afterDevice++; // 跳过反斜杠
        }
        
        return afterDevice;
    }

    // 如果没有设备路径前缀，直接返回原路径
    return FullPath;
}

// ====================================
// 检查是否在白名单（统一使用路径后缀匹配）
// ====================================
// 所有路径都使用后缀匹配：去掉设备路径前缀后比较
// 例如：\Device\HarddiskVolume4\whitelist\client\SecureTransferClient.exe 匹配 whitelist\client\SecureTransferClient.exe
BOOLEAN IsProcessInWhitelist(_In_ PWCHAR ProcessPath)
{
    KIRQL oldIrql;
    PLIST_ENTRY entry;
    BOOLEAN found = FALSE;
    PWCHAR processSuffix = NULL;
    PWCHAR itemSuffix = NULL;
    UNICODE_STRING processSuffixStr, itemSuffixStr;
    LONG compareResult;

    if (ProcessPath == NULL) {
        return FALSE;
    }

    // 提取进程路径的后缀（去掉设备路径前缀）
    processSuffix = ExtractPathSuffix(ProcessPath);

    KeAcquireSpinLock(&g_SecurityData->WhitelistLock, &oldIrql);

    for (entry = g_SecurityData->WhitelistHead.Flink;
        entry != &g_SecurityData->WhitelistHead;
        entry = entry->Flink)
    {
        PWHITELIST_ENTRY item = CONTAINING_RECORD(
            entry,
            WHITELIST_ENTRY,
            ListEntry
        );

        // 提取白名单项路径的后缀（统一处理，即使白名单项可能已经是相对路径）
        itemSuffix = ExtractPathSuffix(item->ProcessPath);

        // 比较路径后缀（必须完全匹配，包含目录结构）
        RtlInitUnicodeString(&processSuffixStr, processSuffix);
        RtlInitUnicodeString(&itemSuffixStr, itemSuffix);
        compareResult = RtlCompareUnicodeString(&processSuffixStr, &itemSuffixStr, TRUE);

        if (compareResult == 0 && item->IsActive) {
            found = TRUE;
            break;
        }
    }

    KeReleaseSpinLock(&g_SecurityData->WhitelistLock, oldIrql);

    return found;
}

// ====================================
// 从白名单中移除进程
// ====================================
NTSTATUS RemoveFromWhitelist(_In_ PWCHAR ProcessPath)
{
    KIRQL oldIrql;
    PLIST_ENTRY entry;
    PWHITELIST_ENTRY item;
    BOOLEAN found = FALSE;
    UNICODE_STRING processPathStr, itemPathStr;
    LONG compareResult;

    if (ProcessPath == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    // 初始化要比较的 UNICODE_STRING
    RtlInitUnicodeString(&processPathStr, ProcessPath);

    // 获取自旋锁
    KeAcquireSpinLock(&g_SecurityData->WhitelistLock, &oldIrql);

    // 遍历链表查找匹配项
    for (entry = g_SecurityData->WhitelistHead.Flink;
        entry != &g_SecurityData->WhitelistHead;
        entry = entry->Flink)
    {
        item = CONTAINING_RECORD(entry, WHITELIST_ENTRY, ListEntry);

        // 使用内核模式的字符串比较函数（TRUE 表示不区分大小写）
        RtlInitUnicodeString(&itemPathStr, item->ProcessPath);
        compareResult = RtlCompareUnicodeString(&itemPathStr, &processPathStr, TRUE);

        // 比较路径（不区分大小写）
        if (compareResult == 0) {
            // 找到了，从链表中移除
            RemoveEntryList(&item->ListEntry);
            found = TRUE;

            DbgPrint("[+]SecureFilter: Removed from whitelist: %ws\n", ProcessPath);

            // 释放锁后再释放内存（避免死锁）
            KeReleaseSpinLock(&g_SecurityData->WhitelistLock, oldIrql);

            // 释放内存
            ExFreePoolWithTag(item, POOL_TAG);

            return STATUS_SUCCESS;
        }
    }

    // 释放锁
    KeReleaseSpinLock(&g_SecurityData->WhitelistLock, oldIrql);

    // 没找到
    if (!found) {
        DbgPrint("[+]SecureFilter: Process not in whitelist: %ws\n", ProcessPath);
        return STATUS_NOT_FOUND;
    }

    return STATUS_SUCCESS;
}

// ====================================
// 清空整个白名单
// ====================================
VOID CleanupWhitelist(VOID)
{
    KIRQL oldIrql;
    PLIST_ENTRY entry;
    PWHITELIST_ENTRY item;

    KeAcquireSpinLock(&g_SecurityData->WhitelistLock, &oldIrql);

    // 遍历并删除所有条目
    while (!IsListEmpty(&g_SecurityData->WhitelistHead)) {
        entry = RemoveHeadList(&g_SecurityData->WhitelistHead);
        item = CONTAINING_RECORD(entry, WHITELIST_ENTRY, ListEntry);

        // 释放锁后释放内存
        KeReleaseSpinLock(&g_SecurityData->WhitelistLock, oldIrql);
        ExFreePoolWithTag(item, POOL_TAG);
        KeAcquireSpinLock(&g_SecurityData->WhitelistLock, &oldIrql);
    }

    KeReleaseSpinLock(&g_SecurityData->WhitelistLock, oldIrql);

    DbgPrint("[+]SecureFilter: Whitelist cleared\n");
}