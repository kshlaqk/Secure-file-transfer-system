// Callbacks.c
#include "SecureFilter.h"

extern PSECURITY_DATA g_SecurityData;

// ====================================
// 文件创建前回调（核心功能）
// ====================================
FLT_PREOP_CALLBACK_STATUS PreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
)
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    PEPROCESS process;
    WCHAR processName[MAX_PROCESS_NAME];
    BOOLEAN isProtected = FALSE;
    BOOLEAN isAuthorized = FALSE;

    // 第一次完善：初始化 CompletionContext 以消除代码分析警告
    if (CompletionContext != NULL) {
        *CompletionContext = NULL;
    }

    // 第一次完善：标记未使用的参数以消除编译器警告
    UNREFERENCED_PARAMETER(FltObjects);

    // 1. 获取文件名信息
    status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &nameInfo
    );

    if (!NT_SUCCESS(status)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 2. 检查是否为受保护文件
    isProtected = IsProtectedFile(&nameInfo->Name);

    if (!isProtected) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 3. 获取进程信息
    process = IoThreadToProcess(Data->Thread);
    status = GetProcessName(process, processName, MAX_PROCESS_NAME);

    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 4. 检查进程是否在白名单
    isAuthorized = IsProcessInWhitelist(processName);

    if (!isAuthorized) {
        // 拦截！
        DbgPrint("[+]SecureFilter: Blocked %ws from accessing %wZ\n",
            processName, &nameInfo->Name);

        // 记录日志
        LogEvent(processName, nameInfo->Name.Buffer, 1); // 1=拒绝
        // 第一次完善：添加统计信息更新
        InterlockedIncrement((PLONG)&g_SecurityData->DeniedAccess);

        // 返回拒绝访问
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;

        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_COMPLETE;
    }

    // 允许访问
    DbgPrint("[+]SecureFilter: Allowed %ws to access %wZ\n",
        processName, &nameInfo->Name);

    LogEvent(processName, nameInfo->Name.Buffer, 0); // 0=允许
    // 第一次完善：添加统计信息更新
    InterlockedIncrement((PLONG)&g_SecurityData->AllowedAccess);
    InterlockedIncrement((PLONG)&g_SecurityData->ProtectedFilesAccessed);

    FltReleaseFileNameInformation(nameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

// ====================================
// 文件创建后回调
// ====================================
FLT_POSTOP_CALLBACK_STATUS PostCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    // 第一次完善：PostCreateCallback 实现
    // 创建操作后可以更新统计信息
    if (NT_SUCCESS(Data->IoStatus.Status)) {
        InterlockedIncrement((PLONG)&g_SecurityData->TotalInterceptions);
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}

// ====================================
// 文件读取前回调
// ====================================
FLT_PREOP_CALLBACK_STATUS PreReadCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
)
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    PEPROCESS process;
    WCHAR processName[MAX_PROCESS_NAME];
    BOOLEAN isProtected = FALSE;
    BOOLEAN isAuthorized = FALSE;

    // 第一次完善：初始化 CompletionContext 以消除代码分析警告
    if (CompletionContext != NULL) {
        *CompletionContext = NULL;
    }
    UNREFERENCED_PARAMETER(FltObjects);

    // 第一次完善：PreReadCallback 实现
    // 1. 获取文件名信息
    status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &nameInfo
    );

    if (!NT_SUCCESS(status)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 2. 检查是否为受保护文件
    isProtected = IsProtectedFile(&nameInfo->Name);

    if (!isProtected) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 3. 获取进程信息
    process = IoThreadToProcess(Data->Thread);
    status = GetProcessName(process, processName, MAX_PROCESS_NAME);

    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 4. 检查进程是否在白名单
    isAuthorized = IsProcessInWhitelist(processName);

    if (!isAuthorized) {
        // 拦截读取操作
        DbgPrint("[+]SecureFilter: Blocked %ws from reading %wZ\n",
            processName, &nameInfo->Name);

        // 记录日志
        LogEvent(processName, nameInfo->Name.Buffer, 1); // 1=拒绝
        InterlockedIncrement((PLONG)&g_SecurityData->DeniedAccess);

        // 返回拒绝访问
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;

        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_COMPLETE;
    }

    // 允许读取
    DbgPrint("[+]SecureFilter: Allowed %ws to read %wZ\n",
        processName, &nameInfo->Name);

    LogEvent(processName, nameInfo->Name.Buffer, 0); // 0=允许
    InterlockedIncrement((PLONG)&g_SecurityData->AllowedAccess);
    InterlockedIncrement((PLONG)&g_SecurityData->ProtectedFilesAccessed);

    FltReleaseFileNameInformation(nameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

// ====================================
// 文件读取后回调
// ====================================
FLT_POSTOP_CALLBACK_STATUS PostReadCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    // 第一次完善：PostReadCallback 实现
    // 读取操作后可以更新统计信息
    if (NT_SUCCESS(Data->IoStatus.Status)) {
        InterlockedIncrement((PLONG)&g_SecurityData->TotalInterceptions);
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}

// ====================================
// 文件写入前回调
// ====================================
FLT_PREOP_CALLBACK_STATUS PreWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
)
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    PEPROCESS process;
    WCHAR processName[MAX_PROCESS_NAME];
    BOOLEAN isProtected = FALSE;
    BOOLEAN isAuthorized = FALSE;

    // 第一次完善：初始化 CompletionContext 以消除代码分析警告
    if (CompletionContext != NULL) {
        *CompletionContext = NULL;
    }
    UNREFERENCED_PARAMETER(FltObjects);

    // 第一次完善：PreWriteCallback 实现
    // 1. 获取文件名信息
    status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &nameInfo
    );

    if (!NT_SUCCESS(status)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 2. 检查是否为受保护文件
    isProtected = IsProtectedFile(&nameInfo->Name);

    if (!isProtected) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 3. 获取进程信息
    process = IoThreadToProcess(Data->Thread);
    status = GetProcessName(process, processName, MAX_PROCESS_NAME);

    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 4. 检查进程是否在白名单
    isAuthorized = IsProcessInWhitelist(processName);

    if (!isAuthorized) {
        // 拦截写入操作
        DbgPrint("[+]SecureFilter: Blocked %ws from writing %wZ\n",
            processName, &nameInfo->Name);

        // 记录日志
        LogEvent(processName, nameInfo->Name.Buffer, 1); // 1=拒绝
        InterlockedIncrement((PLONG)&g_SecurityData->DeniedAccess);

        // 返回拒绝访问
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;

        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_COMPLETE;
    }

    // 允许写入
    DbgPrint("[+]SecureFilter: Allowed %ws to write %wZ\n",
        processName, &nameInfo->Name);

    LogEvent(processName, nameInfo->Name.Buffer, 0); // 0=允许
    InterlockedIncrement((PLONG)&g_SecurityData->AllowedAccess);
    InterlockedIncrement((PLONG)&g_SecurityData->ProtectedFilesAccessed);

    FltReleaseFileNameInformation(nameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
// ====================================
// 文件写入后回调
// ====================================
FLT_POSTOP_CALLBACK_STATUS PostWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    // 第一次完善：PostWriteCallback 实现
    // 写入操作后可以更新统计信息
    if (NT_SUCCESS(Data->IoStatus.Status)) {
        InterlockedIncrement((PLONG)&g_SecurityData->TotalInterceptions);
    }
    
    return FLT_POSTOP_FINISHED_PROCESSING;
}

// ====================================
// 文件重命名/移动前回调
// ====================================
FLT_PREOP_CALLBACK_STATUS PreSetInformationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID* CompletionContext
)
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    PEPROCESS process;
    WCHAR processName[MAX_PROCESS_NAME];
    BOOLEAN isProtected = FALSE;
    BOOLEAN isAuthorized = FALSE;
    FILE_INFORMATION_CLASS fileInfoClass;
    PFILE_RENAME_INFORMATION renameInfo = NULL;
    UNICODE_STRING newFileName;

    // 初始化 CompletionContext
    if (CompletionContext != NULL) {
        *CompletionContext = NULL;
    }
    UNREFERENCED_PARAMETER(FltObjects);

    // 1. 获取文件信息类
    fileInfoClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;

    // 2. 只处理文件重命名操作
    if (fileInfoClass != FileRenameInformation) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 3. 获取原始文件名信息
    status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &nameInfo
    );

    if (!NT_SUCCESS(status)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 4. 获取重命名信息
    renameInfo = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
    if (renameInfo == NULL) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 5. 构建新文件路径（UNICODE_STRING）
    newFileName.Buffer = renameInfo->FileName;
    newFileName.Length = (USHORT)renameInfo->FileNameLength;
    newFileName.MaximumLength = (USHORT)renameInfo->FileNameLength;

    // 6. 特殊检查：如果源文件在 C:\whitelist\protect 目录下，禁止移动到其他目录
    BOOLEAN isSourceInProtectedPath = IsInProtectedDownloadPath(&nameInfo->Name);
    if (isSourceInProtectedPath) {
        // 源文件在保护目录下，检查目标路径
        UNICODE_STRING tempNewPath;
        WCHAR fullNewPath[MAX_PATH_LENGTH];
        BOOLEAN isDestInProtectedPath = FALSE;
        
        // 构建目标文件的完整路径
        if (newFileName.Buffer[0] == L'\\' || 
            (newFileName.Length >= 2 && newFileName.Buffer[1] == L':')) {
            // 绝对路径
            RtlCopyMemory(fullNewPath, newFileName.Buffer, 
                min(newFileName.Length, MAX_PATH_LENGTH * sizeof(WCHAR)));
            fullNewPath[newFileName.Length / sizeof(WCHAR)] = L'\0';
            RtlInitUnicodeString(&tempNewPath, fullNewPath);
        } else {
            // 相对路径，需要与原始文件目录组合
            PWCHAR lastSlash = NULL;
            PWCHAR current = nameInfo->Name.Buffer;
            ULONG i;
            
            for (i = 0; i < nameInfo->Name.Length / sizeof(WCHAR); i++) {
                if (current[i] == L'\\') {
                    lastSlash = &current[i];
                }
            }
            
            if (lastSlash != NULL) {
                ULONG dirLen = (ULONG)(lastSlash - nameInfo->Name.Buffer + 1);
                RtlCopyMemory(fullNewPath, nameInfo->Name.Buffer, dirLen * sizeof(WCHAR));
                RtlCopyMemory(fullNewPath + dirLen, newFileName.Buffer, 
                    min(newFileName.Length, (MAX_PATH_LENGTH - dirLen) * sizeof(WCHAR)));
                fullNewPath[(dirLen + newFileName.Length / sizeof(WCHAR))] = L'\0';
                RtlInitUnicodeString(&tempNewPath, fullNewPath);
            } else {
                // 无法构建路径，使用新文件名
                RtlInitUnicodeString(&tempNewPath, newFileName.Buffer);
            }
        }
        
        // 检查目标路径是否也在保护目录下
        isDestInProtectedPath = IsInProtectedDownloadPath(&tempNewPath);
        
        // 如果目标路径不在保护目录下，拦截（无论是否在白名单）
        if (!isDestInProtectedPath) {
            // 获取进程信息用于日志
            process = IoThreadToProcess(Data->Thread);
            status = GetProcessName(process, processName, MAX_PROCESS_NAME);
            
            if (NT_SUCCESS(status)) {
                DbgPrint("[+]SecureFilter: Blocked %ws from moving file out of protected path: %wZ -> %wZ\n",
                    processName, &nameInfo->Name, &tempNewPath);
                LogEvent(processName, nameInfo->Name.Buffer, 1); // 1=拒绝
            } else {
                DbgPrint("[+]SecureFilter: Blocked moving file out of protected path: %wZ\n",
                    &nameInfo->Name);
            }
            
            InterlockedIncrement((PLONG)&g_SecurityData->DeniedAccess);
            
            // 返回拒绝访问
            Data->IoStatus.Status = STATUS_ACCESS_DENIED;
            Data->IoStatus.Information = 0;
            
            FltReleaseFileNameInformation(nameInfo);
            return FLT_PREOP_COMPLETE;
        }
        
        // 如果目标路径也在保护目录下，允许重命名（在同一目录内重命名或移动）
        // 继续后续的检查流程
    }

    // 7. 检查原始文件或新文件是否受保护（扩展名或路径保护）
    isProtected = IsProtectedFile(&nameInfo->Name);
    
    // 如果原始文件不受保护，检查新文件路径
    if (!isProtected && newFileName.Buffer != NULL && newFileName.Length > 0) {
        // 构建完整的新文件路径
        // 注意：renameInfo->FileName 可能是相对路径或完整路径
        // 这里简化处理，假设是相对路径，需要与原始文件目录组合
        // 为了简化，我们直接检查新文件名是否包含受保护的扩展名
        UNICODE_STRING tempNewPath;
        WCHAR fullNewPath[MAX_PATH_LENGTH];
        
        // 尝试构建完整路径
        // 如果新路径是绝对路径（以 \ 或 X: 开头），直接使用
        // 否则需要与原始文件目录组合
        if (newFileName.Buffer[0] == L'\\' || 
            (newFileName.Length >= 2 && newFileName.Buffer[1] == L':')) {
            // 绝对路径
            RtlCopyMemory(fullNewPath, newFileName.Buffer, 
                min(newFileName.Length, MAX_PATH_LENGTH * sizeof(WCHAR)));
            fullNewPath[newFileName.Length / sizeof(WCHAR)] = L'\0';
            RtlInitUnicodeString(&tempNewPath, fullNewPath);
        } else {
            // 相对路径，需要与原始文件目录组合
            // 简化处理：提取原始文件目录，然后拼接新文件名
            // 手动查找最后一个反斜杠（内核模式，不能使用 wcsrchr）
            PWCHAR lastSlash = NULL;
            PWCHAR current = nameInfo->Name.Buffer;
            ULONG i;
            
            for (i = 0; i < nameInfo->Name.Length / sizeof(WCHAR); i++) {
                if (current[i] == L'\\') {
                    lastSlash = &current[i];
                }
            }
            
            if (lastSlash != NULL) {
                ULONG dirLen = (ULONG)(lastSlash - nameInfo->Name.Buffer + 1);
                RtlCopyMemory(fullNewPath, nameInfo->Name.Buffer, dirLen * sizeof(WCHAR));
                RtlCopyMemory(fullNewPath + dirLen, newFileName.Buffer, 
                    min(newFileName.Length, (MAX_PATH_LENGTH - dirLen) * sizeof(WCHAR)));
                fullNewPath[(dirLen + newFileName.Length / sizeof(WCHAR))] = L'\0';
                RtlInitUnicodeString(&tempNewPath, fullNewPath);
            } else {
                // 无法构建路径，使用新文件名
                RtlInitUnicodeString(&tempNewPath, newFileName.Buffer);
            }
        }
        
        isProtected = IsProtectedFile(&tempNewPath);
    }

    // 如果文件不受保护，直接允许
    if (!isProtected) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 8. 获取进程信息
    process = IoThreadToProcess(Data->Thread);
    status = GetProcessName(process, processName, MAX_PROCESS_NAME);

    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // 9. 检查进程是否在白名单
    isAuthorized = IsProcessInWhitelist(processName);

    if (!isAuthorized) {
        // 拦截重命名/移动操作
        DbgPrint("[+]SecureFilter: Blocked %ws from renaming/moving %wZ\n",
            processName, &nameInfo->Name);

        // 记录日志
        LogEvent(processName, nameInfo->Name.Buffer, 1); // 1=拒绝
        InterlockedIncrement((PLONG)&g_SecurityData->DeniedAccess);

        // 返回拒绝访问
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;

        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_COMPLETE;
    }

    // 允许重命名/移动
    DbgPrint("[+]SecureFilter: Allowed %ws to rename/move %wZ\n",
        processName, &nameInfo->Name);

    LogEvent(processName, nameInfo->Name.Buffer, 0); // 0=允许
    InterlockedIncrement((PLONG)&g_SecurityData->AllowedAccess);
    InterlockedIncrement((PLONG)&g_SecurityData->ProtectedFilesAccessed);

    FltReleaseFileNameInformation(nameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

// ====================================
// 文件重命名/移动后回调
// ====================================
FLT_POSTOP_CALLBACK_STATUS PostSetInformationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    // 重命名/移动操作后可以更新统计信息
    if (NT_SUCCESS(Data->IoStatus.Status)) {
        InterlockedIncrement((PLONG)&g_SecurityData->TotalInterceptions);
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}