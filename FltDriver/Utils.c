// Utils.c
#pragma warning(disable: 4996)
#include "SecureFilter.h"

// ====================================
// wcstok_s内核版本替代实现字符串分割功能
// ====================================
PWCHAR KernelWcsTok(
    _Inout_opt_ PWCHAR String,
    _In_ PCWSTR Delimiters,
    _Inout_ PWCHAR* Context
)
{
    PWCHAR token;

    // 如果 String 为 NULL，使用上次保存的位置
    if (String == NULL) {
        String = *Context;
    }

    if (String == NULL) {
        return NULL;
    }

    // 跳过前导分隔符
    while (*String != L'\0') {
        BOOLEAN isDelimiter = FALSE;
        for (PCWSTR delim = Delimiters; *delim != L'\0'; delim++) {
            if (*String == *delim) {
                isDelimiter = TRUE;
                break;
            }
        }
        if (!isDelimiter) {
            break;
        }
        String++;
    }

    if (*String == L'\0') {
        *Context = NULL;
        return NULL;
    }

    // 找到 token 的开始
    token = String;

    // 查找下一个分隔符
    while (*String != L'\0') {
        for (PCWSTR delim = Delimiters; *delim != L'\0'; delim++) {
            if (*String == *delim) {
                *String = L'\0';
                *Context = String + 1;
                return token;
            }
        }
        String++;
    }

    *Context = NULL;
    return token;
}

// 获取进程名称（完整路径）
// 修复：使用 ZwQueryInformationProcess 获取进程完整路径
// 正确处理 STATUS_INFO_LENGTH_MISMATCH 错误
NTSTATUS GetProcessName(
    _In_ PEPROCESS Process,
    _Out_writes_bytes_(BufferLength) PWCHAR Buffer,
    _In_ ULONG BufferLength
)
{
    NTSTATUS status;
    HANDLE processHandle = NULL;
    ULONG returnLength = 0;
    UNICODE_STRING imagePath;
    ULONG copyLength;
    PVOID infoBuffer = NULL;
    ULONG infoBufferSize = 0;

    // 初始化 Buffer
    if (Buffer != NULL && BufferLength > 0) {
        Buffer[0] = L'\0';
    }

    if (Buffer == NULL || BufferLength == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // 打开进程句柄
    status = ObOpenObjectByPointer(
        Process,
        OBJ_KERNEL_HANDLE,
        NULL,
        PROCESS_QUERY_INFORMATION,
        *PsProcessType,
        KernelMode,
        &processHandle
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("[+]SecureFilter: Failed to open process handle: 0x%X\n", status);
        return status;
    }

    // 第一次调用：获取所需缓冲区大小
    RtlZeroMemory(&imagePath, sizeof(UNICODE_STRING));
    status = ZwQueryInformationProcess(
        processHandle,
        PROCESS_IMAGE_FILE_NAME,  // ProcessImageFileName 的值 (27)
        &imagePath,
        sizeof(UNICODE_STRING),
        &returnLength
    );

    // 期望返回 STATUS_INFO_LENGTH_MISMATCH，表示需要更大的缓冲区
    if (status != STATUS_INFO_LENGTH_MISMATCH && status != STATUS_BUFFER_OVERFLOW) {
        DbgPrint("[+]SecureFilter: Failed to query process info size: 0x%X\n", status);
        ZwClose(processHandle);
        return status;
    }

    // 分配足够大的缓冲区（returnLength 包含 UNICODE_STRING + 字符串数据的大小）
    infoBufferSize = returnLength;
    if (infoBufferSize < sizeof(UNICODE_STRING) + sizeof(WCHAR)) {
        // 如果返回的大小太小，使用默认大小
        infoBufferSize = sizeof(UNICODE_STRING) + MAX_PATH_LENGTH * sizeof(WCHAR);
    }

    infoBuffer = ExAllocatePoolWithTag(
        NonPagedPool,
        infoBufferSize,
        POOL_TAG
    );

    if (infoBuffer == NULL) {
        DbgPrint("[+]SecureFilter: Failed to allocate buffer for process path\n");
        ZwClose(processHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // 清零缓冲区
    RtlZeroMemory(infoBuffer, infoBufferSize);

    // 第二次调用：获取完整路径信息
    status = ZwQueryInformationProcess(
        processHandle,
        PROCESS_IMAGE_FILE_NAME,
        infoBuffer,
        infoBufferSize,
        &returnLength
    );

    ZwClose(processHandle);

    if (!NT_SUCCESS(status)) {
        DbgPrint("[+]SecureFilter: Failed to query process image path: 0x%X\n", status);
        ExFreePoolWithTag(infoBuffer, POOL_TAG);
        return status;
    }

    // infoBuffer 现在包含 UNICODE_STRING 结构
    imagePath = *(PUNICODE_STRING)infoBuffer;

    // 检查路径是否有效
    if (imagePath.Buffer == NULL || imagePath.Length == 0) {
        DbgPrint("[+]SecureFilter: Process image path is empty\n");
        ExFreePoolWithTag(infoBuffer, POOL_TAG);
        return STATUS_UNSUCCESSFUL;
    }

    // 直接使用设备路径（用于路径后缀匹配）
    copyLength = min(imagePath.Length, (BufferLength - 1) * sizeof(WCHAR));
    RtlCopyMemory(Buffer, imagePath.Buffer, copyLength);
    Buffer[copyLength / sizeof(WCHAR)] = L'\0';
    
    DbgPrint("[+]SecureFilter: Process path: %ws\n", Buffer);

    ExFreePoolWithTag(infoBuffer, POOL_TAG);
    return STATUS_SUCCESS;
}

// ====================================
// 检查文件是否在受保护的下载目录中 (C:\whitelist\protect)
// ====================================
BOOLEAN IsInProtectedDownloadPath(_In_ PUNICODE_STRING FilePath)
{
    UNICODE_STRING protectedPath;
    ULONG filePathLen;
    ULONG protectedPathLen;

    // 固定保护路径：C:\whitelist\protect
    // 注意：路径可能是设备路径格式（如 \Device\HarddiskVolume4\whitelist\protect\...）
    RtlInitUnicodeString(&protectedPath, L"whitelist\\protect");

    if (FilePath == NULL || FilePath->Buffer == NULL || FilePath->Length == 0) {
        return FALSE;
    }

    filePathLen = FilePath->Length / sizeof(WCHAR);
    protectedPathLen = protectedPath.Length / sizeof(WCHAR);

    // 检查文件路径是否包含受保护路径（使用路径后缀匹配）
    // 查找 "whitelist\protect" 在路径中的位置
    PWCHAR pathBuffer = FilePath->Buffer;
    BOOLEAN found = FALSE;

    // 遍历路径，查找 "whitelist\protect" 子串
    for (ULONG i = 0; i <= filePathLen - protectedPathLen; i++) {
        // 检查是否匹配（不区分大小写）
        BOOLEAN match = TRUE;
        for (ULONG j = 0; j < protectedPathLen; j++) {
            WCHAR fileChar = pathBuffer[i + j];
            WCHAR protectChar = protectedPath.Buffer[j];
            
            // 转换为小写进行比较（简单的不区分大小写比较）
            if (fileChar >= L'A' && fileChar <= L'Z') {
                fileChar = fileChar + (L'a' - L'A');
            }
            if (protectChar >= L'A' && protectChar <= L'Z') {
                protectChar = protectChar + (L'a' - L'A');
            }
            
            if (fileChar != protectChar) {
                match = FALSE;
                break;
            }
        }
        
        if (match) {
            // 确保匹配的是完整路径段（前后是反斜杠或路径开始/结束）
            if ((i == 0 || pathBuffer[i - 1] == L'\\') &&
                (i + protectedPathLen >= filePathLen || pathBuffer[i + protectedPathLen] == L'\\')) {
                found = TRUE;
                break;
            }
        }
    }

    return found;
}

// ====================================
// 从文件路径中提取扩展名
// ====================================
VOID ExtractExtension(
    _In_ PUNICODE_STRING FilePath,
    _Out_ PUNICODE_STRING Extension
)
{
    PWCHAR lastDot;
    PWCHAR fileName;

    // 第一次完善：添加扩展名提取功能
    RtlInitUnicodeString(Extension, NULL);

    if (FilePath == NULL || FilePath->Buffer == NULL || FilePath->Length == 0) {
        return;
    }

    // 查找最后一个点号
    fileName = FilePath->Buffer;
    lastDot = NULL;

    for (ULONG i = 0; i < FilePath->Length / sizeof(WCHAR); i++) {
        if (fileName[i] == L'.') {
            lastDot = &fileName[i];
        }
        else if (fileName[i] == L'\\' || fileName[i] == L'/') {
            // 遇到路径分隔符，重置
            lastDot = NULL;
        }
    }

    if (lastDot != NULL) {
        RtlInitUnicodeString(Extension, lastDot);
    }
}

// ====================================
// 检查文件是否受保护
// ====================================
// 第一次完善：添加 SAL 批注，与头文件声明保持一致
BOOLEAN IsProtectedFile(_In_ PUNICODE_STRING FilePath)
{
    UNICODE_STRING extension;
    BOOLEAN isProtectedByExtension = FALSE;

    // 第一次完善：改进 IsProtectedFile 使用策略配置
    if (FilePath == NULL || FilePath->Buffer == NULL || FilePath->Length == 0) {
        return FALSE;
    }

    // 检查扩展名
    ExtractExtension(FilePath, &extension);
    if (extension.Buffer != NULL && extension.Length > 0) {
        isProtectedByExtension = IsProtectedExtension(&extension);
    }

    // 如果扩展名匹配，则文件受保护
    return isProtectedByExtension;
}

// ====================================
// 从策略配置中检查扩展名
// ====================================
BOOLEAN IsProtectedExtension(
    _In_ PUNICODE_STRING Extension
)
{
    KIRQL oldIrql;
    PWCHAR token;
    PWCHAR nextToken = NULL;
    BOOLEAN isProtected = FALSE;
    WCHAR tempBuffer[256];
    UNICODE_STRING extToCompare;

    // 参数检查
    if (Extension == NULL || Extension->Buffer == NULL || Extension->Length == 0) {
        return FALSE;
    }

    // 获取策略锁
    KeAcquireSpinLock(&g_SecurityData->PolicyLock, &oldIrql);

    // 检查策略中是否配置了扩展名列表
    if (g_SecurityData->Policy.ProtectedExtensions[0] == L'\0') {
        // 如果策略为空，使用默认列表
        KeReleaseSpinLock(&g_SecurityData->PolicyLock, oldIrql);
        return IsProtectedExtensionDefault(Extension);
    }

    // 复制策略字符串（避免在持有锁时进行复杂操作）
    RtlStringCbCopyW(tempBuffer, sizeof(tempBuffer),
        g_SecurityData->Policy.ProtectedExtensions);

    KeReleaseSpinLock(&g_SecurityData->PolicyLock, oldIrql);

    // 解析扩展名列表（格式：".docx;.xlsx;.pdf"）
    token = KernelWcsTok(tempBuffer, L";", &nextToken);
    while (token != NULL) {
        // 去除前后空格
        while (*token == L' ' || *token == L'\t') {
            token++;
        }

        if (*token != L'\0') {
            RtlInitUnicodeString(&extToCompare, token);

            // 比较扩展名（不区分大小写）
            if (RtlEqualUnicodeString(Extension, &extToCompare, TRUE)) {
                isProtected = TRUE;
                break;
            }
        }

        token = KernelWcsTok(NULL, L";", &nextToken);
    }

    return isProtected;
}

// ====================================
// 默认扩展名列表（备用）
// ====================================
BOOLEAN IsProtectedExtensionDefault(
    _In_ PUNICODE_STRING Extension
)
{
    ULONG i;

    UNICODE_STRING protectedExtensions[] = {
        RTL_CONSTANT_STRING(L".docx"),
        RTL_CONSTANT_STRING(L".xlsx"),
        RTL_CONSTANT_STRING(L".pptx"),
        RTL_CONSTANT_STRING(L".pdf"),
        RTL_CONSTANT_STRING(L".doc"),
        RTL_CONSTANT_STRING(L".xls"),
        RTL_CONSTANT_STRING(L".ppt"),
        RTL_CONSTANT_STRING(L".txt")
    };

    for (i = 0; i < sizeof(protectedExtensions) / sizeof(UNICODE_STRING); i++) {
        if (RtlEqualUnicodeString(Extension, &protectedExtensions[i], TRUE)) {
            return TRUE;
        }
    }

    return FALSE;
}

