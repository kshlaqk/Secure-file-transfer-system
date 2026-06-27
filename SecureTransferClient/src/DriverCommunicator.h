#ifndef DRIVERCOMMUNICATOR_H
#define DRIVERCOMMUNICATOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "DriverTypes.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <fltuser.h>
#endif

// 前向声明
struct LogEntry;
struct FilterStatistics;

/**
 * @brief 驱动通信封装类
 * 
 * 负责与Minifilter驱动进行通信，包括：
 * - 连接/断开驱动通信端口
 * - 策略更新（用于策略同步）
 * - 日志查询（可选功能）
 * - 统计信息查询（可选功能）
 */
class DriverCommunicator : public QObject
{
    Q_OBJECT

public:
    explicit DriverCommunicator(QObject *parent = nullptr);
    ~DriverCommunicator();

    // 连接管理
    bool connectToDriver();
    void disconnectFromDriver();
    bool isConnected() const;

    // 策略更新（用于策略同步服务）
    bool updatePolicy(const QString& extensions, bool enableEncryption);
    
    // 获取日志
    QList<LogEntry> getLogs();
    
    // 白名单管理
    bool addWhitelistItem(const QString& processPath);
    bool removeWhitelistItem(const QString& processPath);
    QStringList getWhitelist();

signals:
    void connected();
    void disconnected();
    void error(const QString& errorString);

private:
    bool sendIoctl(quint32 ioctlCode, const void* input, quint32 inputSize,
                   void* output, quint32 outputSize, quint32* bytesReturned);
    
    QString convertToWideChar(const QString& str, wchar_t* buffer, size_t bufferSize);
    
    // 日志记录函数
    void writeIoctlLog(quint32 ioctlCode, const QByteArray& requestData);
    QString getIoctlName(quint32 ioctlCode);
    QString bytesToHex(const QByteArray& data, int maxBytes = 256);
    QString getLogFilePath();
    
#ifdef Q_OS_WIN
    HANDLE m_portHandle;
#else
    void* m_portHandle;
#endif
    bool m_connected;
    static const QString PORT_NAME;
};

#endif // DRIVERCOMMUNICATOR_H




