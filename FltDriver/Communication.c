// Communication.c
#include "SecureFilter.h"

// Minifilter 通信端口句柄
PFLT_PORT g_ServerPort = NULL;

// ====================================
// 通信端口连接通知回调
// ====================================
NTSTATUS PortConnectNotify(
    _In_ PFLT_PORT ClientPort,
    _In_opt_ PVOID ServerPortCookie,
    _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID *ConnectionPortCookie
)
{
    UNREFERENCED_PARAMETER(ClientPort);
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionPortCookie);

    DbgPrint("[+]SecureFilter: Client connected to communication port\n");
    return STATUS_SUCCESS;
}

// ====================================
// 通信端口断开通知回调
// ====================================
VOID PortDisconnectNotify(
    _In_opt_ PVOID ConnectionCookie
)
{
    UNREFERENCED_PARAMETER(ConnectionCookie);
    DbgPrint("[+]SecureFilter: Client disconnected from communication port\n");
    // 注意：这里不需要关闭端口，端口应该保持打开以接受新连接
    // FltMgr 会自动处理客户端连接的关闭
}

// ====================================
// 消息接收回调
// ====================================
NTSTATUS PortMessageNotify(
    _In_opt_ PVOID PortCookie,
    _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PSECURE_FILTER_MESSAGE_HEADER messageHeader;
    ULONG bytesReturned = 0;

    UNREFERENCED_PARAMETER(PortCookie);

    // 第一次完善：使用自定义消息格式 SECURE_FILTER_MESSAGE_HEADER
    // 注意：避免与 fltUserStructures.h 中的 FILTER_MESSAGE_HEADER 冲突
    if (InputBuffer == NULL || InputBufferLength < sizeof(SECURE_FILTER_MESSAGE_HEADER)) {
        if (ReturnOutputBufferLength != NULL) {
            *ReturnOutputBufferLength = 0;
        }
        return STATUS_INVALID_PARAMETER;
    }

    messageHeader = (PSECURE_FILTER_MESSAGE_HEADER)InputBuffer;

    // 调用 IOCTL 处理函数（使用 MessageId 作为 ControlCode）
    status = HandleIoctl(
        messageHeader->MessageId,
        (PUCHAR)InputBuffer + sizeof(SECURE_FILTER_MESSAGE_HEADER),
        InputBufferLength - sizeof(SECURE_FILTER_MESSAGE_HEADER),
        OutputBuffer ? (PUCHAR)OutputBuffer + sizeof(SECURE_FILTER_MESSAGE_HEADER) : NULL,
        OutputBuffer ? OutputBufferLength - sizeof(SECURE_FILTER_MESSAGE_HEADER) : 0,
        &bytesReturned
    );

    if (OutputBuffer != NULL && ReturnOutputBufferLength != NULL) {
        PSECURE_FILTER_MESSAGE_HEADER outHeader = (PSECURE_FILTER_MESSAGE_HEADER)OutputBuffer;
        outHeader->Status = status;
        outHeader->MessageId = messageHeader->MessageId;  // 回显 MessageId
        *ReturnOutputBufferLength = sizeof(SECURE_FILTER_MESSAGE_HEADER) + bytesReturned;
    } else if (ReturnOutputBufferLength != NULL) {
        *ReturnOutputBufferLength = 0;
    }

    return status;
}

// ====================================
// 创建 Minifilter 通信端口
// ====================================
NTSTATUS CreateCommunicationPort(VOID)
{
    NTSTATUS status;
    PSECURITY_DESCRIPTOR sd = NULL;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING portName;

    // 第一次完善：重构为使用 FltCreateCommunicationPort
    RtlInitUnicodeString(&portName, L"\\SecureFilterPort");

    // 创建安全描述符，允许所有用户访问
    status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[+]SecureFilter: Failed to build security descriptor: 0x%X\n", status);
        return status;
    }

    InitializeObjectAttributes(
        &oa,
        &portName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        sd
    );

    // 创建通信端口
    status = FltCreateCommunicationPort(
        g_FilterHandle,
        &g_ServerPort,
        &oa,
        NULL,
        PortConnectNotify,
        PortDisconnectNotify,
        PortMessageNotify,
        1  // 最大连接数
    );

    if (sd != NULL) {
        FltFreeSecurityDescriptor(sd);
    }

    if (NT_SUCCESS(status)) {
        DbgPrint("[+]SecureFilter: Communication port created successfully\n");
    } else {
        DbgPrint("[+]SecureFilter: Failed to create communication port: 0x%X\n", status);
    }

    return status;
}

// ====================================
// 删除通信端口
// ====================================
VOID DeleteCommunicationPort(VOID)
{
    // 第一次完善：使用 FltCloseCommunicationPort 关闭端口
    if (g_ServerPort != NULL) {
        FltCloseCommunicationPort(g_ServerPort);
        g_ServerPort = NULL;
        DbgPrint("[+]SecureFilter: Communication port closed\n");
    }
}

// ====================================
// 处理 IOCTL 请求（保持不变，但通过 PortMessageNotify 调用）
// ====================================
NTSTATUS HandleIoctl(
    _In_ ULONG ControlCode,
    _In_opt_ PVOID InputBuffer,
    _In_ ULONG InputLength,
    _Out_opt_ PVOID OutputBuffer,
    _In_ ULONG OutputLength,
    _Out_opt_ PULONG BytesReturned
)
{
    NTSTATUS status = STATUS_SUCCESS;

    // 第一次完善：初始化 OutputBuffer 以消除代码分析警告
    // 某些 IOCTL 操作不使用 OutputBuffer，但为了满足 _Out_ 批注要求，需要初始化它
    if (OutputBuffer != NULL && OutputLength > 0) {
        RtlZeroMemory(OutputBuffer, OutputLength);
    }

    if (BytesReturned != NULL) {
        *BytesReturned = 0;
    }

    switch (ControlCode) {

        // ====================================
        // 添加进程到白名单
        // ====================================
    case IOCTL_ADD_WHITELIST:
    {
        if (InputBuffer == NULL || InputLength < sizeof(WCHAR)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        // 检查字符串是否以 NULL 结尾
        PWCHAR processPath = (PWCHAR)InputBuffer;
        ULONG maxLen = InputLength / sizeof(WCHAR);
        BOOLEAN nullTerminated = FALSE;

        for (ULONG i = 0; i < maxLen; i++) {
            if (processPath[i] == L'\0') {
                nullTerminated = TRUE;
                break;
            }
        }

        if (!nullTerminated) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        // 添加到白名单
        status = AddToWhitelist(processPath);

        if (NT_SUCCESS(status)) {
            DbgPrint("[+]SecureFilter: IOCTL - Added to whitelist: %ws\n", processPath);
        }
        else {
            DbgPrint("[+]SecureFilter: IOCTL - Failed to add to whitelist: 0x%X\n", status);
        }

        break;
    }

    // ====================================
    // 从白名单中移除进程
    // ====================================
    case IOCTL_REMOVE_WHITELIST:
    {
        if (InputBuffer == NULL || InputLength < sizeof(WCHAR)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        PWCHAR processPath = (PWCHAR)InputBuffer;
        ULONG maxLen = InputLength / sizeof(WCHAR);
        BOOLEAN nullTerminated = FALSE;

        for (ULONG i = 0; i < maxLen; i++) {
            if (processPath[i] == L'\0') {
                nullTerminated = TRUE;
                break;
            }
        }

        if (!nullTerminated) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        // 从白名单移除
        status = RemoveFromWhitelist(processPath);

        if (NT_SUCCESS(status)) {
            DbgPrint("[+]SecureFilter: IOCTL - Removed from whitelist: %ws\n", processPath);
        }
        else if (status == STATUS_NOT_FOUND) {
            DbgPrint("[+]SecureFilter: IOCTL - Process not in whitelist: %ws\n", processPath);
        }
        else {
            DbgPrint("[+]SecureFilter: IOCTL - Failed to remove: 0x%X\n", status);
        }

        break;
    }

    // ====================================
    // 获取日志
    // ====================================
    case IOCTL_GET_LOGS:
    {
        if (OutputBuffer == NULL || OutputLength == 0) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        ULONG bytesReturned = 0;
        status = GetLogs((PLOG_ENTRY)OutputBuffer, OutputLength, &bytesReturned);

        if (NT_SUCCESS(status) && BytesReturned != NULL) {
            *BytesReturned = bytesReturned;
            DbgPrint("[+]SecureFilter: IOCTL - Retrieved %lu bytes of logs\n", bytesReturned);
        }

        break;
    }

    // ====================================
    // 更新策略配置
    // ====================================
    case IOCTL_UPDATE_POLICY:
    {
        DbgPrint("[+]SecureFilter: ===== IOCTL_UPDATE_POLICY DEBUG START =====\n");
        DbgPrint("[+]SecureFilter: InputBuffer: %p\n", InputBuffer);
        DbgPrint("[+]SecureFilter: InputLength: %lu bytes\n", InputLength);
        DbgPrint("[+]SecureFilter: Expected sizeof(POLICY_CONFIG): %lu bytes\n", sizeof(POLICY_CONFIG));
        
        if (InputBuffer == NULL || InputLength < sizeof(POLICY_CONFIG)) {
            DbgPrint("[+]SecureFilter: ERROR - Invalid parameter (InputBuffer=%p, InputLength=%lu)\n", 
                     InputBuffer, InputLength);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        // 打印原始数据的十六进制（前64字节）
        DbgPrint("[+]SecureFilter: Raw data (first 64 bytes, hex):\n");
        PUCHAR rawData = (PUCHAR)InputBuffer;
        for (ULONG i = 0; i < 64 && i < InputLength; i++) {
            DbgPrint("%02X ", rawData[i]);
            if ((i + 1) % 16 == 0) {
                DbgPrint("\n");
            }
        }
        DbgPrint("\n");

        PPOLICY_CONFIG newPolicy = (PPOLICY_CONFIG)InputBuffer;

        // 打印接收到的字符串内容（用于调试）
        DbgPrint("[+]SecureFilter: Received ProtectedExtensions (first 200 chars):\n");
        DbgPrint("[+]SecureFilter: %.*ws\n", 200, newPolicy->ProtectedExtensions);
        
        DbgPrint("[+]SecureFilter: Received EnableEncryption: %d (0x%02X)\n", 
                 newPolicy->EnableEncryption, (UCHAR)newPolicy->EnableEncryption);

        // 检查字符串是否以NULL结尾
        ULONG extLen = 0;
        for (ULONG i = 0; i < 256; i++) {
            if (newPolicy->ProtectedExtensions[i] == L'\0') {
                extLen = i;
                break;
            }
        }
        DbgPrint("[+]SecureFilter: ProtectedExtensions length: %lu chars\n", extLen);

        // 更新全局策略（需要加锁）
        KIRQL oldIrql;
        KeAcquireSpinLock(&g_SecurityData->PolicyLock, &oldIrql);

        // 打印更新前的策略
        DbgPrint("[+]SecureFilter: Before update - Current policy:\n");
        DbgPrint("[+]SecureFilter:   Extensions: %ws\n", g_SecurityData->Policy.ProtectedExtensions);
        DbgPrint("[+]SecureFilter:   Encryption: %d\n", g_SecurityData->Policy.EnableEncryption);

        // 复制策略数据
        RtlCopyMemory(&g_SecurityData->Policy, newPolicy, sizeof(POLICY_CONFIG));

        // 打印更新后的策略
        DbgPrint("[+]SecureFilter: After update - New policy:\n");
        DbgPrint("[+]SecureFilter:   Extensions: %ws\n", g_SecurityData->Policy.ProtectedExtensions);
        DbgPrint("[+]SecureFilter:   Encryption: %d\n", g_SecurityData->Policy.EnableEncryption);

        KeReleaseSpinLock(&g_SecurityData->PolicyLock, oldIrql);

        DbgPrint("[+]SecureFilter: ===== IOCTL_UPDATE_POLICY DEBUG END =====\n");

        status = STATUS_SUCCESS;
        break;
    }

    // ====================================
    // 获取白名单列表
    // ====================================
    case IOCTL_GET_WHITELIST:
    {
        if (OutputBuffer == NULL || OutputLength == 0) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        KIRQL oldIrql;
        PLIST_ENTRY entry;
        PWCHAR output = (PWCHAR)OutputBuffer;
        ULONG totalBytes = 0;
        ULONG remainingBytes = OutputLength;

        KeAcquireSpinLock(&g_SecurityData->WhitelistLock, &oldIrql);

        // 遍历白名单
        for (entry = g_SecurityData->WhitelistHead.Flink;
            entry != &g_SecurityData->WhitelistHead;
            entry = entry->Flink)
        {
            PWHITELIST_ENTRY item = CONTAINING_RECORD(entry, WHITELIST_ENTRY, ListEntry);
            UNICODE_STRING pathStr;

            // 使用内核模式的字符串操作获取长度
            RtlInitUnicodeString(&pathStr, item->ProcessPath);
            ULONG pathLen = pathStr.Length + sizeof(WCHAR); // 包括 null 终止符

            // 检查缓冲区是否足够
            if (remainingBytes < pathLen) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            // 复制路径
            RtlCopyMemory(output, item->ProcessPath, pathLen);
            output += pathLen / sizeof(WCHAR);
            totalBytes += pathLen;
            remainingBytes -= pathLen;
        }

        KeReleaseSpinLock(&g_SecurityData->WhitelistLock, oldIrql);

        if (NT_SUCCESS(status) && BytesReturned != NULL) {
            *BytesReturned = totalBytes;
        }

        break;
    }

    // ====================================
    // 获取统计信息
    // ====================================
    case IOCTL_GET_STATISTICS:
    {
        if (OutputBuffer == NULL || OutputLength < sizeof(FILTER_STATISTICS)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        PFILTER_STATISTICS stats = (PFILTER_STATISTICS)OutputBuffer;

        // 填充统计数据
        stats->TotalInterceptions = g_SecurityData->TotalInterceptions;
        stats->AllowedAccess = g_SecurityData->AllowedAccess;
        stats->DeniedAccess = g_SecurityData->DeniedAccess;
        stats->ProtectedFilesAccessed = g_SecurityData->ProtectedFilesAccessed;

        if (BytesReturned != NULL) {
            *BytesReturned = sizeof(FILTER_STATISTICS);
        }

        DbgPrint("[+]SecureFilter: IOCTL - Statistics retrieved\n");
        status = STATUS_SUCCESS;
        break;
    }

    // ====================================
    // 清空日志
    // ====================================
    case IOCTL_CLEAR_LOGS:
    {
        KIRQL oldIrql;
        KeAcquireSpinLock(&g_SecurityData->LogLock, &oldIrql);

        // 清空日志缓冲区
        RtlZeroMemory(g_SecurityData->LogBuffer, sizeof(g_SecurityData->LogBuffer));
        g_SecurityData->LogIndex = 0;

        KeReleaseSpinLock(&g_SecurityData->LogLock, oldIrql);

        DbgPrint("[+]SecureFilter: IOCTL - Logs cleared\n");
        status = STATUS_SUCCESS;
        break;
    }

    // ====================================
    // 未知的控制码
    // ====================================
    default:
    {
        DbgPrint("[+]SecureFilter: IOCTL - Unknown control code: 0x%X\n", ControlCode);
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }
    }

    return status;
}
