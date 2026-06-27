#include "DriverCommunicator.h"
#include <QDebug>
#include <QByteArray>
#include <QMutexLocker>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <fltuser.h>
#include <winioctl.h>
#endif

const QString DriverCommunicator::PORT_NAME = "\\SecureFilterPort";

DriverCommunicator::DriverCommunicator(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_WIN
    , m_portHandle(INVALID_HANDLE_VALUE)
#else
    , m_portHandle(nullptr)
#endif
    , m_connected(false)
{
}

DriverCommunicator::~DriverCommunicator()
{
    disconnectFromDriver();
}

bool DriverCommunicator::connectToDriver()
{
    if (m_connected) {
        return true;
    }

#ifdef Q_OS_WIN
    // 连接 Minifilter 通信端口
    HRESULT hr = FilterConnectCommunicationPort(
        reinterpret_cast<LPCWSTR>(PORT_NAME.utf16()),  // 端口名称
        0,                                              // 选项
        NULL,                                           // 连接上下文
        0,                                              // 上下文大小
        NULL,                                           // 安全属性
        &m_portHandle                                   // 返回的端口句柄
    );

    if (SUCCEEDED(hr) && m_portHandle != INVALID_HANDLE_VALUE) {
        m_connected = true;
        emit connected();
        qInfo() << "Connected to driver communication port";
        return true;
    } else {
        m_portHandle = INVALID_HANDLE_VALUE;
        QString error = QString("Failed to connect to driver port. Error: 0x%1")
                       .arg(hr, 8, 16, QChar('0'));
        qWarning() << error;
        emit this->error(error);
        return false;
    }
#else
    emit error("Driver communication is only supported on Windows");
    return false;
#endif
}

void DriverCommunicator::disconnectFromDriver()
{
    if (!m_connected) {
        return;
    }

#ifdef Q_OS_WIN
    if (m_portHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_portHandle);
        m_portHandle = INVALID_HANDLE_VALUE;
    }
#endif

    m_connected = false;
    emit disconnected();
    qInfo() << "Disconnected from driver communication port";
}

bool DriverCommunicator::isConnected() const
{
    return m_connected;
}

bool DriverCommunicator::updatePolicy(const QString& extensions, 
                                      bool enableEncryption)
{
    if (!m_connected) {
        emit error("Driver not connected");
        return false;
    }

#ifdef Q_OS_WIN
    // 构建策略配置结构
    PolicyConfig policyConfig;
    memset(&policyConfig, 0, sizeof(policyConfig));

    // 转换扩展名字符串（QString -> wchar_t array）
    // QString内部使用UTF-16，可以直接转换为wchar_t（在Windows上）
    int extMaxLen = (sizeof(policyConfig.ProtectedExtensions) / sizeof(wchar_t)) - 1;
    int extLen = qMin(extensions.length(), extMaxLen);
    if (extLen > 0) {
        const wchar_t* extData = reinterpret_cast<const wchar_t*>(extensions.utf16());
        for (int i = 0; i < extLen; ++i) {
            policyConfig.ProtectedExtensions[i] = extData[i];
        }
    }
    policyConfig.ProtectedExtensions[extLen] = L'\0';

    policyConfig.EnableEncryption = enableEncryption ? 1 : 0;

    // 构建消息
    SecureFilterMessageHeader header;
    header.MessageId = IOCTL_UPDATE_POLICY;
    header.Status = 0;

    // 准备请求缓冲区
    quint32 requestSize = sizeof(SecureFilterMessageHeader) + sizeof(PolicyConfig);
    QByteArray requestBuffer(requestSize, 0);
    memcpy(requestBuffer.data(), &header, sizeof(header));
    memcpy(requestBuffer.data() + sizeof(header), &policyConfig, sizeof(policyConfig));

    // 准备响应缓冲区
    QByteArray responseBuffer(4096, 0);
    DWORD bytesReturnedWin = 0;

    // 发送消息
    HRESULT hr = FilterSendMessage(
        m_portHandle,
        requestBuffer.data(),
        static_cast<DWORD>(requestSize),
        responseBuffer.data(),
        static_cast<DWORD>(responseBuffer.size()),
        &bytesReturnedWin
    );
    
    quint32 bytesReturned = static_cast<quint32>(bytesReturnedWin);

    if (SUCCEEDED(hr)) {
        SecureFilterMessageHeader* respHeader = 
            reinterpret_cast<SecureFilterMessageHeader*>(responseBuffer.data());
        if (respHeader->Status == 0) {  // STATUS_SUCCESS
            qInfo() << "Policy updated successfully";
            return true;
        } else {
            QString error = QString("Policy update failed. Status: 0x%1")
                           .arg(respHeader->Status, 8, 16, QChar('0'));
            qWarning() << error;
            emit this->error(error);
            return false;
        }
    } else {
        QString error = QString("Failed to send policy update. Error: 0x%1")
                       .arg(hr, 8, 16, QChar('0'));
        qWarning() << error;
        emit this->error(error);
        return false;
    }
#else
    emit error("Policy update is only supported on Windows");
    return false;
#endif
}

bool DriverCommunicator::sendIoctl(quint32 ioctlCode, 
                                    const void* input, 
                                    quint32 inputSize,
                                    void* output, 
                                    quint32 outputSize, 
                                    quint32* bytesReturned)
{
#ifdef Q_OS_WIN
    if (!m_connected || m_portHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    // 构建消息头
    SecureFilterMessageHeader header;
    header.MessageId = ioctlCode;
    header.Status = 0;

    // 准备请求缓冲区
    quint32 requestSize = sizeof(header) + inputSize;
    QByteArray requestBuffer(requestSize, 0);
    memcpy(requestBuffer.data(), &header, sizeof(header));
    if (input && inputSize > 0) {
        memcpy(requestBuffer.data() + sizeof(header), input, inputSize);
    }

    // 准备响应缓冲区
    QByteArray responseBuffer(outputSize > 0 ? outputSize : 4096, 0);
    DWORD bytesRetWin = 0;

    // ===== 在 FilterSendMessage 调用之前记录日志 =====
    writeIoctlLog(ioctlCode, requestBuffer);

    // 发送消息
    HRESULT hr = FilterSendMessage(
        m_portHandle,
        requestBuffer.data(),
        static_cast<DWORD>(requestSize),
        responseBuffer.data(),
        static_cast<DWORD>(responseBuffer.size()),
        &bytesRetWin
    );
    
    quint32 bytesRet = static_cast<quint32>(bytesRetWin);

    if (SUCCEEDED(hr)) {
        SecureFilterMessageHeader* respHeader = 
            reinterpret_cast<SecureFilterMessageHeader*>(responseBuffer.data());
        
        if (bytesReturned) {
            *bytesReturned = bytesRet > sizeof(header) ? bytesRet - sizeof(header) : 0;
        }

        if (output && outputSize > 0 && bytesRet > sizeof(header)) {
            memcpy(output, responseBuffer.data() + sizeof(header), 
                   qMin(outputSize, bytesRet - (quint32)sizeof(header)));
        }

        return respHeader->Status == 0;  // STATUS_SUCCESS
    }

    return false;
#else
    Q_UNUSED(ioctlCode)
    Q_UNUSED(input)
    Q_UNUSED(inputSize)
    Q_UNUSED(output)
    Q_UNUSED(outputSize)
    Q_UNUSED(bytesReturned)
    return false;
#endif
}

// 获取日志文件路径
QString DriverCommunicator::getLogFilePath()
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    return logDir + "/DriverIoctl.log";
}// 获取 IOCTL 名称
QString DriverCommunicator::getIoctlName(quint32 ioctlCode)
{
    switch (ioctlCode) {
        case IOCTL_ADD_WHITELIST: return "IOCTL_ADD_WHITELIST";
        case IOCTL_REMOVE_WHITELIST: return "IOCTL_REMOVE_WHITELIST";
        case IOCTL_GET_LOGS: return "IOCTL_GET_LOGS";
        case IOCTL_UPDATE_POLICY: return "IOCTL_UPDATE_POLICY";
        case IOCTL_GET_WHITELIST: return "IOCTL_GET_WHITELIST";
        case IOCTL_GET_STATISTICS: return "IOCTL_GET_STATISTICS";
        case IOCTL_CLEAR_LOGS: return "IOCTL_CLEAR_LOGS";
        default: return QString("UNKNOWN_IOCTL(0x%1)").arg(ioctlCode, 8, 16, QChar('0'));
    }
}// 将字节数组转换为十六进制字符串
QString DriverCommunicator::bytesToHex(const QByteArray& data, int maxBytes)
{
    QString hex;
    int len = qMin(data.size(), maxBytes);
    for (int i = 0; i < len; i++) {
        hex += QString("%1 ").arg((quint8)data[i], 2, 16, QChar('0')).toUpper();
        if ((i + 1) % 16 == 0) {
            hex += "\n                              ";
        }
    }
    if (data.size() > maxBytes) {
        hex += QString("... (truncated, total %1 bytes)").arg(data.size());
    }
    return hex.trimmed();
}

// 写入 IOCTL 日志（在 FilterSendMessage 调用之前）
void DriverCommunicator::writeIoctlLog(quint32 ioctlCode, const QByteArray& requestData)
{
    QString logPath = getLogFilePath();
    QFile logFile(logPath);
    
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open log file:" << logPath;
        return;
    }
    
    QTextStream stream(&logFile);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString ioctlName = getIoctlName(ioctlCode);
    
    stream << "========================================\n";
    stream << QString("Time: %1\n").arg(timestamp);
    stream << QString("IOCTL: %1 (0x%2)\n")
              .arg(ioctlName)
              .arg(ioctlCode, 8, 16, QChar('0')).toUpper();
    stream << QString("Request Size: %1 bytes\n").arg(requestData.size());
    
    // 如果是策略更新，解析并显示详细信息
    if (ioctlCode == IOCTL_UPDATE_POLICY && requestData.size() >= sizeof(SecureFilterMessageHeader)) {
        SecureFilterMessageHeader* header = (SecureFilterMessageHeader*)requestData.data();
        if (requestData.size() >= sizeof(SecureFilterMessageHeader) + sizeof(PolicyConfig)) {
            PolicyConfig* policy = (PolicyConfig*)(requestData.data() + sizeof(SecureFilterMessageHeader));
            stream << "\n--- Policy Details ---\n";
            stream << QString("Protected Extensions: %1\n")
                      .arg(QString::fromWCharArray(policy->ProtectedExtensions));
            stream << QString("Enable Encryption: %1\n").arg(policy->EnableEncryption);
        }
    }
    
    stream << "\n--- Request Data (Hex) ---\n";
    stream << bytesToHex(requestData) << "\n";
    stream << "========================================\n\n";
    
    logFile.close();
}

// 获取日志
QList<LogEntry> DriverCommunicator::getLogs()
{
    QList<LogEntry> logs;
    
    if (!m_connected) {
        emit error("Driver not connected");
        return logs;
    }

#ifdef Q_OS_WIN
    // 先尝试获取日志大小
    quint32 bytesReturned = 0;
    QByteArray tempBuffer(sizeof(LogEntry), 0);
    
    // 第一次调用，获取需要的大小
    sendIoctl(IOCTL_GET_LOGS, nullptr, 0, tempBuffer.data(), tempBuffer.size(), &bytesReturned);
    
    // 如果返回 STATUS_BUFFER_TOO_SMALL，bytesReturned 会包含需要的大小
    // 否则直接使用返回的数据
    quint32 bufferSize = bytesReturned > 0 ? bytesReturned : (MAX_LOG_ENTRIES * sizeof(LogEntry));
    QByteArray logBuffer(bufferSize, 0);
    
    // 再次调用获取实际日志
    if (sendIoctl(IOCTL_GET_LOGS, nullptr, 0, logBuffer.data(), logBuffer.size(), &bytesReturned)) {
        // 解析日志条目
        quint32 entryCount = bytesReturned / sizeof(LogEntry);
        for (quint32 i = 0; i < entryCount; ++i) {
            LogEntry* entry = reinterpret_cast<LogEntry*>(logBuffer.data() + i * sizeof(LogEntry));
            LogEntry logEntry;
            logEntry.Timestamp = entry->Timestamp;
            memcpy(logEntry.ProcessName, entry->ProcessName, sizeof(entry->ProcessName));
            memcpy(logEntry.FilePath, entry->FilePath, sizeof(entry->FilePath));
            logEntry.Action = entry->Action;
            logs.append(logEntry);
        }
        
        qInfo() << "Retrieved" << logs.size() << "log entries from driver";
    } else {
        qWarning() << "Failed to retrieve logs from driver";
        emit error("Failed to retrieve logs from driver");
    }
#else
    emit error("Log retrieval is only supported on Windows");
#endif
    
    return logs;
}

// 添加白名单项
bool DriverCommunicator::addWhitelistItem(const QString& processPath)
{
    if (!m_connected) {
        emit error("Driver not connected");
        return false;
    }

#ifdef Q_OS_WIN
    int pathLen = processPath.length();
    QByteArray pathBuffer((pathLen + 1) * sizeof(wchar_t), 0);
    wchar_t* wPath = reinterpret_cast<wchar_t*>(pathBuffer.data());
    const wchar_t* srcPath = reinterpret_cast<const wchar_t*>(processPath.utf16());
    for (int i = 0; i <= pathLen; ++i) {
        wPath[i] = srcPath[i];
    }
    
    quint32 bytesReturned = 0;
    bool success = sendIoctl(IOCTL_ADD_WHITELIST, 
                              pathBuffer.data(), 
                              static_cast<quint32>(pathBuffer.size()),
                              nullptr, 
                              0, 
                              &bytesReturned);
    
    if (success) {
        qInfo() << "Added to whitelist:" << processPath;
    } else {
        qWarning() << "Failed to add to whitelist:" << processPath;
        emit error("Failed to add to whitelist: " + processPath);
    }
    
    return success;
#else
    emit error("Whitelist management is only supported on Windows");
    return false;
#endif
}

// 移除白名单项
bool DriverCommunicator::removeWhitelistItem(const QString& processPath)
{
    if (!m_connected) {
        emit error("Driver not connected");
        return false;
    }
#ifdef Q_OS_WIN
    int pathLen = processPath.length();
    QByteArray pathBuffer((pathLen + 1) * sizeof(wchar_t), 0);
    wchar_t* wPath = reinterpret_cast<wchar_t*>(pathBuffer.data());
    const wchar_t* srcPath = reinterpret_cast<const wchar_t*>(processPath.utf16());
    for (int i = 0; i <= pathLen; ++i) {
        wPath[i] = srcPath[i];
    }
    
    quint32 bytesReturned = 0;
    bool success = sendIoctl(IOCTL_REMOVE_WHITELIST, 
                              pathBuffer.data(), 
                              static_cast<quint32>(pathBuffer.size()),
                              nullptr, 
                              0, 
                              &bytesReturned);
    
    if (success) {
        qInfo() << "Removed from whitelist:" << processPath;
    } else {
        qWarning() << "Failed to remove from whitelist:" << processPath;
        emit error("Failed to remove from whitelist: " + processPath);
    }
    
    return success;
#else
    emit error("Whitelist management is only supported on Windows");
    return false;
#endif
}

// 获取白名单列表
QStringList DriverCommunicator::getWhitelist()
{
    QStringList whitelist;
    
    if (!m_connected) {
        emit error("Driver not connected");
        return whitelist;
    }
    
#ifdef Q_OS_WIN
    // 先尝试获取白名单大小
    quint32 bytesReturned = 0;
    QByteArray tempBuffer(1024, 0);  // 初始缓冲区
    
    // 第一次调用，获取需要的大小
    sendIoctl(IOCTL_GET_WHITELIST, nullptr, 0, tempBuffer.data(), tempBuffer.size(), &bytesReturned);
    
    // 如果返回 STATUS_BUFFER_TOO_SMALL，bytesReturned 会包含需要的大小
    // 否则直接使用返回的数据
    quint32 bufferSize = bytesReturned > tempBuffer.size() ? bytesReturned : (1024 * 10);  // 最多10KB
    QByteArray whitelistBuffer(bufferSize, 0);
    
    // 再次调用获取实际白名单
    if (sendIoctl(IOCTL_GET_WHITELIST, nullptr, 0, whitelistBuffer.data(), whitelistBuffer.size(), &bytesReturned)) {
        // 解析白名单：驱动返回的是多个以null结尾的宽字符串
        const wchar_t* current = reinterpret_cast<const wchar_t*>(whitelistBuffer.data());
        const wchar_t* end = reinterpret_cast<const wchar_t*>(whitelistBuffer.data() + bytesReturned);
        
        while (current < end && *current != L'\0') {
            QString path = QString::fromWCharArray(current);
            if (!path.isEmpty()) {
                whitelist.append(path);
            }
            // 移动到下一个字符串（跳过当前字符串和null终止符）
            current += path.length() + 1;
            
            // 防止越界
            if (current >= end) {
                break;
            }
        }
        
        qInfo() << "Retrieved" << whitelist.size() << "whitelist items from driver";
    } else {
        qWarning() << "Failed to retrieve whitelist from driver";
        emit error("Failed to retrieve whitelist from driver");
    }
#else
    emit error("Whitelist retrieval is only supported on Windows");
#endif
    
    return whitelist;
}