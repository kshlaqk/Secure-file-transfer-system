#include "MachineIdHelper.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <intrin.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

QString MachineIdHelper::getMachineId()
{
#ifdef Q_OS_WIN
    return getWindowsMachineId();
#else
    // 其他平台可以扩展
    return QString();
#endif
}

#ifdef Q_OS_WIN
QString MachineIdHelper::getWindowsMachineId()
{
    QString machineId;
    
    // 方法1: 尝试获取CPU序列号
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    if (cpuInfo[3] != 0) {
        machineId += QString::number(cpuInfo[3], 16);
    }
    
    // 方法2: 获取MAC地址（更稳定）
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD dwBufLen = sizeof(adapterInfo);
    DWORD dwStatus = GetAdaptersInfo(adapterInfo, &dwBufLen);
    
    if (dwStatus == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
        if (pAdapterInfo != nullptr) {
            QString macAddress;
            for (UINT i = 0; i < pAdapterInfo->AddressLength; i++) {
                if (i > 0) macAddress += ":";
                macAddress += QString::number(pAdapterInfo->Address[i], 16).rightJustified(2, '0');
            }
            if (!macAddress.isEmpty()) {
                machineId += macAddress;
            }
        }
    }
    
    // 方法3: 获取计算机名（作为补充）
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(computerName, &size)) {
        QString name = QString::fromWCharArray(computerName);
        machineId += name;
    }
    
    // 如果都获取失败，使用一个基于系统信息的哈希值
    if (machineId.isEmpty()) {
        // 使用系统目录路径的哈希
        wchar_t systemDir[MAX_PATH];
        if (GetSystemDirectoryW(systemDir, MAX_PATH)) {
            QString sysPath = QString::fromWCharArray(systemDir);
            machineId = QString::number(qHash(sysPath), 16);
        }
    }
    
    // 转换为大写并移除特殊字符，确保唯一性
    machineId = machineId.toUpper().replace(":", "").replace("-", "");
    
    if (machineId.isEmpty()) {
        qWarning() << "无法获取机器码，使用默认值";
        machineId = "UNKNOWN_MACHINE";
    }
    
    return machineId;
}
#endif
